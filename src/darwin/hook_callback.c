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
#include <limits.h>
#include <pthread.h>
#include <sys/time.h>
#include <uiohook.h>

#include "hook_callback.h"
#include "input_helper.h"
#include "logger.h"

// Modifiers for tracking key masks.
static uint16_t current_modifiers = 0x00000000;

// Required to transport messages between the main runloop and our thread for
// Unicode lookups.
typedef struct {
	CGEventRef event;
	UniChar buffer[2];
	UniCharCount length;
} TISMessage;

// Click count globals.
static unsigned short click_count = 0;
static CGEventTimestamp click_time = 0;
static unsigned short int click_button = MOUSE_NOBUTTON;
static bool mouse_dragged = false;

static CFRunLoopSourceRef src_msg_port;

// Structure for the current Unix epoch in milliseconds.
static struct timeval system_time;
static CGEventTimestamp previous_time = (CGEventTimestamp) ~0x00;
static uint64_t offset_time = 0;

// Virtual event pointer.
static uiohook_event event;

// Event dispatch callback.
static dispatcher_t dispatcher = NULL;

extern pthread_mutex_t hook_running_mutex, hook_control_mutex;
extern pthread_cond_t hook_control_cond;
extern Boolean restart_tap;

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

static inline uint64_t get_event_timestamp(CGEventRef event_ref) {
	// Grab the system uptime in NS.
	CGEventTimestamp hook_time = CGEventGetTimestamp(event_ref);

	// Convert time from NS to MS.
	hook_time /= 1000000;

	// Check for event clock reset.
	if (previous_time > hook_time) {
		// Get the local system time in UTC.
		gettimeofday(&system_time, NULL);

		// Convert the local system time to a Unix epoch in MS.
		uint64_t epoch_time = (system_time.tv_sec * 1000) + (system_time.tv_usec / 1000);

		// Calculate the offset based on the system and hook times.
		offset_time = epoch_time - hook_time;

		logger(LOG_LEVEL_INFO,	"%s [%u]: Resynchronizing event clock. (%" PRIu64 ")\n",
				__FUNCTION__, __LINE__, offset_time);
	}
	// Set the previous event time for click reset check above.
	previous_time = hook_time;

	// Set the event time to the server time + offset.
	return hook_time + offset_time;
}

static pthread_cond_t msg_port_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t msg_port_mutex = PTHREAD_MUTEX_INITIALIZER;

void message_port_status_proc(CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *info) {
	switch (activity) {
		case kCFRunLoopExit:
			// Acquire a lock on the msg_port and signal that anyone waiting
			// should continue.
			pthread_mutex_lock(&msg_port_mutex);
			pthread_cond_broadcast(&msg_port_cond);
			pthread_mutex_unlock(&msg_port_mutex);
			break;

		default:
			logger(LOG_LEVEL_WARN,	"%s [%u]: Unhandled RunLoop activity! (%#X)\n",
					__FUNCTION__, __LINE__, (unsigned int) activity);
			break;
	}
}

// Runloop to execute KeyCodeToString on the "Main" runloop due to an
// undocumented thread safety requirement.
static void message_port_proc(void *info) {
	// Lock the msg_port mutex as we enter the main runloop.
	pthread_mutex_lock(&msg_port_mutex);

	TISMessage *data = (TISMessage *) info;

	if (data != NULL && data->event != NULL) {
		// Preform Unicode lookup.
		data->length = keycode_to_unicode(data->event, data->buffer, sizeof(data->buffer));
	}

	// Unlock the msg_port mutex to signal to the hook_thread that we have
	// finished on the main runloop.
	pthread_cond_broadcast(&msg_port_cond);
	pthread_mutex_unlock(&msg_port_mutex);
}

static CFRunLoopObserverRef observer = NULL;
void start_message_port_runloop() {
	// Create a runloop observer for the main runloop.
	observer = CFRunLoopObserverCreate(
			kCFAllocatorDefault,
			kCFRunLoopExit, //kCFRunLoopEntry | kCFRunLoopExit, //kCFRunLoopAllActivities,
			true,
			0,
			message_port_status_proc,
			NULL
		);

	if (observer != NULL) {
		pthread_mutex_lock(&msg_port_mutex);

		// Initialize the TISMessage struct.
		TISMessage *data = (TISMessage *) malloc(sizeof(TISMessage));
		data->event = NULL;
		//data->buffer = { 0x00 };
		//data->length = 0;

		CFRunLoopSourceContext context = {
			.version			= 0,
			.info				= data,
			.retain				= NULL,
			.release			= NULL,
			.copyDescription	= NULL,
			.equal				= NULL,
			.hash				= NULL,
			.schedule			= NULL,
			.cancel				= NULL,
			.perform			= message_port_proc
		};

		CFRunLoopRef main_loop = CFRunLoopGetMain();

		src_msg_port = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &context);
		if (src_msg_port != NULL) {

			CFRunLoopAddSource(main_loop, src_msg_port, kCFRunLoopDefaultMode);

			CFRunLoopAddObserver(main_loop, observer, kCFRunLoopDefaultMode);

			logger(LOG_LEVEL_DEBUG, "%s [%u]: Successful.\n",
					__FUNCTION__, __LINE__);
		}
		else {
			logger(LOG_LEVEL_ERROR,	"%s [%u]: CFRunLoopSourceCreate failure!\n",
					__FUNCTION__, __LINE__);
		}

		pthread_mutex_unlock(&msg_port_mutex);
	}
	else {
		logger(LOG_LEVEL_ERROR,	"%s [%u]: CFRunLoopObserverCreate failure!\n",
				__FUNCTION__, __LINE__);
	}
}

void stop_message_port_runloop() {
	CFRunLoopRef main_loop = CFRunLoopGetMain();

	if (CFRunLoopContainsObserver(main_loop, observer, kCFRunLoopDefaultMode)) {
		CFRunLoopRemoveObserver(main_loop, observer, kCFRunLoopDefaultMode);
		CFRunLoopObserverInvalidate(observer);
	}

	if (CFRunLoopContainsSource(main_loop, src_msg_port, kCFRunLoopDefaultMode)) {
		CFRunLoopRemoveSource(main_loop, src_msg_port, kCFRunLoopDefaultMode);

		CFRunLoopSourceContext context = { .version = 0 };
		CFRunLoopSourceGetContext(src_msg_port, &context);
		if (context.info != NULL) {
			free(context.info);
		}

		CFRelease(src_msg_port);
	}

	observer = NULL;
	src_msg_port = NULL;
/*
	logger(LOG_LEVEL_DEBUG,	"%s [%u]: Successful.\n",
			__FUNCTION__, __LINE__);*/
}

void thread_start_proc() {
	// Get the local system time in UTC.
	gettimeofday(&system_time, NULL);

	// Convert the local system time to a Unix epoch in MS.
	uint64_t timestamp = (system_time.tv_sec * 1000) + (system_time.tv_usec / 1000);

	// Populate the hook start event.
	event.time = timestamp;
	event.reserved = 0x00;

	event.type = EVENT_THREAD_STARTED;
	event.mask = 0x00;

	// Fire the hook start event.
	dispatch_event(&event);
}

void thread_stop_proc() {
	// Get the local system time in UTC.
	gettimeofday(&system_time, NULL);

	// Convert the local system time to a Unix epoch in MS.
	uint64_t timestamp = (system_time.tv_sec * 1000) + (system_time.tv_usec / 1000);
	
	// Populate the hook stop event.
	event.time = timestamp;
	event.reserved = 0x00;

	event.type = EVENT_THREAD_STOPPED;
	event.mask = 0x00;

	// Fire the hook stop event.
	dispatch_event(&event);
}

void hook_status_proc(CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *info) {
	// Get the local system time in UTC.
	gettimeofday(&system_time, NULL);

	// Convert the local system time to a Unix epoch in MS.
	uint64_t timestamp = (system_time.tv_sec * 1000) + (system_time.tv_usec / 1000);

	switch (activity) {
		case kCFRunLoopEntry:
			logger(LOG_LEVEL_DEBUG,	"%s [%u]: Entering hook thread RunLoop.\n",
					__FUNCTION__, __LINE__);

			// Lock the running mutex to signal the hook has started.
			pthread_mutex_lock(&hook_running_mutex);

			// Populate the hook start event.
			event.time = timestamp;
			event.reserved = 0x00;

			event.type = EVENT_HOOK_ENABLED;
			event.mask = 0x00;

			// Fire the hook start event.
			dispatch_event(&event);

			// Unlock the control mutex so hook_enable() can continue.
			pthread_cond_signal(&hook_control_cond);
			pthread_mutex_unlock(&hook_control_mutex);
			break;

		case kCFRunLoopExit:
			logger(LOG_LEVEL_DEBUG,	"%s [%u]: Exiting hook thread RunLoop.\n",
					__FUNCTION__, __LINE__);
			
			// Lock the control mutex until we exit.
			pthread_mutex_lock(&hook_control_mutex);

			// Unlock the running mutex to signal the hook has stopped.
			pthread_mutex_unlock(&hook_running_mutex);

			// Populate the hook stop event.
			event.time = timestamp;
			event.reserved = 0x00;

			event.type = EVENT_HOOK_DISABLED;
			event.mask = 0x00;

			// Fire the hook stop event.
			dispatch_event(&event);
			break;

		default:
			logger(LOG_LEVEL_WARN,	"%s [%u]: Unhandled RunLoop activity! (%#X)\n",
					__FUNCTION__, __LINE__, (unsigned int) activity);
			break;
	}
}

static inline void process_key_pressed(uint64_t timestamp, CGEventRef event_ref) {
	// Populate key pressed event.
	event.time = timestamp;
	event.reserved = 0x00;

	event.type = EVENT_KEY_PRESSED;
	event.mask = get_modifiers();

	// FIXME Check for overflow.
	UInt64 keycode = CGEventGetIntegerValueField(event_ref, kCGKeyboardEventKeycode);
	event.data.keyboard.keycode = keycode_to_scancode(keycode);
	event.data.keyboard.rawcode = keycode;
	event.data.keyboard.keychar = CHAR_UNDEFINED;

	logger(LOG_LEVEL_INFO,	"%s [%u]: Key %#X pressed. (%#X)\n",
			__FUNCTION__, __LINE__, event.data.keyboard.keycode, event.data.keyboard.rawcode);
	
	// Fire key pressed event.
	dispatch_event(&event);

	// If the pressed event was not consumed...
	if (event.reserved ^ 0x01) {
		// Lock for code dealing with the main runloop.
		pthread_mutex_lock(&msg_port_mutex);

		// Check to see if the main runloop is still running.
		// TOOD I would rather this be a check on hook_enable(),
		// but it makes the usage complicated by requiring a separate 
		// thread for the main runloop and hook registration.
		CFStringRef mode = CFRunLoopCopyCurrentMode(CFRunLoopGetMain());
		if (mode != NULL) {
			CFRelease(mode);

			// Lookup the Unicode representation for this event.
			CFRunLoopSourceContext context = { .version = 0 };
			CFRunLoopSourceGetContext(src_msg_port, &context);

			// Get the run loop context info pointer.
			TISMessage *info = (TISMessage *) context.info;

			// Set the event pointer.
			info->event = event_ref;

			// Signal the custom source and wakeup the main runloop.
			CFRunLoopSourceSignal(src_msg_port);
			CFRunLoopWakeUp(CFRunLoopGetMain());

			// Wait for a lock while the main run loop processes they key typed event.
			pthread_cond_wait(&msg_port_cond, &msg_port_mutex);

			for (unsigned int i = 0; i < info->length; i++) {
				// Populate key typed event.
				event.time = timestamp;
				event.reserved = 0x00;

				event.type = EVENT_KEY_TYPED;
				event.mask = get_modifiers();

				event.data.keyboard.keycode = VC_UNDEFINED;
				event.data.keyboard.rawcode = keycode;
				event.data.keyboard.keychar = info->buffer[i];

				logger(LOG_LEVEL_INFO,	"%s [%u]: Key %#X typed. (%lc)\n",
						__FUNCTION__, __LINE__, event.data.keyboard.keycode, 
						(wint_t) event.data.keyboard.keychar);
				
				// Populate key typed event.
				dispatch_event(&event);
			}

			info->event = NULL;
			info->length = 0;
		}
		else {
			logger(LOG_LEVEL_WARN,	"%s [%u]: Failed to signal RunLoop main!\n",
					__FUNCTION__, __LINE__);
		}

		// Maintain a lock until we pass all code dealing with TISMessage *info.
		pthread_mutex_unlock(&msg_port_mutex);
	}
}

static inline void process_key_released(uint64_t timestamp, CGEventRef event_ref) {
	// Populate key released event.
	event.time = timestamp;
	event.reserved = 0x00;
	
	event.type = EVENT_KEY_RELEASED;
	event.mask = get_modifiers();

	// FIXME Check for overflow.
	UInt64 keycode = CGEventGetIntegerValueField(event_ref, kCGKeyboardEventKeycode);
	event.data.keyboard.keycode = keycode_to_scancode(keycode);
	event.data.keyboard.rawcode = keycode;
	event.data.keyboard.keychar = CHAR_UNDEFINED;

	logger(LOG_LEVEL_INFO,	"%s [%u]: Key %#X released. (%#X)\n",
			__FUNCTION__, __LINE__, event.data.keyboard.keycode, event.data.keyboard.rawcode);
	
	// Fire key released event.
	dispatch_event(&event);
}

static inline void process_modifier_changed(uint64_t timestamp, CGEventRef event_ref) {
	CGEventFlags event_mask = CGEventGetFlags(event_ref);
	
	logger(LOG_LEVEL_INFO,	"%s [%u]: Modifiers Changed. (%#X)\n",
					__FUNCTION__, __LINE__, (unsigned int) event_mask);

	/* Because Apple treats modifier keys differently than normal key
	 * events, any changes to the modifier keys will require a key state
	 * change to be fired manually.
	 */
	UInt64 keycode = CGEventGetIntegerValueField(event_ref, kCGKeyboardEventKeycode);
	uint8_t modifier_flag = 0x0000;
	if		(keycode == kVK_Shift)			{ modifier_flag = MASK_SHIFT_L;	}
	else if (keycode == kVK_Control)		{ modifier_flag = MASK_CTRL_L;	}
	else if (keycode == kVK_Command)		{ modifier_flag = MASK_META_L;	}
	else if (keycode == kVK_Option)			{ modifier_flag = MASK_ALT_L;	}
	else if (keycode == kVK_RightShift)		{ modifier_flag = MASK_SHIFT_R;	}
	else if (keycode == kVK_RightControl)	{ modifier_flag = MASK_CTRL_R;	}
	else if (keycode == kVK_RightCommand)	{ modifier_flag = MASK_META_R;	}
	else if (keycode == kVK_RightOption)	{ modifier_flag = MASK_ALT_R;	}

	// First check to see if a modifier we care about changed.
	if (modifier_flag != 0x0000) {
		if (get_modifiers() & modifier_flag) {
			unset_modifier_mask(modifier_flag);

			// Process as a key released event.
			process_key_released(timestamp, event_ref);
		}
		else {
			set_modifier_mask(modifier_flag);

			// Process as a key pressed event.
			process_key_pressed(timestamp, event_ref);
		}
	}
}

static inline void process_button_pressed(uint64_t timestamp, CGEventRef event_ref, uint16_t button) {
	// Track the number of clicks.
	if (button == click_button && (long int) (timestamp - click_time) <= hook_get_multi_click_time()) {
		if (click_count < USHRT_MAX) {
			click_count++;
		}
		else {
			logger(LOG_LEVEL_WARN, "%s [%u]: Click count overflow detected!\n",
					__FUNCTION__, __LINE__);
		}
	}
	else {
		// Reset the click count.
		click_count = 1;

		// Set the previous button.
		click_button = button;
	}

	// Save this events time to calculate the click_count.
	click_time = timestamp;

	CGPoint event_point = CGEventGetLocation(event_ref);

	// Populate mouse pressed event.
	event.time = timestamp;
	event.reserved = 0x00;

	event.type = EVENT_MOUSE_PRESSED;
	event.mask = get_modifiers();

	event.data.mouse.button = button;
	event.data.mouse.clicks = click_count;
	event.data.mouse.x = event_point.x;
	event.data.mouse.y = event_point.y;

	logger(LOG_LEVEL_INFO,	"%s [%u]: Button %u pressed %u time(s). (%u, %u)\n",
			__FUNCTION__, __LINE__, event.data.mouse.button, event.data.mouse.clicks,
			event.data.mouse.x, event.data.mouse.y);
	
	// Fire mouse pressed event.
	dispatch_event(&event);
}

static inline void process_button_released(uint64_t timestamp, CGEventRef event_ref, uint16_t button) {
	CGPoint event_point = CGEventGetLocation(event_ref);

	// Populate mouse released event.
	event.time = timestamp;
	event.reserved = 0x00;
	
	event.type = EVENT_MOUSE_RELEASED;
	event.mask = get_modifiers();

	event.data.mouse.button = button;
	event.data.mouse.clicks = click_count;
	event.data.mouse.x = event_point.x;
	event.data.mouse.y = event_point.y;

	logger(LOG_LEVEL_INFO,	"%s [%u]: Button %u released %u time(s). (%u, %u)\n",
			__FUNCTION__, __LINE__, event.data.mouse.button, event.data.mouse.clicks,
			event.data.mouse.x, event.data.mouse.y);
	
	// Fire mouse released event.
	dispatch_event(&event);

	// FIXME Conusmed shouldn't fire.
	if (mouse_dragged != true) {
		// Populate mouse clicked event.
		event.time = timestamp;
		event.reserved = 0x00;
	
		event.type = EVENT_MOUSE_CLICKED;
		event.mask = get_modifiers();

		event.data.mouse.button = button;
		event.data.mouse.clicks = click_count;
		event.data.mouse.x = event_point.x;
		event.data.mouse.y = event_point.y;

		logger(LOG_LEVEL_INFO,	"%s [%u]: Button %u clicked %u time(s). (%u, %u)\n",
				__FUNCTION__, __LINE__, event.data.mouse.button, event.data.mouse.clicks,
				event.data.mouse.x, event.data.mouse.y);
		
		// Fire mouse clicked event.
		dispatch_event(&event);
	}	
}

static inline void process_mouse_moved(uint64_t timestamp, CGEventRef event_ref) {
	CGPoint event_point = CGEventGetLocation(event_ref);

	// Reset the click count.
	if (click_count != 0 && (long int) (event.time - click_time) > hook_get_multi_click_time()) {
		click_count = 0;
	}

	// Populate mouse motion event.
	event.time = timestamp;
	event.reserved = 0x00;
	
	if (mouse_dragged) {
		event.type = EVENT_MOUSE_DRAGGED;
	}
	else {
		event.type = EVENT_MOUSE_MOVED;
	}
	event.mask = get_modifiers();

	event.data.mouse.button = MOUSE_NOBUTTON;
	event.data.mouse.clicks = click_count;
	event.data.mouse.x = event_point.x;
	event.data.mouse.y = event_point.y;

	// FIXME moved OR dragged 
	logger(LOG_LEVEL_INFO,	"%s [%u]: Mouse dragged to %u, %u.\n",
			__FUNCTION__, __LINE__, event.data.mouse.x, event.data.mouse.y);

	// Fire mouse motion event.
	dispatch_event(&event);
}

static inline void process_mouse_wheel(uint64_t timestamp, CGEventRef event_ref) {
	// Check to see what axis was rotated, we only care about axis 1 for vertical rotation.
	// TODO Implement horizontal scrolling by examining axis 2.
	// NOTE kCGScrollWheelEventDeltaAxis3 is currently unused.
	if (CGEventGetIntegerValueField(event_ref, kCGScrollWheelEventDeltaAxis1) != 0) {
		CGPoint event_point = CGEventGetLocation(event_ref);

		// Track the number of clicks.
		if ((long int) (event.time - click_time) <= hook_get_multi_click_time()) {
			click_count++;
		}
		else {
			click_count = 1;
		}
		click_time = event.time;


		// Populate mouse wheel event.
		event.time = timestamp;
		event.reserved = 0x00;
		
		event.type = EVENT_MOUSE_WHEEL;
		event.mask = get_modifiers();

		event.data.wheel.clicks = click_count;
		event.data.wheel.x = event_point.x;
		event.data.wheel.y = event_point.y;

		// TODO Figure out if kCGScrollWheelEventDeltaAxis2 causes mouse events with zero rotation.
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

		logger(LOG_LEVEL_INFO,	"%s [%u]: Mouse wheel type %u, rotated %i units at %u, %u.\n",
				__FUNCTION__, __LINE__, event.data.wheel.type, event.data.wheel.amount * event.data.wheel.rotation,
				event.data.wheel.x, event.data.wheel.y);

		// Fire mouse wheel event.
		dispatch_event(&event);
	}
}

CGEventRef hook_event_proc(CGEventTapProxy tap_proxy, CGEventType type, CGEventRef event_ref, void *refcon) {
	// Calculate Unix epoch from native time source.
	uint64_t timestamp = get_event_timestamp(event_ref);

	// Get the event class.
	switch (type) {
		case kCGEventKeyDown:
			process_key_pressed(timestamp, event_ref);
			break;

		case kCGEventKeyUp:
			process_key_released(timestamp, event_ref);
			break;

		case kCGEventFlagsChanged:
			process_modifier_changed(timestamp, event_ref);
			break;

		case kCGEventLeftMouseDown:
			set_modifier_mask(MASK_BUTTON1);
			process_button_pressed(timestamp, event_ref, MOUSE_BUTTON1);
			break;
			
		case kCGEventRightMouseDown:
			set_modifier_mask(MASK_BUTTON2);
			process_button_pressed(timestamp, event_ref, MOUSE_BUTTON2);
			break;
			
		case kCGEventOtherMouseDown:
			// Extra mouse buttons.
			if (CGEventGetIntegerValueField(event_ref, kCGMouseEventButtonNumber) < UINT16_MAX) {
				uint16_t button = (uint16_t) CGEventGetIntegerValueField(event_ref, kCGMouseEventButtonNumber) + 1;

				if (button + 7 < 16) {
					set_modifier_mask(1 << (button + 7));
				}
				
				process_button_pressed(timestamp, event_ref, button);
			}
			break;

		case kCGEventLeftMouseUp:
			unset_modifier_mask(MASK_BUTTON1);
			process_button_released(timestamp, event_ref, MOUSE_BUTTON1);
			break;
			
		case kCGEventRightMouseUp:
			unset_modifier_mask(MASK_BUTTON2);
			process_button_released(timestamp, event_ref, MOUSE_BUTTON2);
			break;			
			
		case kCGEventOtherMouseUp:
			// Extra mouse buttons.
			if (CGEventGetIntegerValueField(event_ref, kCGMouseEventButtonNumber) < UINT16_MAX) {
				uint16_t button = (uint16_t) CGEventGetIntegerValueField(event_ref, kCGMouseEventButtonNumber) + 1;

				if (button + 7 < 16) {
					set_modifier_mask(1 << (button + 7));
				}
				
				process_button_pressed(timestamp, event_ref, button);
			}
			break;


		case kCGEventLeftMouseDragged:
		case kCGEventRightMouseDragged:
		case kCGEventOtherMouseDragged:
			// FIXME The drag flag is confusing.  Use prev x,y to determine click.
			// Set the mouse dragged flag.
			mouse_dragged = true;
			process_mouse_moved(timestamp, event_ref);
			break;
			
		case kCGEventMouseMoved:
			// Set the mouse dragged flag.
			mouse_dragged = false;
			process_mouse_moved(timestamp, event_ref);
			break;

			
		case kCGEventScrollWheel:
			process_mouse_wheel(timestamp, event_ref);
			break;


		#ifdef USE_DEBUG
		case kCGEventNull:
			logger(LOG_LEVEL_DEBUG, "%s [%u]: Ignoring kCGEventNull.\n",
					__FUNCTION__, __LINE__);
			break;
		#endif

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
