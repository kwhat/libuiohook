/* libUIOHook: Cross-platfrom userland keyboard and mouse hooking.
 * Copyright (C) 2006-2014 Alexander Barker.  All Rights Received.
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

UIOHOOK_API void hook_post_event(uiohook_event * const event) {
	unsigned char events_size = 0, events_max = 28;
	INPUT *events = malloc(sizeof(INPUT) * events_max);

	double fScreenWidth   = GetSystemMetrics( SM_CXSCREEN )-1; 
	double fScreenHeight  = GetSystemMetrics( SM_CYSCREEN )-1;
	
	if (event->mask & (MASK_SHIFT | MASK_CTRL | MASK_META | MASK_ALT)) {
		for (unsigned int i = 0; i < sizeof(keymask_lookup) / sizeof(UINT); i++) {
			if (event->mask & 1 << i) {
				events[events_size].type = INPUT_KEYBOARD;
				events[events_size].ki.wVk = keymask_lookup[i];
				events[events_size].ki.dwFlags = KEYEVENTF_KEYDOWN;
				events[events_size].ki.time = 0; // Use current system time.
				events_size++;
			}
		}
	}

	if (event->mask & (MASK_BUTTON1 | MASK_BUTTON2 | MASK_BUTTON3 | MASK_BUTTON4 | MASK_BUTTON5)) {
		events[events_size].type = INPUT_MOUSE;
		events[events_size].mi.dx = 0;	// Relative mouse movement due to
		events[events_size].mi.dy = 0;	// MOUSEEVENTF_ABSOLUTE not being set.
		events[events_size].mi.mouseData = 0x00;
		events[events_size].mi.time = 0; // Use current system time.

		if (event->mask & MASK_BUTTON1) {
			events[events_size].mi.mouseData |= MOUSEEVENTF_LEFTDOWN;
		}

		if (event->mask & MASK_BUTTON2) {
			events[events_size].mi.mouseData |= MOUSEEVENTF_RIGHTDOWN;
		}

		if (event->mask & MASK_BUTTON3) {
			events[events_size].mi.mouseData |= MOUSEEVENTF_MIDDLEDOWN;
		}

		if (event->mask & MASK_BUTTON4) {
			events[events_size].mi.mouseData = XBUTTON1;
			events[events_size].mi.mouseData |= MOUSEEVENTF_XDOWN;
		}

		if (event->mask & MASK_BUTTON5) {
			events[events_size].mi.mouseData = XBUTTON2;
			events[events_size].mi.dwFlags |= MOUSEEVENTF_XDOWN;
		}

		events_size++;
	}

	char buffer[4];
	switch (event->type) {
		case EVENT_KEY_PRESSED:
			events[events_size].ki.dwFlags = 0x0000;  // KEYEVENTF_KEYDOWN
			goto EVENT_KEY;

		case EVENT_KEY_TYPED:
			// Need to convert a wchar_t to keysym!
			snprintf(buffer, 4, "%lc", (wint_t) event->data.keyboard.keychar);

			event->type = EVENT_KEY_PRESSED;
			event->data.keyboard.keycode = MapVirtualKey(VkKeyScanEx((TCHAR) event->data.keyboard.keycode, GetKeyboardLayout(0)), MAPVK_VK_TO_VSC_EX);
			event->data.keyboard.keychar = CHAR_UNDEFINED;
			hook_post_event(event);

		case EVENT_KEY_RELEASED:
			events[events_size].ki.dwFlags = KEYEVENTF_KEYUP;

		EVENT_KEY:
			events[events_size].type = INPUT_KEYBOARD;

			events[events_size].ki.wVk = MapVirtualKey(event->data.keyboard.keycode, MAPVK_VK_TO_VSC_EX);
			events[events_size].ki.wScan = event->data.keyboard.keycode;
			events[events_size].ki.dwFlags |= KEYEVENTF_SCANCODE;

			if ((events[events_size].ki.wVk >= 33 && events[events_size].ki.wVk <= 46) ||
					(events[events_size].ki.wVk >= 91 && events[events_size].ki.wVk <= 93)) {
				//Key is an extended key.
				events[events_size].ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
			}
			events[events_size].ki.time = 0; //GetSystemTime()
			events_size++;
			break;

		case EVENT_MOUSE_PRESSED:
		    switch( event->data.mouse.button ){
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
					events[events_size].mi.dwFlags = MOUSEEVENTF_XDOWN;
					events[events_size].mi.mouseData = XBUTTON1;
				break;
				case MOUSE_BUTTON5:
					events[events_size].mi.dwFlags = MOUSEEVENTF_XDOWN;
					events[events_size].mi.mouseData = XBUTTON2;
				break;
			}
			goto EVENT_MOUSEBUTTON;

		case EVENT_MOUSE_WHEEL:
			events[events_size].mi.dwFlags = MOUSEEVENTF_WHEEL;
			events[events_size].mi.mouseData = event->data.wheel.amount * event->data.wheel.rotation * WHEEL_DELTA;
			goto EVENT_MOUSEBUTTON;

		case EVENT_MOUSE_CLICKED:
			event->type = EVENT_MOUSE_PRESSED;
			hook_post_event(event);

		case EVENT_MOUSE_RELEASED:
			switch( event->data.mouse.button ){
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
					events[events_size].mi.dwFlags = MOUSEEVENTF_XUP;
					events[events_size].mi.mouseData = XBUTTON1;
				break;
				case MOUSE_BUTTON5:
					events[events_size].mi.dwFlags = MOUSEEVENTF_XUP;
					events[events_size].mi.mouseData = XBUTTON2;
				break;
			}
			goto EVENT_MOUSEBUTTON;

			//TODO: remove goto's and refactor
		EVENT_MOUSEBUTTON:
			events[events_size].type = INPUT_MOUSE;
			//http://msdn.microsoft.com/en-us/library/windows/desktop/ms646273%28v=vs.85%29.aspx
			//The coordinates need to be normalized
			if( event->type == EVENT_MOUSE_WHEEL ){
				events[events_size].mi.dx = event->data.wheel.x * (65535.0f/fScreenWidth);
				events[events_size].mi.dy = event->data.wheel.y * (65535.0f/fScreenHeight);
			}else{
				events[events_size].mi.dx = event->data.mouse.x * (65535.0f/fScreenWidth);
				events[events_size].mi.dy = event->data.mouse.y * (65535.0f/fScreenHeight);
			}				
			/*events[events_size].mi.dx = event->data.mouse.x;
			events[events_size].mi.dy = event->data.mouse.y;*/
			//TODO: why does try to move it?!?!? it should do an action and/or append mov, not override
			//events[events_size].mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
			//TODO: events[events_size].mi.dwFlags |= MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
			events[events_size].mi.dwFlags |= MOUSEEVENTF_ABSOLUTE;
			events[events_size].mi.time = 0; //GetSystemTime()
			events_size++;
			break;

		case EVENT_MOUSE_DRAGGED:
			// The button masks are all applied with the modifier masks.

		case EVENT_MOUSE_MOVED:
			events[events_size].mi.dwFlags = MOUSEEVENTF_MOVE;
			goto EVENT_MOUSEBUTTON;
			break;

		case EVENT_HOOK_START:
		case EVENT_HOOK_STOP:
			// TODO Figure out if we should start / stop the event hook
			// or fall thru to a warning.

		default:
			// FIXME Produce a warning.
			break;
	}

	// Release the previously held modifier keys used to fake the event mask.
	if (event->mask & (MASK_SHIFT | MASK_CTRL | MASK_META | MASK_ALT)) {
		for (unsigned int i = 0; i < sizeof(keymask_lookup) / sizeof(UINT); i++) {
			if (event->mask & 1 << i) {
				events[events_size].type = INPUT_KEYBOARD;
				events[events_size].ki.wVk = keymask_lookup[i];
				events[events_size].ki.dwFlags = KEYEVENTF_KEYUP;
				events[events_size].ki.time = 0; // Use current system time.
				events_size++;
			}
		}
	}

	if (event->mask & (MASK_BUTTON1 | MASK_BUTTON2 | MASK_BUTTON3 | MASK_BUTTON4 | MASK_BUTTON5)) {
		events[events_size].type = INPUT_MOUSE;
		events[events_size].mi.dx = 0;	// Relative mouse movement due to
		events[events_size].mi.dy = 0;	// MOUSEEVENTF_ABSOLUTE not being set.
		events[events_size].mi.mouseData = 0x00;
		events[events_size].mi.time = 0; // Use current system time.

		//http://msdn.microsoft.com/en-us/library/windows/desktop/ms646273%28v=vs.85%29.aspx
		//If dwFlags does not contain MOUSEEVENTF_WHEEL, MOUSEEVENTF_XDOWN, or MOUSEEVENTF_XUP, 
		//then mouseData should be zero.
		if (event->mask & MASK_BUTTON1) {
			events[events_size].mi.dwFlags |= MOUSEEVENTF_LEFTUP;
		}

		if (event->mask & MASK_BUTTON2) {
			events[events_size].mi.dwFlags |= MOUSEEVENTF_RIGHTUP;
		}

		if (event->mask & MASK_BUTTON3) {
			events[events_size].mi.dwFlags |= MOUSEEVENTF_MIDDLEUP;
		}

		if (event->mask & MASK_BUTTON4) {
			events[events_size].mi.mouseData = XBUTTON1;
			events[events_size].mi.dwFlags |= MOUSEEVENTF_XUP;
		}

		if (event->mask & MASK_BUTTON5) {
			events[events_size].mi.mouseData = XBUTTON2;
			events[events_size].mi.dwFlags |= MOUSEEVENTF_XUP;
		}

		events_size++;
	}

	//Create the key release input
	//memcpy(key_events + 1, key_events, sizeof(INPUT));
	//key_events[1].ki.dwFlags |= KEYEVENTF_KEYUP;

	if (! SendInput(events_size, events, sizeof(INPUT)) ) {
		logger(LOG_LEVEL_ERROR,	"%s [%u]: SendInput() failed! (%#lX)\n",
				__FUNCTION__, __LINE__, (unsigned long) GetLastError());
	}

	free(events);
}
