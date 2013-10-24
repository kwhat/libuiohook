/* JNativeHook: Global keyboard and mouse hooking for Java.
 * Copyright (C) 2006-2013 Alexander Barker.  All Rights Received.
 * http://code.google.com/p/jnativehook/
 *
 * JNativeHook is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * JNativeHook is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <nativehook.h>

#include <ApplicationServices/ApplicationServices.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "input_converter.h"

static unsigned int btnmask_lookup[10] = {
	kCGEventLeftMouseDown,	// Button 1
	kCGEventRightMouseDown	// Button 2
	kCGEventOtherMouseDown,	// Button 3
	kCGEventOtherMouseDown,	// Button 4
	kCGEventOtherMouseDown,	// Button 5
	
	kCGEventLeftMouseUp,	// Button 1
	kCGEventRightMouseUp	// Button 2
	kCGEventOtherMouseUp,	// Button 3
	kCGEventOtherMouseUp,	// Button 4
	kCGEventOtherMouseUp,	// Button 5
};


NATIVEHOOK_API void hook_post_event(virtual_event * const event) {
	CGEventRef cg_event = NULL;
	
	switch (event->type) {
		case EVENT_KEY_TYPED:

		case EVENT_KEY_PRESSED:
			cg_event = CGEventCreateKeyboardEvent(NULL, 
					(CGKeyCode) convert_to_native_key(event->data.keyboard.keycode), 
					true);
			CGEventSetFlags(cg_event, (CGEventFlags) 0x00);
			
			if (event->type == EVENT_KEY_PRESSED) {
				break;
			}

		case EVENT_KEY_RELEASED:
			cg_event = CGEventCreateKeyboardEvent(NULL, 
					(CGKeyCode) convert_to_native_key(event->data.keyboard.keycode), 
					true);
			CGEventSetFlags(event, (CGEventFlags) 0x00);
			break;


		case EVENT_MOUSE_CLICKED:
			
		case EVENT_MOUSE_PRESSED:
			CGEventCreateMouseEvent(NULL, 
					btnmask_lookup[event->data.mouse.button - 1],
					CGPointMake(
						(CGFloat) event->data.mouse.x,
						(CGFloat) event->data.mouse.y
					),
					event->data.mouse.button - 1
			);
			
			if (event->type == EVENT_MOUSE_PRESSED) {
				break;
			}
		case EVENT_MOUSE_RELEASED:
			CGEventCreateMouseEvent(NULL, 
					btnmask_lookup[event->data.mouse.button - 1],
					CGPointMake(
						(CGFloat) event->data.mouse.x,
						(CGFloat) event->data.mouse.y
					),
					event->data.mouse.button - 1
			);
			break;
			
		case EVENT_MOUSE_MOVED:
			CGEventCreateMouseEvent(NULL, 
					kCGEventMouseMoved,
					CGPointMake(
						(CGFloat) event->data.mouse.x,
						(CGFloat) event->data.mouse.y
					),
					0
			);
			break;
			
		case EVENT_MOUSE_DRAGGED:
			//kCGEventLeftMouseDragged
			//kCGEventRightMouseDragged
			//kCGEventOtherMouseDragged
			break;
					
		case EVENT_MOUSE_WHEEL:
			/*
			CGEventCreateScrollWheelEvent(NULL, 
					CGScrollEventUnit units,
					CGWheelCount wheelCount,
					int32_t wheel1)
			*/
			break;
	}
	
	CGEventSetFlags(cg_event, (CGEventFlags) 0x00);
	
	CFRelease(cg_event); 
}
