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
#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>
#include <uiohook.h>
#include <X11/Xlibint.h>
#include <X11/Xlib.h>
#include <X11/extensions/record.h>

#include "logger.h"
#include "input_helper.h"
#include "hook_callback.h"

// Modifiers for tracking key masks.
static uint16_t current_modifiers = 0x0000;

// For this struct, refer to libxnee, requires Xlibint.h
typedef union {
	unsigned char		type;
	xEvent				event;
	xResourceReq		req;
	xGenericReply		reply;
	xError				error;
	xConnSetupPrefix	setup;
} XRecordDatum;

// Mouse globals.
static unsigned short click_count = 0;
static long click_time = 0;
static bool mouse_dragged = false;

// Structure for the current Unix epoch in milliseconds.
static struct timeval system_time;
static Time previous_time = (Time) ~0x00;
static uint64_t offset_time = 0;

// Virtual event pointer.
static uiohook_event event;

// Event dispatch callback.
static dispatcher_t dispatcher = NULL;

extern pthread_mutex_t hook_running_mutex, hook_control_mutex;
extern pthread_cond_t hook_control_cond;

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
static inline void set_modifier_mask(uint16_t mask) {
	current_modifiers |= mask;
}

// Unset the native modifier mask for future events.
static inline void unset_modifier_mask(uint16_t mask) {
	current_modifiers ^= mask;
}

// Get the current native modifier mask state.
static inline uint16_t get_modifiers() {
	return current_modifiers;
}

void hook_event_proc(XPointer pointer, XRecordInterceptData *hook) {
	if (hook->category == XRecordStartOfData) {
		// Lock the running mutex to signal the hook has started.
		pthread_mutex_lock(&hook_running_mutex);

		event.type = EVENT_HOOK_START;

		// Set the event.time.
		// FIXME See if we can do something lighter with the event_time instead of more division.
		gettimeofday(&system_time, NULL);
		event.time = (system_time.tv_sec * 1000) + (system_time.tv_usec / 1000);

		event.mask = 0x00;
		event.reserved = 0x00;

		dispatch_event(&event);

		// Unlock the control mutex so hook_enable() can continue.
		pthread_cond_signal(&hook_control_cond);
		pthread_mutex_unlock(&hook_control_mutex);
	}
	else if (hook->category == XRecordEndOfData) {
		// Lock the control mutex until we exit.
		pthread_mutex_lock(&hook_control_mutex);

		// Send the  hook event end of data
		event.type = EVENT_HOOK_STOP;

		// Set the event.time.
		// FIXME See if we can do something lighter with the event_time instead of more division.
		gettimeofday(&system_time, NULL);
		event.time = (system_time.tv_sec * 1000) + (system_time.tv_usec / 1000);

		event.mask = 0x00;
		event.reserved = 0x00;

		dispatch_event(&event);
	}
	else if (hook->category == XRecordFromServer || hook->category == XRecordFromClient) {
		// Get XRecord data.
		XRecordDatum *data = (XRecordDatum *) hook->data;

		// Check for event clock reset.
		if (previous_time > hook->server_time) {
			// Get the local system time in UTC.
			gettimeofday(&system_time, NULL);

			// Convert the local system time to a Unix epoch in MS.
			uint64_t epoch_time = (system_time.tv_sec * 1000) + (system_time.tv_usec / 1000);

			// Calculate the offset based on the system and hook times.
			offset_time = epoch_time - hook->server_time;

			logger(LOG_LEVEL_INFO,	"%s [%u]: Resynchronizing event clock. (%llu)\n",
					__FUNCTION__, __LINE__, offset_time);
		}
		// Set the previous event time for click reset check above.
		previous_time = hook->server_time;

		// Set the event time to the server time + offset.
		event.time = hook->server_time + offset_time;

		// Make sure reserved bits are zeroed out.
		event.reserved = 0x00;

		// Use more readable variables.
		int event_type = data->type;
		BYTE event_code = data->event.u.u.detail;
		int event_x = data->event.u.keyButtonPointer.rootX;
		int event_y = data->event.u.keyButtonPointer.rootY;

		// FIXME Its not worth using the native modifier tracking.
		int event_mask = data->event.u.keyButtonPointer.state;

		KeySym keysym;
		unsigned short int button, scancode;
		switch (event_type) {
			case KeyPress:
				scancode = keycode_to_scancode(event_code);

				// TODO If you have a better suggestion for this ugly, let me know.
				if (scancode == VC_SHIFT_L)			set_modifier_mask(MASK_SHIFT_L);
				else if (scancode == VC_SHIFT_R)	set_modifier_mask(MASK_SHIFT_R);
				else if (scancode == VC_CONTROL_L)	set_modifier_mask(MASK_CTRL_L);
				else if (scancode == VC_CONTROL_R)	set_modifier_mask(MASK_CTRL_R);
				else if (scancode == VC_ALT_L)		set_modifier_mask(MASK_ALT_L);
				else if (scancode == VC_ALT_R)		set_modifier_mask(MASK_ALT_R);
				else if (scancode == VC_META_L)		set_modifier_mask(MASK_META_L);
				else if (scancode == VC_META_R)		set_modifier_mask(MASK_META_R);


				// Fire key pressed event.
				event.type = EVENT_KEY_PRESSED;
				event.mask = get_modifiers();

				keysym = keycode_to_keysym(event_code, event_mask);
				event.data.keyboard.keycode = scancode;
				event.data.keyboard.rawcode = keysym;
				event.data.keyboard.keychar = CHAR_UNDEFINED;

				logger(LOG_LEVEL_INFO,	"%s [%u]: Key %#X pressed. (%#X)\n",
						__FUNCTION__, __LINE__, event.data.keyboard.keycode, event.data.keyboard.rawcode);
				dispatch_event(&event);

				// If the pressed event was not consumed...
				if (event.reserved ^ 0x01) {
					// Check to make sure the key is printable.
					wchar_t keychar = keysym_to_unicode(keysym);
					if (keychar != 0x0000) {
						// Fire key typed event.
						event.type = EVENT_KEY_TYPED;

						event.data.keyboard.keycode = VC_UNDEFINED;
						event.data.keyboard.keychar = keychar;

						logger(LOG_LEVEL_INFO,	"%s [%u]: Key %#X typed. (%lc)\n",
								__FUNCTION__, __LINE__, event.data.keyboard.keycode, (wint_t) event.data.keyboard.keychar);
						dispatch_event(&event);
					}
				}
				break;

			case KeyRelease:
				scancode = keycode_to_scancode(event_code);

				// TODO If you have a better suggestion for this ugly, let me know.
				if (scancode == VC_SHIFT_L)			unset_modifier_mask(MASK_SHIFT_L);
				else if (scancode == VC_SHIFT_R)	unset_modifier_mask(MASK_SHIFT_R);
				else if (scancode == VC_CONTROL_L)	unset_modifier_mask(MASK_CTRL_L);
				else if (scancode == VC_CONTROL_R)	unset_modifier_mask(MASK_CTRL_R);
				else if (scancode == VC_ALT_L)		unset_modifier_mask(MASK_ALT_L);
				else if (scancode == VC_ALT_R)		unset_modifier_mask(MASK_ALT_R);
				else if (scancode == VC_META_L)		unset_modifier_mask(MASK_META_L);
				else if (scancode == VC_META_R)		unset_modifier_mask(MASK_META_R);


				// Fire key released event.
				event.type = EVENT_KEY_RELEASED;
				event.mask = get_modifiers();

				keysym = keycode_to_keysym(event_code, event_mask);
				event.data.keyboard.keycode = keycode_to_scancode(event_code);
				event.data.keyboard.rawcode = keysym;
				event.data.keyboard.keychar = CHAR_UNDEFINED;

				logger(LOG_LEVEL_INFO, "%s [%u]: Key %#X released. (%#X)\n",
						__FUNCTION__, __LINE__, event.data.keyboard.keycode, event.data.keyboard.rawcode);
				dispatch_event(&event);
				break;

			case ButtonPress:
				// Track the number of clicks.
				if ((long int) (event.time - click_time) <= hook_get_multi_click_time()) {
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

				/* This information is all static for X11, its up to the WM to
				 * decide how to interpret the wheel events.
				 */
				// TODO Should use constants and a lookup table for button codes.
				if (event_code > 0 && (event_code <= 3 || event_code == XButton1 || event_code == XButton2)) {
					// TODO This would probably be faster and simpler as a if (> 3) { event_code - 4 } conditional.
					button = (event_code & 0x03) | (event_code & 0x08) >> 1;

					set_modifier_mask(1 << (button + 7));

					// Fire mouse pressed event.
					event.type = EVENT_MOUSE_PRESSED;
					event.mask = get_modifiers();

					event.data.mouse.button = button;
					event.data.mouse.clicks = click_count;
					event.data.mouse.x = event_x;
					event.data.mouse.y = event_y;

					logger(LOG_LEVEL_INFO,	"%s [%u]: Button %u  pressed %u time(s). (%u, %u)\n",
							__FUNCTION__, __LINE__, event.data.mouse.button, event.data.mouse.clicks, event.data.mouse.x, event.data.mouse.y);
					dispatch_event(&event);
				}
				else if (event_code == WheelUp || event_code == WheelDown) {
					/* Scroll wheel release events.
					 * Scroll type: WHEEL_UNIT_SCROLL
					 * Scroll amount: 3 unit increments per notch
					 * Units to scroll: 3 unit increments
					 * Vertical unit increment: 15 pixels
					 */

					// Fire mouse wheel event.
					event.type = EVENT_MOUSE_WHEEL;
					event.mask = get_modifiers();

					event.data.wheel.clicks = click_count;
					event.data.wheel.x = event_x;
					event.data.wheel.y = event_y;

					/* X11 does not have an API call for acquiring the mouse scroll type.  This
					 * maybe part of the XInput2 (XI2) extention but I will wont know until it
					 * is available on my platform.  For the time being we will just use the
					 * unit scroll value.
					 */
					event.data.wheel.type = WHEEL_UNIT_SCROLL;

					/* Some scroll wheel properties are available via the new XInput2 (XI2)
					 * extention.  Unfortunately the extention is not available on my
					 * development platform at this time.  For the time being we will just
					 * use the Windows default value of 3.
					 */
					event.data.wheel.amount = 3;

					if (event_code == WheelUp) {
						// Wheel Rotated Up and Away.
						event.data.wheel.rotation = -1;
					}
					else { // event_code == WheelDown
						// Wheel Rotated Down and Towards.
						event.data.wheel.rotation = 1;
					}

					logger(LOG_LEVEL_INFO,	"%s [%u]: Mouse wheel rotated %i units. (%u)\n",
							__FUNCTION__, __LINE__, event.data.wheel.amount * event.data.wheel.rotation, event.data.wheel.type);
					dispatch_event(&event);
				}
				break;

			case ButtonRelease:
				// TODO Should use constants for button codes.
				if (event_code > 0 && (event_code <= 3 || event_code == XButton1 || event_code == XButton2)) {
					// Handle button release events.
					// TODO This would probably be faster and simpler as a if (> 3) { event_code - 4 } conditional.
					button = (event_code & 0x03) | (event_code & 0x08) >> 1;

					unset_modifier_mask(1 << (button + 7));

					// Fire mouse released event.
					event.type = EVENT_MOUSE_RELEASED;
					event.mask = get_modifiers();

					// TODO This would probably be faster and simpler as a if (> 3) { event_code - 4 } conditional.
					event.data.mouse.button = button;
					event.data.mouse.clicks = click_count;
					event.data.mouse.x = event_x;
					event.data.mouse.y = event_y;

					logger(LOG_LEVEL_INFO,	"%s [%u]: Button %u released %u time(s). (%u, %u)\n",
							__FUNCTION__, __LINE__, event.data.mouse.button, event.data.mouse.clicks, event.data.mouse.x, event.data.mouse.y);
					dispatch_event(&event);

					if (mouse_dragged != true) {
						// Fire mouse clicked event.
						event.type = EVENT_MOUSE_CLICKED;
						event.mask = get_modifiers();

						event.data.mouse.button = button;
						event.data.mouse.clicks = click_count;
						event.data.mouse.x = event_x;
						event.data.mouse.y = event_y;

						logger(LOG_LEVEL_INFO,	"%s [%u]: Button %u clicked %u time(s). (%u, %u)\n",
								__FUNCTION__, __LINE__, event.data.mouse.button, event.data.mouse.clicks, event.data.mouse.x, event.data.mouse.y);
						dispatch_event(&event);
					}
				}
				break;

			case MotionNotify:
				// Reset the click count.
				if (click_count != 0 && (long int) (event.time - click_time) > hook_get_multi_click_time()) {
					click_count = 0;
				}

				// Populate common event info.
				event.mask = get_modifiers();

				// Check the upper half of virtual modifiers for non zero
				// values and set the mouse dragged flag.
				mouse_dragged = event.mask >> 4 > 0;
				if (mouse_dragged) {
					// Create Mouse Dragged event.
					event.type = EVENT_MOUSE_DRAGGED;
				}
				else {
					// Create a Mouse Moved event.
					event.type = EVENT_MOUSE_MOVED;
				}

				event.data.mouse.button = MOUSE_NOBUTTON;
				event.data.mouse.clicks = click_count;
				event.data.mouse.x = event_x;
				event.data.mouse.y = event_y;

				logger(LOG_LEVEL_INFO,	"%s [%u]: Mouse moved to %u, %u.\n",
						__FUNCTION__, __LINE__, event.data.mouse.x, event.data.mouse.y);
				dispatch_event(&event);
				break;

			default:
				// In theory this *should* never execute.
				logger(LOG_LEVEL_WARN,	"%s [%u]: Unhandled Unix event! (%#X)\n",
						__FUNCTION__, __LINE__, (unsigned int) event_type);
				break;
		}
	}
	else {
		logger(LOG_LEVEL_WARN,	"%s [%u]: Unhandled Unix hook category! (%#X)\n",
				__FUNCTION__, __LINE__, hook->category);
	}

	// TODO There is no way to consume the XRecord event.

	XRecordFreeData(hook);
}
