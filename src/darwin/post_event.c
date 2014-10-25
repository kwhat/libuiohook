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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <uiohook.h>

#include "input_helper.h"

//https://developer.apple.com/library/mac/documentation/Carbon/Reference/
//QuartzEventServicesRef/index.html#//apple_ref/swift/tdef/CGMouseButton
CGMouseButton toCarbonMouseButton( int evenMouseButton ){
    CGMouseButton cgMButton = -1;
	switch( evenMouseButton ){
		case MOUSE_LEFT:
			cgMButton = kCGMouseButtonLeft;
			break;
		case MOUSE_RIGHT:
			cgMButton = kCGMouseButtonRight;
			break;
		case MOUSE_MIDDLE:
			cgMButton = kCGMouseButtonCenter;
			break;
	}
	return cgMButton;
}

UIOHOOK_API void hook_post_event(uiohook_event * const event) {
	CGEventRef cg_event = NULL;
	CGEventType cg_event_type = kCGEventNull;
	CGScrollEventUnit cg_event_unit;

	CGEventSourceRef src = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
	//CGEventSourceRef src = NULL;

	switch (event->type) {
		case EVENT_KEY_TYPED:

		case EVENT_KEY_PRESSED:
			cg_event = CGEventCreateKeyboardEvent( src,
					(CGKeyCode) scancode_to_keycode(event->data.keyboard.keycode),
					true);
			CGEventSetFlags(cg_event, (CGEventFlags) 0x00);
			//CGEventSetFlags(cg_event, kCGEventFlagMaskCommand);
  
			if (event->type == EVENT_KEY_PRESSED) {
				break;
			}

		case EVENT_KEY_RELEASED:
			cg_event = CGEventCreateKeyboardEvent( src,
					(CGKeyCode) scancode_to_keycode(event->data.keyboard.keycode),
					false);
			CGEventSetFlags(cg_event, (CGEventFlags) 0x00);
			break;


		case EVENT_MOUSE_CLICKED:

		case EVENT_MOUSE_PRESSED:
			if (event->data.mouse.button == MOUSE_NOBUTTON) {
				cg_event_type = kCGEventNull;
			}
			else if (event->data.mouse.button == MOUSE_BUTTON1) {
				cg_event_type = kCGEventLeftMouseDown;
			}
			else if (event->data.mouse.button == MOUSE_BUTTON2) {
				cg_event_type = kCGEventRightMouseDown;
			}
			else {
				cg_event_type = kCGEventOtherMouseDown;
			}

			cg_event = CGEventCreateMouseEvent( src,
					cg_event_type,
					CGPointMake(
						(CGFloat) event->data.mouse.x,
						(CGFloat) event->data.mouse.y
					),
					toCarbonMouseButton( event->data.mouse.button )
			);

			if (event->type == EVENT_MOUSE_PRESSED) {
				break;
			}
		case EVENT_MOUSE_RELEASED:
			if (event->data.mouse.button == MOUSE_NOBUTTON) {
				cg_event_type = kCGEventNull;
			}
			else if (event->data.mouse.button == MOUSE_BUTTON1) {
				cg_event_type = kCGEventLeftMouseUp;
			}
			else if (event->data.mouse.button == MOUSE_BUTTON2) {
				cg_event_type = kCGEventRightMouseUp;
			}
			else {
				cg_event_type = kCGEventOtherMouseUp;
			}

			cg_event = CGEventCreateMouseEvent( src,
					cg_event_type,
					CGPointMake(
						(CGFloat) event->data.mouse.x,
						(CGFloat) event->data.mouse.y
					),
					toCarbonMouseButton( event->data.mouse.button )
			);
			break;

		case EVENT_MOUSE_MOVED:

			cg_event = CGEventCreateMouseEvent( src,
					kCGEventMouseMoved,
					CGPointMake(
						(CGFloat) event->data.mouse.x,
						(CGFloat) event->data.mouse.y
					),
					0
			);
			break;

		case EVENT_MOUSE_DRAGGED:
			if (event->data.mouse.button == MOUSE_NOBUTTON) {
				cg_event_type = kCGEventMouseMoved;
			}
			else if (event->data.mouse.button == MOUSE_BUTTON1) {
				cg_event_type = kCGEventLeftMouseDragged;
			}
			else if (event->data.mouse.button == MOUSE_BUTTON2) {
				cg_event_type = kCGEventRightMouseDragged;
			}
			else {
				cg_event_type = kCGEventOtherMouseDragged;
			}
			
			cg_event = CGEventCreateMouseEvent( src,
					cg_event_type,
					CGPointMake(
						(CGFloat) event->data.mouse.x,
						(CGFloat) event->data.mouse.y
					),
					toCarbonMouseButton( event->data.mouse.button )
			);
			break;

		case EVENT_MOUSE_WHEEL:
			if (event->data.wheel.type == WHEEL_BLOCK_SCROLL) {
				// Scrolling data is line-based.
				cg_event_unit = kCGScrollEventUnitLine;
			}
			else {
				// Scrolling data is pixel-based.
				cg_event_unit = kCGScrollEventUnitPixel;
			}

			//TODO: Should I create a source event with the coords?
			//It seems to use automagically the current location of the cursor
			//(CGFloat) event->data.wheel.x,
			//(CGFloat) event->data.wheel.y			
			cg_event = CGEventCreateScrollWheelEvent( src,
					cg_event_unit,
					(CGWheelCount) 1, // TODO Currently only support 1 wheel axis.
					event->data.wheel.amount * event->data.wheel.rotation);
			break;

		default:
		break;
	}

	//CGEventSetFlags(cg_event, (CGEventFlags) 0x00);
	
	//CGEventSetFlags(cg_event, kCGEventFlagMaskCommand);

    CGEventTapLocation loc = kCGHIDEventTap; // kCGSessionEventTap also works
    CGEventPost(loc, cg_event);

	CFRelease(cg_event);
	CFRelease(src);
}
