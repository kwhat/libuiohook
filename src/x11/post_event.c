/* libUIOHook: Cross-platform keyboard and mouse hooking from userland.
 * Copyright (C) 2006-2022 Alexander Barker.  All Rights Reserved.
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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <uiohook.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>

#include "input_helper.h"
#include "logger.h"


static int post_key_event(uiohook_event * const event) {
    KeyCode keycode = scancode_to_keycode(event->data.keyboard.keycode);
    if (keycode == 0x0000) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Unable to lookup scancode: %li\n",
                __FUNCTION__, __LINE__, event->data.keyboard.keycode);
        return UIOHOOK_FAILURE;
    }

    Bool is_pressed;
    if (event->type == EVENT_KEY_PRESSED) {
        is_pressed = True;
    } else if (event->type == EVENT_KEY_RELEASED) {
        is_pressed = False;
    } else {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Invalid event for keyboard post event: %#X.\n",
                __FUNCTION__, __LINE__, event->type);
        return UIOHOOK_FAILURE;
    }

    if (XTestFakeKeyEvent(helper_disp, keycode, is_pressed, 0) != Success) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: XTestFakeKeyEvent() failed!\n",
                __FUNCTION__, __LINE__, event->type);
        return UIOHOOK_FAILURE;
    }

    return UIOHOOK_SUCCESS;
}

static int post_mouse_button_event(uiohook_event * const event) {
    XButtonEvent btn_event = {
        .serial = 0,
        .send_event = False,
        .display = helper_disp,

        .window = None,                                /* “event” window it is reported relative to */
        .root = None,                                  /* root window that the event occurred on */
        .subwindow = XDefaultRootWindow(helper_disp),  /* child window */

        .time = CurrentTime,

        .x = event->data.mouse.x,                      /* pointer x, y coordinates in event window */
        .y = event->data.mouse.y,

        .x_root = 0,                                   /* coordinates relative to root */
        .y_root = 0,

        .state = 0x00,                                 /* key or button mask */
        .same_screen = True
    };

    // Move the pointer to the specified position.
    XTestFakeMotionEvent(btn_event.display, -1, btn_event.x, btn_event.y, 0);

    int status = UIOHOOK_FAILURE;
    switch (event->type) {
        case EVENT_MOUSE_PRESSED:
            if (event->data.mouse.button < MOUSE_BUTTON1 || event->data.mouse.button > MOUSE_BUTTON5) {
                logger(LOG_LEVEL_WARN, "%s [%u]: Invalid button specified for mouse pressed event! (%u)\n",
                        __FUNCTION__, __LINE__, event->data.mouse.button);
                return UIOHOOK_FAILURE;
            }

            if (XTestFakeButtonEvent(helper_disp, event->data.mouse.button, True, 0) != 0) {
                status = UIOHOOK_SUCCESS;
            }
            break;

        case EVENT_MOUSE_RELEASED:
            if (event->data.mouse.button < MOUSE_BUTTON1 || event->data.mouse.button > MOUSE_BUTTON5) {
                logger(LOG_LEVEL_WARN, "%s [%u]: Invalid button specified for mouse released event! (%u)\n",
                        __FUNCTION__, __LINE__, event->data.mouse.button);
                return UIOHOOK_FAILURE;
            }

            if (XTestFakeButtonEvent(helper_disp, event->data.mouse.button, False, 0) != 0) {
                status = UIOHOOK_SUCCESS;
            }
            break;

        default:
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Invalid mouse button event: %#X.\n",
                    __FUNCTION__, __LINE__, event->type);
            status = UIOHOOK_FAILURE;
    }

    return status;
}

static int post_mouse_wheel_event(uiohook_event * const event) {
    int status = UIOHOOK_FAILURE;

    XButtonEvent btn_event = {
        .serial = 0,
        .send_event = False,
        .display = helper_disp,

        .window = None,                                /* “event” window it is reported relative to */
        .root = None,                                  /* root window that the event occurred on */
        .subwindow = XDefaultRootWindow(helper_disp),  /* child window */

        .time = CurrentTime,

        .x = event->data.wheel.x,                      /* pointer x, y coordinates in event window */
        .y = event->data.wheel.y,

        .x_root = 0,                                   /* coordinates relative to root */
        .y_root = 0,

        .state = 0x00,                                 /* key or button mask */
        .same_screen = True
    };

    // Wheel events should be the same as click events on X11.
    // type, amount and rotation
    unsigned int button = button_map_lookup(event->data.wheel.rotation < 0 ? WheelUp : WheelDown);

    if (XTestFakeButtonEvent(helper_disp, button, True, 0) != 0) {
        status = UIOHOOK_SUCCESS;
    }

    if (status == UIOHOOK_SUCCESS && XTestFakeButtonEvent(helper_disp, button, False, 0) == 0) {
        status = UIOHOOK_FAILURE;
    }

    return UIOHOOK_SUCCESS;
}

static int post_mouse_motion_event(uiohook_event * const event) {
    int status = UIOHOOK_FAILURE;

    if (XTestFakeMotionEvent(helper_disp, -1, event->data.mouse.x, event->data.mouse.y, 0) != 0) {
        status = UIOHOOK_SUCCESS;
    }

    return status;
}

// TODO This should return a status code, UIOHOOK_SUCCESS or otherwise.
UIOHOOK_API int hook_post_event(uiohook_event * const event) {
    if (helper_disp == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: XDisplay helper_disp is unavailable!\n",
                __FUNCTION__, __LINE__);
        return UIOHOOK_ERROR_X_OPEN_DISPLAY;
    }

    XLockDisplay(helper_disp);

    int status = UIOHOOK_FAILURE;
    switch (event->type) {
        case EVENT_KEY_PRESSED:
        case EVENT_KEY_RELEASED:
            status = post_key_event(event);
            break;

        case EVENT_MOUSE_PRESSED:
        case EVENT_MOUSE_RELEASED:
            status = post_mouse_button_event(event);
            break;

        case EVENT_MOUSE_WHEEL:
            status = post_mouse_wheel_event(event);
            break;

        case EVENT_MOUSE_MOVED:
        case EVENT_MOUSE_DRAGGED:
            status = post_mouse_motion_event(event);
            break;

        case EVENT_KEY_TYPED:
        case EVENT_MOUSE_CLICKED:

        case EVENT_HOOK_ENABLED:
        case EVENT_HOOK_DISABLED:

        default:
            logger(LOG_LEVEL_WARN, "%s [%u]: Ignoring post event type %#X\n",
                    __FUNCTION__, __LINE__, event->type);
            status = UIOHOOK_FAILURE;
    }

    // Don't forget to flush!
    XSync(helper_disp, True);
    XUnlockDisplay(helper_disp);

    return status;
}
