/* libUIOHook: Cross-platfrom userland keyboard and mouse hooking.
 * Copyright (C) 2006-2015 Alexander Barker.  All Rights Received.
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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <uiohook.h>

#include "input_helper.h"
#include "logger.h"

// TODO Possibly relocate to input helper.
static inline CGEventFlags get_key_event_mask(uiohook_event * const event) {
	CGEventFlags native_mask = 0x00;

	if (event->mask & (MASK_SHIFT))	{ native_mask |= kCGEventFlagMaskShift;		}
	if (event->mask & (MASK_CTRL))	{ native_mask |= kCGEventFlagMaskControl;	}
	if (event->mask & (MASK_META))	{ native_mask |= kCGEventFlagMaskControl;	}
	if (event->mask & (MASK_ALT))	{ native_mask |= kCGEventFlagMaskAlternate;	}
	
	if (event->type == EVENT_KEY_PRESSED || event->type == EVENT_KEY_RELEASED || event->type == EVENT_KEY_TYPED) {
		switch (event->data.keyboard.keycode) {
			case VC_KP_0:
			case VC_KP_1:
			case VC_KP_2:
			case VC_KP_3:
			case VC_KP_4:
			case VC_KP_5:
			case VC_KP_6:
			case VC_KP_7:
			case VC_KP_8:
			case VC_KP_9:

			case VC_NUM_LOCK:
			case VC_KP_ENTER:
			case VC_KP_MULTIPLY:
			case VC_KP_ADD:
			case VC_KP_SEPARATOR:
			case VC_KP_SUBTRACT:
			case VC_KP_DIVIDE:
			case VC_KP_COMMA:
				native_mask |= kCGEventFlagMaskNumericPad;
				break;	
		}
	}

	return native_mask;
}

UIOHOOK_API void hook_post_event(uiohook_event * const event) {
	CGEventRef cg_event;

	CGEventTapLocation loc = kCGHIDEventTap; // kCGSessionEventTap also works.
	CGEventSourceRef src = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);

	switch (event->type) {
		case EVENT_KEY_PRESSED:
			cg_event = CGEventCreateKeyboardEvent(src,
					(CGKeyCode) scancode_to_keycode(event->data.keyboard.keycode),
					true);
			
			CGEventSetFlags(cg_event, get_key_event_mask(event));
			CGEventPost(loc, cg_event);
			CFRelease(cg_event);
			break;

		case EVENT_KEY_RELEASED:
			cg_event = CGEventCreateKeyboardEvent(src,
					(CGKeyCode) scancode_to_keycode(event->data.keyboard.keycode),
					false);
			
			CGEventSetFlags(cg_event, get_key_event_mask(event));
			CGEventPost(loc, cg_event);
			CFRelease(cg_event);
			break;

			
		case EVENT_MOUSE_PRESSED:
			if (event->data.mouse.button == MOUSE_BUTTON1) {
				cg_event = CGEventCreateMouseEvent(src,
					kCGEventLeftMouseDown,
					CGPointMake(
						(CGFloat) event->data.mouse.x,
						(CGFloat) event->data.mouse.y
					),
					kCGMouseButtonLeft
				);
				
				CGEventPost(loc, cg_event);
				CFRelease(cg_event);
			}
			else if (event->data.mouse.button == MOUSE_BUTTON2) {
				cg_event = CGEventCreateMouseEvent(src,
					kCGEventRightMouseDown,
					CGPointMake(
						(CGFloat) event->data.mouse.x,
						(CGFloat) event->data.mouse.y
					),
					kCGMouseButtonRight
				);
				
				CGEventPost(loc, cg_event);
				CFRelease(cg_event);
			}
			else if (event->data.mouse.button > 0) {
				cg_event = CGEventCreateMouseEvent(src,
					kCGEventOtherMouseDown,
					CGPointMake(
						(CGFloat) event->data.mouse.x,
						(CGFloat) event->data.mouse.y
					),
					event->data.mouse.button - 1
				);
				CGEventPost(loc, cg_event);
				CFRelease(cg_event);
			}
			break;
						
		case EVENT_MOUSE_RELEASED:
			if (event->data.mouse.button == MOUSE_BUTTON1) {
				cg_event = CGEventCreateMouseEvent(src,
					kCGEventLeftMouseUp,
					CGPointMake(
						(CGFloat) event->data.mouse.x,
						(CGFloat) event->data.mouse.y
					),
					kCGMouseButtonLeft
				);
				CGEventPost(loc, cg_event);
				CFRelease(cg_event);
			}
			else if (event->data.mouse.button == MOUSE_BUTTON2) {
				cg_event = CGEventCreateMouseEvent(src,
					kCGEventRightMouseUp,
					CGPointMake(
						(CGFloat) event->data.mouse.x,
						(CGFloat) event->data.mouse.y
					),
					kCGMouseButtonRight
				);
				CGEventPost(loc, cg_event);
				CFRelease(cg_event);
			}
			else if (event->data.mouse.button > 0) {
				cg_event = CGEventCreateMouseEvent(src,
					kCGEventOtherMouseUp,
					CGPointMake(
						(CGFloat) event->data.mouse.x,
						(CGFloat) event->data.mouse.y
					),
					event->data.mouse.button - 1
				);
				CGEventPost(loc, cg_event);
				CFRelease(cg_event);
			}
			break;


		case EVENT_MOUSE_WHEEL:
			// FIXME Should I create a source event with the coords?
			// It seems to use automagically the current location of the cursor.
			// Two options: Query the mouse, move it to x/y, scroll, then move back 
			// OR disable x/y for scroll events on Windows & X11.
			if (event->data.wheel.type == WHEEL_BLOCK_SCROLL) {
				// Scrolling data is line-based.
				cg_event = CGEventCreateScrollWheelEvent(src,
					kCGScrollEventUnitLine,
					// TODO Currently only support 1 wheel axis.
					(CGWheelCount) 1, // 1 for Y-only, 2 for Y-X, 3 for Y-X-Z
					event->data.wheel.amount * event->data.wheel.rotation);
			}
			else {
				// Scrolling data is pixel-based.
				cg_event = CGEventCreateScrollWheelEvent(src,
					kCGScrollEventUnitPixel,
					// TODO Currently only support 1 wheel axis.
					(CGWheelCount) 1, // 1 for Y-only, 2 for Y-X, 3 for Y-X-Z
					event->data.wheel.amount * event->data.wheel.rotation);
			}
			CGEventPost(loc, cg_event);
			CFRelease(cg_event);
			break;


		case EVENT_MOUSE_MOVED:
		case EVENT_MOUSE_DRAGGED:
			if (event->mask >> 8 == 0x00) {
				// No mouse flags.
				cg_event = CGEventCreateMouseEvent(src,
					kCGEventMouseMoved,
					CGPointMake(
						(CGFloat) event->data.mouse.x,
						(CGFloat) event->data.mouse.y
					),
					0
				);
			}
			else if (event->mask & MASK_BUTTON1) {
				cg_event = CGEventCreateMouseEvent(src,
					kCGEventLeftMouseDragged,
					CGPointMake(
						(CGFloat) event->data.mouse.x,
						(CGFloat) event->data.mouse.y
					),
					kCGMouseButtonLeft
				);
			}
			else if (event->mask & MASK_BUTTON2) {
				cg_event = CGEventCreateMouseEvent(src,
					kCGEventRightMouseDragged,
					CGPointMake(
						(CGFloat) event->data.mouse.x,
						(CGFloat) event->data.mouse.y
					),
					kCGMouseButtonRight
				);
			}
			else {
				cg_event = CGEventCreateMouseEvent(src,
					kCGEventOtherMouseDragged,
					CGPointMake(
						(CGFloat) event->data.mouse.x,
						(CGFloat) event->data.mouse.y
					),
					(event->mask >> 8) - 1
				);
			}
			CGEventPost(loc, cg_event);
			CFRelease(cg_event);
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

	CFRelease(src);
}
