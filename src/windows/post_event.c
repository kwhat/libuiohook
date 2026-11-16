/* libUIOHook: Cross-platform keyboard and mouse hooking from userland.
 * Copyright (C) 2006-2026 Alexander Barker.  All Rights Reserved.
 * https://github.com/kwhat/libuiohook/
 *
 * libUIOHook is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libUIOHook is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <uiohook.h>
#include <windows.h>

#include "input_helper.h"
#include "logger.h"
#include "wheel_helper.h"

// Some buggy versions of MinGW and MSys do not include these constants in winuser.h.
#ifndef MAPVK_VK_TO_VSC
#define MAPVK_VK_TO_VSC         0
#define MAPVK_VSC_TO_VK         1
#define MAPVK_VK_TO_CHAR        2
#define MAPVK_VSC_TO_VK_EX      3
#endif
// Some buggy versions of MinGW and MSys only define this value for Windows
// versions >= 0x0600 (Windows Vista) when it should be 0x0500 (Windows 2000).
#ifndef MAPVK_VK_TO_VSC_EX
#define MAPVK_VK_TO_VSC_EX      4
#endif

#ifndef KEYEVENTF_SCANCODE
#define KEYEVENTF_EXTENDEDKEY   0x0001
#define KEYEVENTF_KEYUP         0x0002
#define KEYEVENTF_UNICODE       0x0004
#define KEYEVENTF_SCANCODE      0x0008
#endif

#ifndef KEYEVENTF_KEYDOWN
#define KEYEVENTF_KEYDOWN       0x0000
#endif

#define MAX_WINDOWS_COORD_VALUE (1 << 16)

static LONG convert_to_relative_position(int coordinate, int screen_size) {
    // See https://stackoverflow.com/a/4555214 and its comments
	int offset = (coordinate > 0 ? 1 : -1); // Negative coordinates appear when using multiple monitors
	return ((coordinate * MAX_WINDOWS_COORD_VALUE) / screen_size) + offset;
}

static int map_keyboard_event(uiohook_event * const event, INPUT * const input) {
    input->type = INPUT_KEYBOARD; // | KEYEVENTF_SCANCODE
    //input->ki.time = GetSystemTime();

    switch (event->type) {
        case EVENT_KEY_PRESSED:
            input->ki.dwFlags = KEYEVENTF_KEYDOWN;
            break;

        case EVENT_KEY_RELEASED:
            input->ki.dwFlags = KEYEVENTF_KEYUP;
            break;

        default:
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Invalid event for keyboard event mapping: %#X.\n",
                    __FUNCTION__, __LINE__, event->type);
            return UIOHOOK_FAILURE;
    }

    input->ki.wVk = (WORD) uiocode_to_vkcode(event->data.keyboard.keycode);
    if (input->ki.wVk == 0x0000) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Unable to lookup scancode: %li\n",
                __FUNCTION__, __LINE__, event->data.keyboard.keycode);
        return UIOHOOK_FAILURE;
    }

    input->ki.wScan = MapVirtualKeyW(input->ki.wVk, MAPVK_VK_TO_VSC_EX);

    if (HIBYTE(input->ki.wScan)) {
        input->ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    } else {
        switch (event->data.keyboard.keycode) {
            case VC_UP:
            case VC_DOWN:
            case VC_LEFT:
            case VC_RIGHT:
            case VC_HOME:
            case VC_END:
            case VC_PAGE_UP:
            case VC_PAGE_DOWN:
            case VC_INSERT:
            case VC_DELETE:
            case VC_KP_ENTER:
                input->ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
                break;
        }
    }

    return UIOHOOK_SUCCESS;
}

static int map_mouse_button(uint16_t button, event_type type, MOUSEINPUT *input) {
    if (type != EVENT_MOUSE_PRESSED && type != EVENT_MOUSE_RELEASED) {
        return UIOHOOK_FAILURE;
    }

    input->mouseData = 0;
    switch (button) {
        case MOUSE_NOBUTTON:
            logger(LOG_LEVEL_WARN, "%s [%u]: No button specified for mouse %s event!\n",
                    __FUNCTION__, __LINE__, type == EVENT_MOUSE_PRESSED ? "pressed" : "released");
            return UIOHOOK_FAILURE;

        case MOUSE_BUTTON1:
            input->dwFlags = type == EVENT_MOUSE_PRESSED ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
            break;

        case MOUSE_BUTTON2:
            input->dwFlags = type == EVENT_MOUSE_PRESSED ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
            break;

        case MOUSE_BUTTON3:
            input->dwFlags = type == EVENT_MOUSE_PRESSED ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
            break;

        default:
            input->dwFlags = type == EVENT_MOUSE_PRESSED ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
            if (button == MOUSE_BUTTON4) {
                input->mouseData = XBUTTON1;
            } else if (button == MOUSE_BUTTON5) {
                input->mouseData = XBUTTON2;
            } else {
                input->mouseData = button - 3;
            }
            break;
    }

    return UIOHOOK_SUCCESS;
}

static int map_mouse_event(uiohook_event * const event, INPUT * const input) {
    // FIXME implement multiple monitor support
    uint16_t screen_width  = GetSystemMetrics(SM_CXSCREEN);
    uint16_t screen_height = GetSystemMetrics(SM_CYSCREEN);

    input->type = INPUT_MOUSE;
    input->mi.mouseData = 0;
    input->mi.dwExtraInfo = 0;
    input->mi.time = 0; // GetSystemTime();

    input->mi.dx = convert_to_relative_position(event->data.mouse.x, screen_width);
    input->mi.dy = convert_to_relative_position(event->data.mouse.y, screen_height);

    switch (event->type) {
        case EVENT_MOUSE_PRESSED:
        case EVENT_MOUSE_RELEASED: {
            int status = map_mouse_button(event->data.mouse.button, event->type, &input->mi);
            if (status != UIOHOOK_SUCCESS) {
                return status;
            }

            // Move to the requested position before posting the button event.
            uiohook_event move = *event;
            move.type = EVENT_MOUSE_MOVED;

            status = hook_post_event(&move);
            if (status != UIOHOOK_SUCCESS) {
                return status;
            }
            break;
        }

        case EVENT_MOUSE_DRAGGED:
        case EVENT_MOUSE_MOVED:
            input->mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
            break;

        default:
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Invalid event for mouse event mapping: %#X.\n",
                    __FUNCTION__, __LINE__, event->type);
            return UIOHOOK_FAILURE;
    }

    return UIOHOOK_SUCCESS;
}

static int post_mouse_wheel_event(uiohook_event * const event) {
    static double remainders[2][2];
    unsigned int axis = event->data.wheel.direction == WHEEL_HORIZONTAL_DIRECTION;
    unsigned int mode = event->data.wheel.type == WHEEL_BLOCK_SCROLL;

    double remainder = remainders[mode][axis];
    int32_t rotation;
    if (!wheel_post_rotation(&event->data.wheel, WHEEL_DELTA, &remainder, &rotation)) {
        return UIOHOOK_FAILURE;
    }

    if (rotation != 0) {
        INPUT input = { 0 };
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = axis ? MOUSEEVENTF_HWHEEL : MOUSEEVENTF_WHEEL;
        input.mi.mouseData = (DWORD) rotation;

        // Send physical motion; Windows applies its current line/page preference.
        if (SendInput(1, &input, sizeof(input)) != 1) {
            return UIOHOOK_FAILURE;
        }
    }
    remainders[mode][axis] = remainder;

    return UIOHOOK_SUCCESS;
}

UIOHOOK_API int hook_post_event(uiohook_event * const event) {
    if (event->type == EVENT_MOUSE_WHEEL) {
        return post_mouse_wheel_event(event);
    }

    INPUT *input = (INPUT *) calloc(1, sizeof(INPUT))   ;
    if (input == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: failed to allocate memory: calloc!\n",
                __FUNCTION__, __LINE__);
        return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }

    int status = UIOHOOK_FAILURE;
    switch (event->type) {
        case EVENT_KEY_PRESSED:
        case EVENT_KEY_RELEASED:
            status = map_keyboard_event(event, input);
            break;

        case EVENT_MOUSE_PRESSED:
        case EVENT_MOUSE_RELEASED:
        case EVENT_MOUSE_MOVED:
        case EVENT_MOUSE_DRAGGED:
            status = map_mouse_event(event, input);
            break;

        case EVENT_KEY_TYPED:
        case EVENT_MOUSE_CLICKED:

        case EVENT_HOOK_ENABLED:
        case EVENT_HOOK_DISABLED:

        default:
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Ignoring post event: %#X.\n",
                    __FUNCTION__, __LINE__, event->type);
            status = UIOHOOK_FAILURE;
    }

    if (status == UIOHOOK_SUCCESS && !SendInput(1, input, sizeof(INPUT))) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: SendInput() failed! (%#lX)\n",
                __FUNCTION__, __LINE__, (unsigned long) GetLastError());
        status = UIOHOOK_FAILURE;
    }

    free(input);

    return status;
}
