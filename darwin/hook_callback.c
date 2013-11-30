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

#include <pthread.h>
#include <sys/time.h>
#include <uiohook.h>

#include "input_converter.h"
#include "logger.h"
#include "osx_input_helper.h"

//
typedef struct {
	CGEventRef event;
	UniChar buffer[4];
	UniCharCount length;
} TISMessage;

static TISMessage data = {
	.event = NULL,
	.buffer = { 0x00 },
	.length = 0
};

// Click count globals.
static unsigned short click_count = 0;
static CGEventTimestamp click_time = 0;
static bool mouse_dragged = false;

static CFRunLoopSourceRef src_msg_port;
static CGEventFlags prev_event_mask, diff_event_mask, keyup_event_mask;
static const CGEventFlags key_event_mask =	kCGEventFlagMaskShift + 
											kCGEventFlagMaskControl +
											kCGEventFlagMaskAlternate + 
											kCGEventFlagMaskCommand;

// Structure for the current Unix epoch in milliseconds.
static struct timeval system_time;

// Virtual event pointer.
static virtual_event event;

extern pthread_mutex_t hook_running_mutex, hook_control_mutex;

// Event dispatch callback.
static dispatcher_t dispatcher = NULL;

UIOHOOK_API void hook_set_dispatch_proc(dispatcher_t dispatch_proc) {
	logger(LOG_LEVEL_DEBUG,	"%s [%u]: Setting new dispatch callback to %#p.\n", 
			__FUNCTION__, __LINE__, dispatch_proc);

	dispatcher = dispatch_proc;
}

// Send out an event if a dispatcher was set.
static inline void dispatch_event(virtual_event *const event) {
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

// Keyboard Upper 16 / Mouse Lower 16
static CGEventFlags curr_modifier_mask = 0x00000000;

void set_modifier_mask(CGEventFlags mask) {
	curr_modifier_mask |= mask;
}

void unset_modifier_mask(CGEventFlags mask) {
	curr_modifier_mask ^= mask;
}

CGEventFlags get_modifiers() {
	return curr_modifier_mask;
}



// Runloop to execute KeyCodeToString on the "Main" runloop due to an 
// undocumented thread safety requirement.
static void message_port_proc(void *info) {
	TISMessage *data = (TISMessage *) info;

	if (data->event != NULL) {
		// Preform Unicode lookup.
		keycode_to_string(data->event, sizeof(data->buffer), &(data->length), data->buffer);
	}

	// Unlock the control mutex to signal that we have finished on the main 
	// runloop.
	pthread_mutex_unlock(&hook_control_mutex);
}

void start_message_port_runloop() {
	CFRunLoopSourceContext context = {
		.version = 0,
		.info = &data,
		.retain = NULL,
		.release = NULL,
		.copyDescription = NULL,
		.equal = NULL,
		.hash = NULL,
		.schedule = NULL,
		.cancel = NULL,

		.perform = message_port_proc
	};

	src_msg_port = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &context);
	CFRunLoopAddSource(CFRunLoopGetMain(), src_msg_port, kCFRunLoopDefaultMode);

	logger(LOG_LEVEL_DEBUG,	"%s [%u]: Successful.\n", 
			__FUNCTION__, __LINE__);
}

void stop_message_port_runloop() {
	if (CFRunLoopContainsSource(CFRunLoopGetMain(), src_msg_port, kCFRunLoopDefaultMode)) {
		CFRunLoopRemoveSource(CFRunLoopGetMain(), src_msg_port, kCFRunLoopDefaultMode);
		CFRelease(src_msg_port);
	}

	src_msg_port = NULL;

	logger(LOG_LEVEL_DEBUG,	"%s [%u]: Successful.\n", 
			__FUNCTION__, __LINE__);
}

void hook_status_proc(CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *info) {
	switch (activity) {
		case kCFRunLoopEntry:
			pthread_mutex_lock(&hook_running_mutex);
			pthread_mutex_unlock(&hook_control_mutex);
			break;

		case kCFRunLoopExit:
			// We do not need to touch the hook_control_mutex because 
			// hook_disable() is blocking on pthread_join().
			pthread_mutex_unlock(&hook_running_mutex);
			break;

		#ifdef DEBUG
		default:
			fprintf(stderr, "CallbackProc(): Unhandled Activity: 0x%X\n", (unsigned int) activity);
			break;
		#endif
	}
}

CGEventRef hook_event_proc(CGEventTapProxy tap_proxy, CGEventType type_ref, CGEventRef event_ref, void *refcon) {
	// Event data.
	CGPoint event_point;

	// Set the event.time.
	gettimeofday(&system_time, NULL);
	event.time = (system_time.tv_sec * 1000) + (system_time.tv_usec / 1000);

	UInt64	keycode, button;
	CGEventFlags event_mask = CGEventGetFlags(event_ref);

	// Get the event class.
	switch (type_ref) {
		case kCGEventKeyDown:
		EVENT_KEYDOWN:
			// Fire key pressed event.
			event.type = EVENT_KEY_PRESSED;
			event.mask = convert_to_virtual_mask(get_modifiers());
			
			keycode = CGEventGetIntegerValueField(event_ref, kCGKeyboardEventKeycode);
			event.data.keyboard.keycode = keycode_to_scancode(keycode);
			event.data.keyboard.rawcode = keycode;
			event.data.keyboard.keychar = CHAR_UNDEFINED;
			
			logger(LOG_LEVEL_INFO,	"%s [%u]: Key %#X pressed. (%#X)\n", 
				__FUNCTION__, __LINE__, event.data.keyboard.keycode, event.data.keyboard.rawcode);
			dispatch_event(&event);


			// Lookup the Unicode representation for this event.
			CFRunLoopSourceContext context = { .version = 0 };
			CFRunLoopSourceGetContext(src_msg_port, &context);

			// Get the run loop context info pointer.
			TISMessage *info = (TISMessage *) context.info;

			// Set the event pointer.
			info->event = event_ref;

			// Lock the control mutex as we enter the main run loop.
			pthread_mutex_lock(&hook_control_mutex);

			// Signal the custom source and wakeup the main run loop.
			CFRunLoopSourceSignal(src_msg_port);
			CFRunLoopWakeUp(CFRunLoopGetMain());

			// Wait for a lock while the main run loop processes they key typed event.
			if (pthread_mutex_lock(&hook_control_mutex) == 0) {
				pthread_mutex_unlock(&hook_control_mutex);
			}

			if (info->length == 1) {
				// Fire key typed event.
				event.type = EVENT_KEY_TYPED;

				event.data.keyboard.keycode = VC_UNDEFINED;
				event.data.keyboard.keychar = info->buffer[0];

				logger(LOG_LEVEL_INFO,	"%s [%u]: Key %#X typed. (%ls)\n", 
					__FUNCTION__, __LINE__, event.data.keyboard.keycode, event.data.keyboard.keychar);
				dispatch_event(&event);
			}

			info->event = NULL;
			info->length = 0;
			break;

		case kCGEventKeyUp:
		EVENT_KEYUP:
			// Fire key released event.
			event.type = EVENT_KEY_RELEASED;
			event.mask = convert_to_virtual_mask(get_modifiers());
			
			keycode = CGEventGetIntegerValueField(event_ref, kCGKeyboardEventKeycode);
			event.data.keyboard.keycode = keycode_to_scancode(keycode);
			event.data.keyboard.rawcode = keycode;
			event.data.keyboard.keychar = CHAR_UNDEFINED;
			
			logger(LOG_LEVEL_INFO,	"%s [%u]: Key %#X released. (%#X)\n", 
				__FUNCTION__, __LINE__, event.data.keyboard.keycode, event.data.keyboard.rawcode);
			dispatch_event(&event);
			break;

		case kCGEventFlagsChanged:
			#ifdef DEBUG
			fprintf(stdout, "LowLevelProc(): Modifiers Changed (0x%X)\n", (unsigned int) event_mask);
			#endif

			/* Because Apple treats modifier keys differently than normal key 
			 * events, any changes to the modifier keys will require a key state 
			 * change to be fired manually.
			 * 
			 * Outline of what is happening on the next 3 lines.
 			 * 1010 1100	prev
			 * 1100 1010	curr
			 * 0110 0110	prev xor curr
			 *
			 * turned on - i.e. pressed
			 * 1100 1010	curr
			 * 0110 0110	(prev xor curr)
			 * 0100 0010	(prev xor curr) of prev
			 *
			 * turned off - i.e. released
			 * 1010 1100	prev
			 * 0110 0110	(prev xor curr)
			 * 0010 0100	(prev xor curr) and prev
			 *
			 * CGEventFlags diff_event_mask = prev_event_mask ^ event_mask;
			 * CGEventFlags keydown_event_mask = prev_event_mask | diff_event_mask;
			 * CGEventFlags keyup_event_mask = event_mask & diff_event_mask;
			 */

			prev_event_mask = get_modifiers() & 0xFFFF0000;
			diff_event_mask = prev_event_mask ^ (event_mask & 0xFFFF0000);
			keyup_event_mask = (event_mask & 0xFFFF0000) & diff_event_mask;

			// Update the previous event mask.
			unset_modifier_mask(prev_event_mask);
			set_modifier_mask(event_mask & 0xFFFF0000);

			if (diff_event_mask & key_event_mask && keyup_event_mask & key_event_mask) {
				// Process as a key pressed event.
				goto EVENT_KEYDOWN;
			}
			else if (diff_event_mask & key_event_mask && keyup_event_mask ^ key_event_mask) {
				// Process as a key released event.
				goto EVENT_KEYUP;
			}
			break;

		case kCGEventLeftMouseDown:
			button = kVK_LBUTTON;
			set_modifier_mask(kCGEventFlagMaskButtonLeft);
			goto BUTTONDOWN;

		case kCGEventRightMouseDown:
			button = kVK_RBUTTON;
			set_modifier_mask(kCGEventFlagMaskButtonRight);
			goto BUTTONDOWN;

		case kCGEventOtherMouseDown:
			button = CGEventGetIntegerValueField(event_ref, kCGMouseEventButtonNumber);

			if (button == kVK_MBUTTON) {
				set_modifier_mask(kCGEventFlagMaskButtonCenter);
			}
			else if (button == kVK_XBUTTON1) {
				set_modifier_mask(kCGEventFlagMaskXButton1);
			}
			else if (button == kVK_XBUTTON2) {
				set_modifier_mask(kCGEventFlagMaskXButton2);
			}

		BUTTONDOWN:
			// Track the number of clicks.
			if ((long int) (event.time - click_time) <= hook_get_multi_click_time()) {
				click_count++;
			}
			else {
				click_count = 1;
			}
			click_time = event.time;

			event_point = CGEventGetLocation(event_ref);
			
			// Fire mouse pressed event.
			event.type = EVENT_MOUSE_PRESSED;
			event.mask = convert_to_virtual_mask(event_mask);

			event.data.mouse.button = convert_to_virtual_button(button);
			event.data.mouse.clicks = click_count;
			event.data.mouse.x = event_point.x;
			event.data.mouse.y = event_point.y;

			logger(LOG_LEVEL_INFO,	"%s [%u]: Button%#X  pressed %u time(s). (%u, %u)\n", 
					__FUNCTION__, __LINE__, event.data.mouse.button, event.data.mouse.clicks, event.data.mouse.x, event.data.mouse.y);
			dispatch_event(&event);
			break;

		case kCGEventLeftMouseUp:
			button = kVK_LBUTTON;
			unset_modifier_mask(kCGEventFlagMaskButtonLeft);
			goto BUTTONUP;

		case kCGEventRightMouseUp:
			button = kVK_RBUTTON;
			unset_modifier_mask(kCGEventFlagMaskButtonRight);
			goto BUTTONUP;

		case kCGEventOtherMouseUp:
			button = CGEventGetIntegerValueField(event_ref, kCGMouseEventButtonNumber);

			if (button == kVK_MBUTTON) {
				unset_modifier_mask(kCGEventFlagMaskButtonCenter);
			}
			else if (button == kVK_XBUTTON1) {
				unset_modifier_mask(kCGEventFlagMaskXButton1);
			}
			else if (button == kVK_XBUTTON2) {
				unset_modifier_mask(kCGEventFlagMaskXButton2);
			}

		BUTTONUP:
			event_point = CGEventGetLocation(event_ref);
			
			// Fire mouse released event.
			event.type = EVENT_MOUSE_RELEASED;
			event.mask = convert_to_virtual_mask(event_mask);

			event.data.mouse.button = convert_to_virtual_button(button);
			event.data.mouse.clicks = click_count;
			event.data.mouse.x = event_point.x;
			event.data.mouse.y = event_point.y;

			logger(LOG_LEVEL_INFO,	"%s [%u]: Button%#X released %u time(s). (%u, %u)\n", 
					__FUNCTION__, __LINE__, event.data.mouse.button, event.data.mouse.clicks, event.data.mouse.x, event.data.mouse.y);
			dispatch_event(&event);
		
			
			if (mouse_dragged != true) {
				// Fire mouse clicked event.
				event.type = EVENT_MOUSE_CLICKED;
				event.mask = convert_to_virtual_mask(event_mask);

				event.data.mouse.button = convert_to_virtual_button(button);
				event.data.mouse.clicks = click_count;
				event.data.mouse.x = event_point.x;
				event.data.mouse.y = event_point.y;

				logger(LOG_LEVEL_INFO,	"%s [%u]: Button%#X clicked %u time(s). (%u, %u)\n", 
						__FUNCTION__, __LINE__, event.data.mouse.button, event.data.mouse.clicks, event.data.mouse.x, event.data.mouse.y);
				dispatch_event(&event);
			}
			break;


		case kCGEventLeftMouseDragged:
		case kCGEventRightMouseDragged:
		case kCGEventOtherMouseDragged:
			event_point = CGEventGetLocation(event_ref);

			// Reset the click count.
			if (click_count != 0 && (long int) (event.time - click_time) > hook_get_multi_click_time()) {
				click_count = 0;
			}

			event.type = EVENT_MOUSE_DRAGGED;
			event.mask = convert_to_virtual_mask(get_modifiers());

			event.data.mouse.button = MOUSE_NOBUTTON;
			event.data.mouse.clicks = click_count;
			event.data.mouse.x = event_point.x;
			event.data.mouse.y = event_point.y;

			// Set the mouse dragged flag.
			mouse_dragged = true;

			logger(LOG_LEVEL_INFO,	"%s [%u]: Mouse moved to %u, %u.\n", 
					__FUNCTION__, __LINE__, event.data.mouse.x, event.data.mouse.y);
			dispatch_event(&event);
			break;

		case kCGEventMouseMoved:
			event_point = CGEventGetLocation(event_ref);
			
			// Reset the click count.
			if (click_count != 0 && (long int) (event.time - click_time) > hook_get_multi_click_time()) {
				click_count = 0;
			}

			event.type = EVENT_MOUSE_MOVED;
			event.mask = convert_to_virtual_mask(get_modifiers());

			event.data.mouse.button = MOUSE_NOBUTTON;
			event.data.mouse.clicks = click_count;
			event.data.mouse.x = event_point.x;
			event.data.mouse.y = event_point.y;
			
			// Set the mouse dragged flag.
			mouse_dragged = false;

			logger(LOG_LEVEL_INFO,	"%s [%u]: Mouse moved to %u, %u.\n", 
					__FUNCTION__, __LINE__, event.data.mouse.x, event.data.mouse.y);
			dispatch_event(&event);
			break;

		case kCGEventScrollWheel:
			event_point = CGEventGetLocation(event_ref);

			// Track the number of clicks.
			if ((long int) (event.time - click_time) <= hook_get_multi_click_time()) {
				click_count++;
			}
			else {
				click_count = 1;
			}
			click_time = event.time;
			
			
			// Fire mouse wheel event.
			event.type = EVENT_MOUSE_WHEEL;
			event.mask = convert_to_virtual_mask(event_mask);
			
			event.data.wheel.clicks = click_count;
			event.data.wheel.x = event_point.x;
			event.data.wheel.y = event_point.y;
			
			// TODO Figure out of kCGScrollWheelEventDeltaAxis2 causes mouse events with zero rotation.
			if (CGEventGetIntegerValueField(event_ref, kCGScrollWheelEventIsContinuous) == 0) {
				event.data.wheel.type = WHEEL_UNIT_SCROLL;
			}
			else {
				event.data.wheel.type = WHEEL_BLOCK_SCROLL;
			}

			/* TODO Figure out the scroll wheel amounts are correct.  I
			* suspect that Apples Java implementation maybe reporting a
			* static "1" inaccurately.
			*/
			event.data.wheel.amount = CGEventGetIntegerValueField(event_ref, kCGScrollWheelEventPointDeltaAxis1) * -1;
			
			// Scrolling data uses a fixed-point 16.16 signed integer format (Ex: 1.0 = 0x00010000).
			event.data.wheel.rotation = CGEventGetIntegerValueField(event_ref, kCGScrollWheelEventDeltaAxis1) * -1;

			logger(LOG_LEVEL_INFO,	"%s [%u]: Mouse wheel rotated %i units. (%u)\n", 
					__FUNCTION__, __LINE__, event.data.wheel.amount * event.data.wheel.rotation, event.data.wheel.type);
			dispatch_event(&event);
			break;

		#ifdef DEBUG
		default:
			fprintf(stderr, "LowLevelProc(): Unhandled Event Type: 0x%X\n", type);
			break;
		#endif
	}

	return noErr;
}

