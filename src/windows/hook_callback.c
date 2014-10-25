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

#include <limits.h>
#include <stdint.h>
#include <time.h>
#include <uiohook.h>
#include <windows.h>

#include "hook_callback.h"
#include "logger.h"
#include "input_helper.h"

// Modifiers for tracking key masks.
static unsigned short int current_modifiers = 0x0000;

// Structure for the current Unix epoch in milliseconds.
static FILETIME system_time;
static DWORD previous_time = (DWORD) ~0x00;
static uint64_t offset_time = 0;

// Key typed Unicode return values.
static WCHAR keywchar = '\0';

// Click count globals.
static unsigned short click_count = 0;
static DWORD click_time = 0;
static POINT last_click;

// Virtual event pointer.
static uiohook_event event;

extern HHOOK keyboard_event_hhook, mouse_event_hhook;

// Event dispatch callback.
static dispatcher_t dispatcher = NULL;

UIOHOOK_API void hook_set_dispatch_proc(dispatcher_t dispatch_proc) {
	logger(LOG_LEVEL_DEBUG,	"%s [%u]: Setting new dispatch callback to %#p.\n",
			__FUNCTION__, __LINE__, dispatch_proc);

	dispatcher = dispatch_proc;
}

// Send out an event if a dispatcher was set.
static inline void dispatch_event(uiohook_event *const event) {
	if (dispatcher != NULL) {
		logger(LOG_LEVEL_DEBUG,	"%s [%u]: Dispatching event type %u.\n",
				__FUNCTION__, __LINE__, event->type);

		dispatcher(event);
	}
	else {
		logger(LOG_LEVEL_WARN,	"%s [%u]: No dispatch callback set!\n",
				__FUNCTION__, __LINE__);
	}
}

// Set the native modifier mask for future events.
static inline void set_modifier_mask(unsigned short int mask) {
	current_modifiers |= mask;
}

// Unset the native modifier mask for future events.
static inline void unset_modifier_mask(unsigned short int mask) {
	current_modifiers ^= mask;
}

// Get the current native modifier mask state.
static inline unsigned short int get_modifiers() {
	return current_modifiers;
}

/* Retrieves the mouse wheel scroll type. This function cannot be included as
 * part of the input_helper.h due to platform specific calling restrictions.
 */
static inline unsigned short int get_scroll_wheel_type() {
	unsigned short int value;
	UINT wheel_type;

	SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &wheel_type, 0);
	if (wheel_type == WHEEL_PAGESCROLL) {
		value = WHEEL_BLOCK_SCROLL;
	}
	else {
		value = WHEEL_UNIT_SCROLL;
	}

	return value;
}

/* Retrieves the mouse wheel scroll amount. This function cannot be included as
 * part of the input_helper.h due to platform specific calling restrictions.
 */
static inline unsigned short int get_scroll_wheel_amount() {
	unsigned short int value;
	UINT wheel_amount;

	SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &wheel_amount, 0);
	if (wheel_amount == WHEEL_PAGESCROLL) {
		value = 1;
	}
	else {
		value = (unsigned short int) wheel_amount;
	}

	return value;
}

void hook_start_proc() {
	event.type = EVENT_HOOK_START;

	// Set the event.time.
	// FIXME See if we can do something lighter with the event_time instead of more division.
	GetSystemTimeAsFileTime(&system_time);
	uint64_t epoch_time = (((uint64_t) system_time.dwHighDateTime << 32) | system_time.dwLowDateTime) / 10000;
	event.time = epoch_time - 11644473600000;

	event.mask = 0x00;
	event.reserved = 0x00;

	dispatch_event(&event);
}

void hook_stop_proc() {
	event.type = EVENT_HOOK_STOP;

	// Set the event.time.
	// FIXME See if we can do something lighter with the event_time instead of more division.
	GetSystemTimeAsFileTime(&system_time);
	uint64_t epoch_time = (((uint64_t) system_time.dwHighDateTime << 32) | system_time.dwLowDateTime) / 10000;
	event.time = epoch_time - 11644473600000;

	event.mask = 0x00;
	event.reserved = 0x00;

	dispatch_event(&event);
}

static inline void fillEventDataMouse( LONG *x, LONG *y ){
	event.mask = get_modifiers();
	
	event.data.mouse.clicks = click_count;
	
	//Windows sends overflowed integers as coordinates while continuously moving the mouse to the left side (
	//overflowed x) or to the top upper corner (overflowed y) overflowed vars can't be operated (compare, etc) 
	//but thankfully our coordinates are unsigned, which can't overflow by definition, so they wrap-around.
	//values after wrap-around: 65535, 65534, 65533, so USHRT_MAX - 535 to safely cover those
	//if( event.data.mouse.x >= USHRT_MAX - 535 )
	if( *x < 0 )
		event.data.mouse.x = 0;
	else
		event.data.mouse.x = *x;
		
	if( *y < 0 )
		event.data.mouse.y = 0;
	else
		event.data.mouse.y = *y;
}

LRESULT CALLBACK hook_event_proc(int nCode, WPARAM wParam, LPARAM lParam) {

	// MS Keyboard event struct data.
	KBDLLHOOKSTRUCT *kbhook = (KBDLLHOOKSTRUCT *) lParam;

	// MS Mouse event struct data.
	MSLLHOOKSTRUCT *mshook = (MSLLHOOKSTRUCT *) lParam;

	// Grab the system uptime in MS.
	// NOTE This value rolls every 49.7 days.
	DWORD hook_time = GetTickCount();

	// Check for event clock reset.
	if (previous_time > hook_time) {
		// Get the local system time in UTC.
		GetSystemTimeAsFileTime(&system_time);

		// Convert the local system time to a Unix epoch in MS.
		// milliseconds = 100-nanoseconds / 10000
		uint64_t epoch_time = (((uint64_t) system_time.dwHighDateTime << 32) | system_time.dwLowDateTime) / 10000;

		// Convert Windows epoch to Unix epoch. (1970 - 1601 in milliseconds)
		epoch_time -= 11644473600000;

		// Calculate the offset based on the system and hook times.
		offset_time = epoch_time - hook_time;

		logger(LOG_LEVEL_INFO,	"%s [%u]: Resynchronizing event clock. (%llu)\n",
				__FUNCTION__, __LINE__, offset_time);
	}
	// Set the previous event time for click reset check above.
	previous_time = hook_time;

	// Set the event time to the server time + offset.
	event.time = hook_time + offset_time;

	// Make sure reserved bits are zeroed out.
	event.reserved = 0x00;

	switch(wParam) {
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			// Check and setup modifiers.
			if (kbhook->vkCode == VK_LSHIFT)		set_modifier_mask(MASK_SHIFT_L);
			else if (kbhook->vkCode == VK_RSHIFT)	set_modifier_mask(MASK_SHIFT_R);
			else if (kbhook->vkCode == VK_LCONTROL)	set_modifier_mask(MASK_CTRL_L);
			else if (kbhook->vkCode == VK_RCONTROL)	set_modifier_mask(MASK_CTRL_R);
			else if (kbhook->vkCode == VK_LMENU)	set_modifier_mask(MASK_ALT_L);
			else if (kbhook->vkCode == VK_RMENU)	set_modifier_mask(MASK_ALT_R);
			else if (kbhook->vkCode == VK_LWIN)		set_modifier_mask(MASK_META_L);
			else if (kbhook->vkCode == VK_RWIN)		set_modifier_mask(MASK_META_R);

			// Fire key pressed event.
			event.type = EVENT_KEY_PRESSED;
			event.mask = get_modifiers();

			event.data.keyboard.keycode = keycode_to_scancode(kbhook->vkCode);

			event.data.keyboard.rawcode = kbhook->vkCode;
			event.data.keyboard.keychar = CHAR_UNDEFINED;

			logger(LOG_LEVEL_INFO,	"%s [%u]: Key %#X pressed. (%#X)\n",
					__FUNCTION__, __LINE__, event.data.keyboard.keycode, event.data.keyboard.rawcode);
			dispatch_event(&event);

			// If the pressed event was not consumed and a wchar exists...
			int type_count = keysym_to_unicode(kbhook->vkCode, &keywchar);
			for (int i = 0; event.reserved ^ 0x01 && i < type_count; i++) {
				// Fire key typed event.
				event.type = EVENT_KEY_TYPED;
				event.mask = get_modifiers();

				event.data.keyboard.keycode = VC_UNDEFINED;
				event.data.keyboard.rawcode = kbhook->vkCode;
				event.data.keyboard.keychar = keywchar;

				logger(LOG_LEVEL_INFO,	"%s [%u]: Key %#X typed. (%lc)\n",
						__FUNCTION__, __LINE__, event.data.keyboard.keycode, (wint_t) event.data.keyboard.keychar);
				dispatch_event(&event);
			}
			break;

		case WM_KEYUP:
		case WM_SYSKEYUP:
			// Check and setup modifiers.
			if (kbhook->vkCode == VK_LSHIFT)		unset_modifier_mask(MASK_SHIFT_L);
			else if (kbhook->vkCode == VK_RSHIFT)	unset_modifier_mask(MASK_SHIFT_R);
			else if (kbhook->vkCode == VK_LCONTROL)	unset_modifier_mask(MASK_CTRL_L);
			else if (kbhook->vkCode == VK_RCONTROL)	unset_modifier_mask(MASK_CTRL_R);
			else if (kbhook->vkCode == VK_LMENU)	unset_modifier_mask(MASK_ALT_L);
			else if (kbhook->vkCode == VK_RMENU)	unset_modifier_mask(MASK_ALT_R);
			else if (kbhook->vkCode == VK_LWIN)		unset_modifier_mask(MASK_META_L);
			else if (kbhook->vkCode == VK_RWIN)		unset_modifier_mask(MASK_META_R);

			// Fire key released event.
			event.type = EVENT_KEY_RELEASED;
			event.mask = get_modifiers();

			event.data.keyboard.keycode = keycode_to_scancode(kbhook->vkCode);

			event.data.keyboard.rawcode = kbhook->vkCode;
			event.data.keyboard.keychar = CHAR_UNDEFINED;

			logger(LOG_LEVEL_INFO,	"%s [%u]: Key %#X released. (%#X)\n",
					__FUNCTION__, __LINE__, event.data.keyboard.keycode, event.data.keyboard.rawcode);
			dispatch_event(&event);
			break;

		case WM_LBUTTONDOWN:
			event.data.mouse.button = MOUSE_BUTTON1;
			set_modifier_mask(MASK_BUTTON1);
			goto BUTTONDOWN;

		case WM_RBUTTONDOWN:
			event.data.mouse.button = MOUSE_BUTTON2;
			set_modifier_mask(MASK_BUTTON2);
			goto BUTTONDOWN;

		case WM_MBUTTONDOWN:
			event.data.mouse.button = MOUSE_BUTTON3;
			set_modifier_mask(MASK_BUTTON3);
			goto BUTTONDOWN;

		case WM_XBUTTONDOWN:
		case WM_NCXBUTTONDOWN:
			if (HIWORD(mshook->mouseData) == XBUTTON1) {
				event.data.mouse.button = MOUSE_BUTTON4;
				set_modifier_mask(MASK_BUTTON4);
			}
			else if (HIWORD(mshook->mouseData) == XBUTTON2) {
				event.data.mouse.button = MOUSE_BUTTON5;
				set_modifier_mask(MASK_BUTTON5);
			}
			else {
				// Extra mouse buttons.
				event.data.mouse.button = HIWORD(mshook->mouseData);
			}

		BUTTONDOWN:
			// Track the number of clicks.
			if ((long) (event.time - click_time) <= hook_get_multi_click_time()) {
				if (click_count < USHRT_MAX) {
					click_count++;
				}
				else {
					logger(LOG_LEVEL_WARN, "%s [%u]: Click count overflow detected!\n",
							__FUNCTION__, __LINE__);
				}
			}
			else {
				click_count = 1;
			}
			click_time = event.time;

			// Store the last click point.
			last_click.x = mshook->pt.x;
			last_click.y = mshook->pt.y;

			// Fire mouse pressed event.
			event.type = EVENT_MOUSE_PRESSED;

			fillEventDataMouse( &mshook->pt.x, &mshook->pt.y );

			logger(LOG_LEVEL_INFO,	"%s [%u]: Button %u  pressed %u time(s). (%u, u)\n",
					__FUNCTION__, __LINE__, event.data.mouse.button, event.data.mouse.clicks, event.data.mouse.x, event.data.mouse.y);
			dispatch_event(&event);
			break;

		case WM_LBUTTONUP:
			event.data.mouse.button = MOUSE_BUTTON1;
			unset_modifier_mask(MASK_BUTTON1);
			goto BUTTONUP;

		case WM_RBUTTONUP:
			event.data.mouse.button = MOUSE_BUTTON2;
			unset_modifier_mask(MASK_BUTTON2);
			goto BUTTONUP;

		case WM_MBUTTONUP:
			event.data.mouse.button = MOUSE_BUTTON3;
			unset_modifier_mask(MASK_BUTTON3);
			goto BUTTONUP;

		case WM_XBUTTONUP:
		case WM_NCXBUTTONUP:
			if (HIWORD(mshook->mouseData) == XBUTTON1) {
				event.data.mouse.button = MOUSE_BUTTON4;
				unset_modifier_mask(MASK_BUTTON4);
			}
			else if (HIWORD(mshook->mouseData) == XBUTTON2) {
				event.data.mouse.button = MOUSE_BUTTON5;
				unset_modifier_mask(MASK_BUTTON5);
			}
			else {
				// Extra mouse buttons.
				event.data.mouse.button = HIWORD(mshook->mouseData);
			}

		BUTTONUP:
			// Fire mouse released event.
			event.type = EVENT_MOUSE_RELEASED;

			fillEventDataMouse( &mshook->pt.x, &mshook->pt.y );

			logger(LOG_LEVEL_INFO,	"%s [%u]: Button %u released %u time(s). (%u, %u)\n",
					__FUNCTION__, __LINE__, event.data.mouse.button, event.data.mouse.clicks, event.data.mouse.x, event.data.mouse.y);
			dispatch_event(&event);

			if (last_click.x == mshook->pt.x && last_click.y == mshook->pt.y) {
				// Fire mouse clicked event.
				event.type = EVENT_MOUSE_CLICKED;
				// TODO This shouldn't be necessary but double check that the
				//		ptr const makes this value immutable.
				//event.mask = get_modifiers();
				//event.data.mouse.button = button;

				fillEventDataMouse( &mshook->pt.x, &mshook->pt.y );

				logger(LOG_LEVEL_INFO,	"%s [%u]: Button %u clicked %u time(s). (%u, %u)\n",
						__FUNCTION__, __LINE__, event.data.mouse.button, event.data.mouse.clicks, event.data.mouse.x, event.data.mouse.y);
				dispatch_event(&event);
			}
			break;

		case WM_MOUSEMOVE:
			// Reset the click count.
			if (click_count != 0 && (long) (event.time - click_time) > hook_get_multi_click_time()) {
				click_count = 0;
			}

			// We received a mouse move event with the mouse actually moving.
			// This verifies that the mouse was moved after being depressed.
			if (last_click.x != mshook->pt.x || last_click.y != mshook->pt.y) {
				// Check the upper half of the current modifiers for non zero
				// value.  This indicates the presence of a button down mask.
				if (get_modifiers() >> 8) {
					// Create Mouse Dragged event.
					event.type = EVENT_MOUSE_DRAGGED;
				}
				else {
					// Create a Mouse Moved event.
					event.type = EVENT_MOUSE_MOVED;
				}

				// Populate common event info.
				//event.mask = get_modifiers();

				event.data.mouse.button = MOUSE_NOBUTTON;
				fillEventDataMouse( &mshook->pt.x, &mshook->pt.y );

				logger(LOG_LEVEL_INFO,	"%s [%u]: Mouse moved to %u, %u.\n",
						__FUNCTION__, __LINE__,  event.data.mouse.x, event.data.mouse.y);
				dispatch_event(&event);
			}
			break;

		case WM_MOUSEWHEEL:
			// Track the number of clicks.
			if ((long) (event.time - click_time) <= hook_get_multi_click_time()) {
				click_count++;
			}
			else {
				click_count = 1;
			}
			click_time = event.time;

			// Fire mouse wheel event.
			event.type = EVENT_MOUSE_WHEEL;
			//event.mask = get_modifiers();

			event.data.wheel.type = get_scroll_wheel_type();
			event.data.wheel.amount = get_scroll_wheel_amount();

			//ATT: fillEventDataMouse( &mshook->pt.x, &mshook->pt.y ); data.wheel not data.mouse
			//TODO: complete/improve
			event.data.wheel.x = mshook->pt.x;
			event.data.wheel.y = mshook->pt.y;

			/* Delta HIWORD(mshook->mouseData)
			 * A positive value indicates that the wheel was rotated
			 * forward, away from the user; a negative value indicates that
			 * the wheel was rotated backward, toward the user. One wheel
			 * click is defined as WHEEL_DELTA, which is 120. */
			//event.data.wheel.rotation = ((signed short) HIWORD(mshook->mouseData) / WHEEL_DELTA) * -1;
			//Removing -1 as MS assumption is more natural (follows the cartesian coordinate system)
			event.data.wheel.rotation = ((signed short) HIWORD(mshook->mouseData) / WHEEL_DELTA);

			logger(LOG_LEVEL_INFO,	"%s [%u]: Mouse wheel type %u, rotated %i units at %u, %u\n",
				__FUNCTION__, __LINE__, event.data.wheel.type, event.data.wheel.amount * 
				event.data.wheel.rotation, event.data.wheel.x, event.data.wheel.y);
			dispatch_event(&event);
			break;

		default:
			// In theory this *should* never execute.
			logger(LOG_LEVEL_WARN,	"%s [%u]: Unhandled Windows event! (%#X)\n",
					__FUNCTION__, __LINE__, (unsigned int) wParam);
			break;
	}

	LRESULT hook_result = -1;
	if (nCode < 0 || event.reserved ^ 0x01) {
		hook_result = CallNextHookEx(keyboard_event_hhook, nCode, wParam, lParam);
	}
	else {
		logger(LOG_LEVEL_DEBUG,	"%s [%u]: Consuming the current event. (%li)\n",
				__FUNCTION__, __LINE__, (long) hook_result);
	}

	return hook_result;
}

void initialize_modifiers() {
		current_modifiers = 0x0000;

		if (GetKeyState(VK_LSHIFT)	 < 0)	set_modifier_mask(MASK_SHIFT_L);
		if (GetKeyState(VK_RSHIFT)   < 0)	set_modifier_mask(MASK_SHIFT_R);
		if (GetKeyState(VK_LCONTROL) < 0)	set_modifier_mask(MASK_CTRL_L);
		if (GetKeyState(VK_RCONTROL) < 0)	set_modifier_mask(MASK_CTRL_R);
		if (GetKeyState(VK_LMENU)    < 0)	set_modifier_mask(MASK_ALT_L);
		if (GetKeyState(VK_RMENU)    < 0)	set_modifier_mask(MASK_ALT_R);
		if (GetKeyState(VK_LWIN)     < 0)	set_modifier_mask(MASK_META_L);
		if (GetKeyState(VK_RWIN)     < 0)	set_modifier_mask(MASK_META_R);
}
