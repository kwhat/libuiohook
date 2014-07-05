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

#include <ApplicationServices/ApplicationServices.h>
#include <pthread.h>
#include <sys/time.h>
#include <uiohook.h>

#include "logger.h"
#include "input_helper.h"

// Modifiers for tracking key masks.
static uint16_t current_modifiers = 0x00000000;
static uint8_t modifier_flag = 0x0000;

// Required to transport messages between the main runloop and our thread for
// Unicode lookups.
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

// Structure for the current Unix epoch in milliseconds.
static struct timeval system_time;

// Virtual event pointer.
static virtual_event event;

static pthread_cond_t hook_control_cond = PTHREAD_COND_INITIALIZER;
extern pthread_mutex_t hook_running_mutex, hook_control_mutex;
extern Boolean restart_tap;

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

// Runloop to execute KeyCodeToString on the "Main" runloop due to an
// undocumented thread safety requirement.
static void message_port_proc(void *info) {
	// Lock the control mutex as we enter the main run loop.
	pthread_mutex_lock(&hook_control_mutex);

	TISMessage *data = (TISMessage *) info;

	if (data->event != NULL) {
		// Preform Unicode lookup.
		keycode_to_string(data->event, sizeof(data->buffer), &(data->length), data->buffer);
	}

	// Unlock the control mutex to signal that we have finished on the main
	// runloop.
	pthread_cond_signal(&hook_control_cond);
	pthread_mutex_unlock(&hook_control_mutex);
}

void start_message_port_runloop() {
	CFRunLoopSourceContext context = {
		.version			= 0,
		.info				= &data,
		.retain				= NULL,
		.release			= NULL,
		.copyDescription	= NULL,
		.equal				= NULL,
		.hash				= NULL,
		.schedule			= NULL,
		.cancel				= NULL,
		.perform			= message_port_proc
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
			// Lock the running mutex to signal the runloop has started.
			pthread_mutex_lock(&hook_running_mutex);

			 // Unlock the control mutex so hook_enable() can continue.
			pthread_mutex_unlock(&hook_control_mutex);
			break;

		case kCFRunLoopExit:
			// We do not need to touch the hook_control_mutex because
			// hook_disable() is blocking on pthread_join().

			// Unlock the running mutex.
			pthread_mutex_unlock(&hook_running_mutex);
			break;

		default:
			logger(LOG_LEVEL_WARN,	"%s [%u]: Unhandled RunLoop activity! (%#X)\n",
					__FUNCTION__, __LINE__, (unsigned int) activity);
			break;
	}
}

CGEventRef hook_event_proc(CGEventTapProxy tap_proxy, CGEventType type, CGEventRef event_ref, void *refcon) {
	// Event data.
	CGPoint event_point;

	// Set the event.time.
	gettimeofday(&system_time, NULL);
	event.time = (system_time.tv_sec * 1000) + (system_time.tv_usec / 1000);

	// Make sure reserved bits are zeroed out.
	event.reserved = 0x00;

	UInt64	keycode, button;
	CGEventFlags event_mask = CGEventGetFlags(event_ref);

	// Get the event class.
	switch (type) {
		case kCGEventKeyDown:
		EVENT_KEYDOWN:
			// Fire key pressed event.
			event.type = EVENT_KEY_PRESSED;
			event.mask = get_modifiers();

			keycode = CGEventGetIntegerValueField(event_ref, kCGKeyboardEventKeycode);
			event.data.keyboard.keycode = keycode_to_scancode(keycode);
			event.data.keyboard.rawcode = keycode;
			event.data.keyboard.keychar = CHAR_UNDEFINED;

			logger(LOG_LEVEL_INFO,	"%s [%u]: Key %#X pressed. (%#X)\n",
					__FUNCTION__, __LINE__, event.data.keyboard.keycode, event.data.keyboard.rawcode);
			dispatch_event(&event);

			// Make sure RunLoop main is currently running to avoid deadlock.
			CFStringRef mode = CFRunLoopCopyCurrentMode(CFRunLoopGetMain());
			if (mode != NULL) {
				CFRelease(mode);

				// If the pressed event was not consumed...
				if (event.reserved ^ 0x01) {
					// Lookup the Unicode representation for this event.
					CFRunLoopSourceContext context = { .version = 0 };
					CFRunLoopSourceGetContext(src_msg_port, &context);

					// Get the run loop context info pointer.
					TISMessage *info = (TISMessage *) context.info;

					// Set the event pointer.
					info->event = event_ref;

					// Signal the custom source and wakeup the main run loop.
					CFRunLoopSourceSignal(src_msg_port);
					CFRunLoopWakeUp(CFRunLoopGetMain());

					// Wait for a lock while the main run loop processes they key typed event.
					pthread_cond_wait(&hook_control_cond, &hook_control_mutex);
					pthread_mutex_unlock(&hook_control_mutex);

					if (info->length == 1) {
						// Fire key typed event.
						event.type = EVENT_KEY_TYPED;

						event.data.keyboard.keycode = VC_UNDEFINED;
						event.data.keyboard.keychar = info->buffer[0];

						logger(LOG_LEVEL_INFO,	"%s [%u]: Key %#X typed. (%lc)\n",
								__FUNCTION__, __LINE__, event.data.keyboard.keycode, (wint_t) event.data.keyboard.keychar);
						dispatch_event(&event);
					}

					info->event = NULL;
					info->length = 0;
				}
			}
			else {
				logger(LOG_LEVEL_WARN,	"%s [%u]: Failed to signal RunLoop Main!\n",
							__FUNCTION__, __LINE__);
			}
			break;

		case kCGEventKeyUp:
		EVENT_KEYUP:
			// Fire key released event.
			event.type = EVENT_KEY_RELEASED;
			event.mask = get_modifiers();

			keycode = CGEventGetIntegerValueField(event_ref, kCGKeyboardEventKeycode);
			event.data.keyboard.keycode = keycode_to_scancode(keycode);
			event.data.keyboard.rawcode = keycode;
			event.data.keyboard.keychar = CHAR_UNDEFINED;

			logger(LOG_LEVEL_INFO,	"%s [%u]: Key %#X released. (%#X)\n",
					__FUNCTION__, __LINE__, event.data.keyboard.keycode, event.data.keyboard.rawcode);
			dispatch_event(&event);
			break;

		case kCGEventFlagsChanged:
			logger(LOG_LEVEL_INFO,	"%s [%u]: Modifiers Changed. (%#X)\n",
					__FUNCTION__, __LINE__, (unsigned int) event_mask);

			/* Because Apple treats modifier keys differently than normal key
			 * events, any changes to the modifier keys will require a key state
			 * change to be fired manually.
			 */

			keycode = CGEventGetIntegerValueField(event_ref, kCGKeyboardEventKeycode);
			if (keycode == kVK_Shift) {
				modifier_flag = MASK_SHIFT_L;
			}
			else if (keycode == kVK_Control) {
				modifier_flag = MASK_CTRL_L;
			}
			else if (keycode == kVK_Command) {
				modifier_flag = MASK_META_L;
			}
			else if (keycode == kVK_Option) {
				modifier_flag = MASK_ALT_L;
			}
			else if (keycode == kVK_RightShift) {
				modifier_flag = MASK_SHIFT_R;
			}
			else if (keycode == kVK_RightControl) {
				modifier_flag = MASK_CTRL_R;
			}
			else if (keycode == kVK_RightCommand) {
				modifier_flag = MASK_META_R;
			}
			else if (keycode == kVK_RightOption) {
				modifier_flag = MASK_ALT_R;
			}
			else {
				modifier_flag = 0x0000;
			}

			// First check to see if a modifier we care about changed.
			if (modifier_flag != 0x0000) {
				if (get_modifiers() & modifier_flag) {
					unset_modifier_mask(modifier_flag);

					// Process as a key released event.
					goto EVENT_KEYUP;
				}
				else {
					set_modifier_mask(modifier_flag);

					// Process as a key pressed event.
					goto EVENT_KEYDOWN;
				}
			}
			break;

		case kCGEventLeftMouseDown:
		case kCGEventRightMouseDown:
		case kCGEventOtherMouseDown:
			button = CGEventGetIntegerValueField(event_ref, kCGMouseEventButtonNumber) + 1;
			set_modifier_mask(1 << (button + 7));

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
			event.mask = get_modifiers();

			event.data.mouse.button = button;
			event.data.mouse.clicks = click_count;
			event.data.mouse.x = event_point.x;
			event.data.mouse.y = event_point.y;

			logger(LOG_LEVEL_INFO,	"%s [%u]: Button %u  pressed %u time(s). (%u, %u)\n",
					__FUNCTION__, __LINE__, event.data.mouse.button, event.data.mouse.clicks, event.data.mouse.x, event.data.mouse.y);
			dispatch_event(&event);
			break;

		case kCGEventLeftMouseUp:
		case kCGEventRightMouseUp:
		case kCGEventOtherMouseUp:
			button = CGEventGetIntegerValueField(event_ref, kCGMouseEventButtonNumber) + 1;
			unset_modifier_mask(1 << (button + 7));

			event_point = CGEventGetLocation(event_ref);

			// Fire mouse released event.
			event.type = EVENT_MOUSE_RELEASED;
			event.mask = get_modifiers();

			event.data.mouse.button = button;
			event.data.mouse.clicks = click_count;
			event.data.mouse.x = event_point.x;
			event.data.mouse.y = event_point.y;

			logger(LOG_LEVEL_INFO,	"%s [%u]: Button %u released %u time(s). (%u, %u)\n",
					__FUNCTION__, __LINE__, event.data.mouse.button, event.data.mouse.clicks, event.data.mouse.x, event.data.mouse.y);
			dispatch_event(&event);


			if (mouse_dragged != true) {
				// Fire mouse clicked event.
				event.type = EVENT_MOUSE_CLICKED;
				event.mask = get_modifiers();

				event.data.mouse.button = button;
				event.data.mouse.clicks = click_count;
				event.data.mouse.x = event_point.x;
				event.data.mouse.y = event_point.y;

				logger(LOG_LEVEL_INFO,	"%s [%u]: Button %u clicked %u time(s). (%u, %u)\n",
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
			event.mask = get_modifiers();

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
			event.mask = get_modifiers();

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
			// Check to see what axis was rotated, we only care about axis 1 for vertical rotation.
			// TOOD Implement horizontal scrolling by examining axis 2.
			// NOTE kCGScrollWheelEventDeltaAxis3 is currently unused.
			if (CGEventGetIntegerValueField(event_ref, kCGScrollWheelEventDeltaAxis1) != 0) {
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
				event.mask = get_modifiers();

				event.data.wheel.clicks = click_count;
				event.data.wheel.x = event_point.x;
				event.data.wheel.y = event_point.y;

				// TODO Figure out of kCGScrollWheelEventDeltaAxis2 causes mouse events with zero rotation.
				if (CGEventGetIntegerValueField(event_ref, kCGScrollWheelEventIsContinuous) == 0) {
					// Scrolling data is line-based.
					event.data.wheel.type = WHEEL_BLOCK_SCROLL;
				}
				else {
					// Scrolling data is pixel-based.
					event.data.wheel.type = WHEEL_UNIT_SCROLL;
				}

				// TODO The result of kCGScrollWheelEventIsContinuous may effect this value.
				// Calculate the amount based on the Point Delta / Event Delta.  Integer sign should always be homogeneous resulting in a positive result.
				// NOTE kCGScrollWheelEventFixedPtDeltaAxis1 a floating point value (+0.1/-0.1) that takes acceleration into account.
				// NOTE kCGScrollWheelEventPointDeltaAxis1 will not build on OS X < 10.5
				event.data.wheel.amount = CGEventGetIntegerValueField(event_ref, kCGScrollWheelEventPointDeltaAxis1) / CGEventGetIntegerValueField(event_ref, kCGScrollWheelEventDeltaAxis1);

				// Scrolling data uses a fixed-point 16.16 signed integer format (Ex: 1.0 = 0x00010000).
				event.data.wheel.rotation = CGEventGetIntegerValueField(event_ref, kCGScrollWheelEventDeltaAxis1) * -1;

				logger(LOG_LEVEL_INFO,	"%s [%u]: Mouse wheel rotated %i units. (%u)\n",
						__FUNCTION__, __LINE__, event.data.wheel.amount * event.data.wheel.rotation, event.data.wheel.type);
				dispatch_event(&event);
			}
			break;

		default:
			// Check for an old OS X bug where the tap seems to timeout for no reason.
			// See: http://stackoverflow.com/questions/2969110/cgeventtapcreate-breaks-down-mysteriously-with-key-down-events#2971217
			if (type == (CGEventType) kCGEventTapDisabledByTimeout) {
				logger(LOG_LEVEL_WARN,	"%s [%u]: CGEventTap timeout!\n",
					__FUNCTION__, __LINE__);

				// We need to restart the tap!
				restart_tap = true;
				CFRunLoopStop(CFRunLoopGetCurrent());
			}
			else {
				// In theory this *should* never execute.
				logger(LOG_LEVEL_WARN,	"%s [%u]: Unhandled Darwin event! (%#X)\n",
						__FUNCTION__, __LINE__, (unsigned int) type);
			}
			break;
	}

	CGEventRef result_ref = NULL;
	if (event.reserved ^ 0x01) {
		result_ref = event_ref;
	}
	else {
		logger(LOG_LEVEL_DEBUG,	"%s [%u]: Consuming the current event. (%#X) (%#p)\n",
				__FUNCTION__, __LINE__, type, event_ref);
	}

	return result_ref;
}
