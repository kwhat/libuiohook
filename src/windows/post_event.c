/* libUIOHook: Cross-platfrom userland keyboard and mouse hooking.
 * Copyright (C) 2006-2016 Alexander Barker.  All Rights Received.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <uiohook.h>
#include <windows.h>
#include <math.h>

#include "input_helper.h"
#include "logger.h"

// Some buggy versions of MinGW and MSys do not include these constants in winuser.h.
#ifndef MAPVK_VK_TO_VSC
#define MAPVK_VK_TO_VSC			0
#define MAPVK_VSC_TO_VK			1
#define MAPVK_VK_TO_CHAR		2
#define MAPVK_VSC_TO_VK_EX		3
#endif
// Some buggy versions of MinGW and MSys only define this value for Windows
// versions >= 0x0600 (Windows Vista) when it should be 0x0500 (Windows 2000).
#ifndef MAPVK_VK_TO_VSC_EX
#define MAPVK_VK_TO_VSC_EX		4
#endif

#ifndef KEYEVENTF_SCANCODE
#define KEYEVENTF_EXTENDEDKEY	0x0001
#define KEYEVENTF_KEYUP			0x0002
#define	KEYEVENTF_UNICODE		0x0004
#define KEYEVENTF_SCANCODE		0x0008
#endif

#ifndef KEYEVENTF_KEYDOWN
#define KEYEVENTF_KEYDOWN		0x0000
#endif

#define MAX_WINDOWS_COORD_VALUE (1 << 16)

static UINT keymask_lookup[8] = {
	VK_LSHIFT,
	VK_LCONTROL,
	VK_LWIN,
	VK_LMENU,

	VK_RSHIFT,
	VK_RCONTROL,
	VK_RWIN,
	VK_RMENU
};

// http://letcoderock.blogspot.fr/2011/10/sendinput-with-shift-key-not-work.html
static UINT extend_keys[10] = {
    VK_UP,
    VK_DOWN,
    VK_LEFT,
    VK_RIGHT,
    VK_HOME,
    VK_END,
    VK_PRIOR, // PgUp
    VK_NEXT,  //  PgDn
    VK_INSERT,
    VK_DELETE
};

// See https://stackoverflow.com/a/4555214 and its comments
static inline int convert_to_relative_position(int coordinate, int screen_size){
	int offset = (coordinate > 0 ? 1 : -1); // Negative coordinates appear when using multiple monitors
	return ((coordinate * MAX_WINDOWS_COORD_VALUE) / screen_size) + offset;
}

UIOHOOK_API void hook_post_event(uiohook_event * const event) {
	//FIXME implement multiple monitor support
	uint16_t screen_width   = GetSystemMetrics( SM_CXSCREEN );
	uint16_t screen_height  = GetSystemMetrics( SM_CYSCREEN );

	unsigned char events_size = 0, events_max = 28;
	INPUT *events = malloc(sizeof(INPUT) * events_max);
	
	switch (event->type) {
		case EVENT_KEY_PRESSED:
			events[events_size].ki.wVk = scancode_to_keycode(event->data.keyboard.keycode);
			if (events[events_size].ki.wVk != 0x0000) {
				events[events_size].type = INPUT_KEYBOARD;
				events[events_size].ki.dwFlags = KEYEVENTF_KEYDOWN; // |= KEYEVENTF_SCANCODE;
				if (event->mask & MASK_SHIFT){
					for (unsigned int i = 0; i < sizeof(extend_keys) / sizeof(UINT); i++) {
						if (events[events_size].ki.wVk == extend_keys[i]){
							events[events_size].ki.dwFlags = KEYEVENTF_EXTENDEDKEY;
						}
					}
				}
				events[events_size].ki.wScan = 0; // event->data.keyboard.keycode;
				events[events_size].ki.time = 0; // GetSystemTime()
				events_size++;
			}
			else {
				logger(LOG_LEVEL_INFO, "%s [%u]: Unable to lookup scancode: %li\n",
						__FUNCTION__, __LINE__,
						event->data.keyboard.keycode);
			}
			break;
			
		case EVENT_KEY_RELEASED:
			events[events_size].ki.wVk = scancode_to_keycode(event->data.keyboard.keycode);
			if (events[events_size].ki.wVk != 0x0000) {
				events[events_size].type = INPUT_KEYBOARD;
				events[events_size].ki.dwFlags = KEYEVENTF_KEYUP; // |= KEYEVENTF_SCANCODE;
				if (event->mask & MASK_SHIFT){
					for (unsigned int i = 0; i < sizeof(extend_keys) / sizeof(UINT); i++) {
						if (events[events_size].ki.wVk == extend_keys[i]){
							events[events_size].ki.dwFlags = KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP;
						}
					}
				}
				events[events_size].ki.wScan = 0; // event->data.keyboard.keycode;
				events[events_size].ki.time = 0; // GetSystemTime()
				events_size++;
			}
			else {
				logger(LOG_LEVEL_INFO, "%s [%u]: Unable to lookup scancode: %li\n",
						__FUNCTION__, __LINE__,
						event->data.keyboard.keycode);
			}
			break;

		
		case EVENT_MOUSE_PRESSED:
			events[events_size].type = INPUT_MOUSE;
			events[events_size].mi.dwFlags = MOUSEEVENTF_XDOWN;

			switch (event->data.mouse.button) {
				case MOUSE_BUTTON1:
					events[events_size].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
					break;

				case MOUSE_BUTTON2:
					events[events_size].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
					break;

				case MOUSE_BUTTON3:
					events[events_size].mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
					break;

				case MOUSE_BUTTON4:
					events[events_size].mi.mouseData = XBUTTON1;
					break;

			   case MOUSE_BUTTON5:
					events[events_size].mi.mouseData = XBUTTON2;
					break;
					
				default:
					// Extra buttons.
					if (event->data.mouse.button > 3) {
						events[events_size].mi.mouseData = event->data.mouse.button - 3;
					}
			}
			
			events[events_size].mi.dx = convert_to_relative_position(event->data.mouse.x, screen_width);
			events[events_size].mi.dy = convert_to_relative_position(event->data.mouse.y, screen_height);

			events[events_size].mi.dwFlags |= MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
			events[events_size].mi.time = 0; // GetSystemTime()

			events_size++;
			break;
			
		case EVENT_MOUSE_RELEASED:
			events[events_size].type = INPUT_MOUSE;
			events[events_size].mi.dwFlags = MOUSEEVENTF_XUP;
			
			switch (event->data.mouse.button) {
				case MOUSE_BUTTON1:
					events[events_size].mi.dwFlags = MOUSEEVENTF_LEFTUP;
					break;

				case MOUSE_BUTTON2:
					events[events_size].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
					break;

				case MOUSE_BUTTON3:
					events[events_size].mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
					break;

				case MOUSE_BUTTON4:
					events[events_size].mi.mouseData = XBUTTON1;
					break;

			   case MOUSE_BUTTON5:
					events[events_size].mi.mouseData = XBUTTON2;
					break;
					
				default:
					// Extra buttons.
					if (event->data.mouse.button > 3) {
						events[events_size].mi.mouseData = event->data.mouse.button - 3;
					}
			}
			
			events[events_size].mi.dx = convert_to_relative_position(event->data.mouse.x, screen_width);
			events[events_size].mi.dy = convert_to_relative_position(event->data.mouse.y, screen_height);

			events[events_size].mi.dwFlags |= MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
			events[events_size].mi.time = 0; // GetSystemTime()
			events_size++;
			break;

			
		case EVENT_MOUSE_WHEEL:
			events[events_size].type = INPUT_MOUSE;
			events[events_size].mi.dwFlags = MOUSEEVENTF_WHEEL;
			
			// type, amount and rotation?
			events[events_size].mi.mouseData = event->data.wheel.amount * event->data.wheel.rotation * WHEEL_DELTA;
			
			events[events_size].mi.dx = convert_to_relative_position(event->data.wheel.x, screen_width);
			events[events_size].mi.dy = convert_to_relative_position(event->data.wheel.y, screen_height);

			events[events_size].mi.dwFlags |= MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
			events[events_size].mi.time = 0; // GetSystemTime()
			events_size++;
			break;


		case EVENT_MOUSE_DRAGGED:
			// The button masks are all applied with the modifier masks.

		case EVENT_MOUSE_MOVED:
			events[events_size].type = INPUT_MOUSE;
			events[events_size].mi.dwFlags = MOUSEEVENTF_MOVE;

			events[events_size].mi.dx = convert_to_relative_position(event->data.mouse.x, screen_width);
			events[events_size].mi.dy = convert_to_relative_position(event->data.mouse.y, screen_height);

			events[events_size].mi.dwFlags |= MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
			events[events_size].mi.time = 0; // GetSystemTime()
			events_size++;
			break;


		case EVENT_MOUSE_CLICKED:
		case EVENT_KEY_TYPED:
			// Ignore clicked and typed events.
			
		case EVENT_HOOK_ENABLED:
		case EVENT_HOOK_DISABLED:
			// Ignore hook enabled / disabled events.

		default:
			// Ignore any other garbage.
			logger(LOG_LEVEL_WARN, "%s [%u]: Ignoring post event type %#X\n",
					__FUNCTION__, __LINE__, event->type);
			break;
	}
	
	// Create the key release input
	// memcpy(key_events + 1, key_events, sizeof(INPUT));
	// key_events[1].ki.dwFlags |= KEYEVENTF_KEYUP;

	if (! SendInput(events_size, events, sizeof(INPUT)) ) {
		logger(LOG_LEVEL_ERROR, "%s [%u]: SendInput() failed! (%#lX)\n",
				__FUNCTION__, __LINE__, (unsigned long) GetLastError());
	}

	free(events);
}
