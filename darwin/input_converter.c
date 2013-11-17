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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// Reference: http://developer.apple.com/mac/library/documentation/Carbon/Reference/CarbonRefUpdate/Articles/Carbon_10.4-10.5_SymbolChanges.html
#include <ApplicationServices/ApplicationServices.h> // For CoreGraphics kCGEventFlagMask constants.
#include <nativehook.h>

#include "osx_input_helper.h"

unsigned int convert_to_virtual_key(unsigned int native_keycode) {
	unsigned int virtual_keycode = keycode_to_scancode(native_keycode);

	return virtual_keycode;
}

unsigned int convert_to_native_key(unsigned int virtual_keycode) {
	unsigned int native_keycode = scancode_to_keycode(virtual_keycode);

	return native_keycode;
}

unsigned int convert_to_virtual_button(unsigned int native_button) {
	unsigned int virtual_button = native_button;

	return virtual_button;
}

unsigned int convert_to_native_button(unsigned short int virtual_button) {
	unsigned int native_button = virtual_button;

	return native_button;
}

unsigned int convert_to_virtual_mask(unsigned int native_mask) {
	unsigned int virtual_mask = 0x0000;

	if (native_mask & kCGEventFlagMaskShift)		virtual_mask |= MASK_SHIFT_L;
	if (native_mask & kCGEventFlagMaskControl)		virtual_mask |= MASK_CTRL_L;
	if (native_mask & kCGEventFlagMaskCommand)		virtual_mask |= MASK_META_L;
	if (native_mask & kCGEventFlagMaskAlternate)	virtual_mask |= MASK_ALT_L;

	if (native_mask & kCGEventFlagMaskButtonLeft)	virtual_mask |= MASK_BUTTON1;
	if (native_mask & kCGEventFlagMaskButtonRight)	virtual_mask |= MASK_BUTTON2;
	if (native_mask & kCGEventFlagMaskButtonCenter)	virtual_mask |= MASK_BUTTON3;
	if (native_mask & kCGEventFlagMaskXButton1)		virtual_mask |= MASK_BUTTON4;
	if (native_mask & kCGEventFlagMaskXButton2)		virtual_mask |= MASK_BUTTON5;

	return virtual_mask;
}

unsigned short int convert_to_native_mask(unsigned short int virtual_mask) {
	unsigned int native_mask = 0x0000;

	if (virtual_mask & MASK_SHIFT_L)	native_mask |= kCGEventFlagMaskShift;
	if (virtual_mask & MASK_CTRL_L)		native_mask |= kCGEventFlagMaskControl;
	if (virtual_mask & MASK_META_L)		native_mask |= kCGEventFlagMaskCommand;
	if (virtual_mask & MASK_ALT_L)		native_mask |= kCGEventFlagMaskAlternate;

	if (virtual_mask & MASK_BUTTON1)	native_mask |= kCGEventFlagMaskButtonLeft;
	if (virtual_mask & MASK_BUTTON2)	native_mask |= kCGEventFlagMaskButtonRight;
	if (virtual_mask & MASK_BUTTON3)	native_mask |= kCGEventFlagMaskButtonCenter;
	if (virtual_mask & MASK_BUTTON4)	native_mask |= kCGEventFlagMaskXButton1;
	if (virtual_mask & MASK_BUTTON5)	native_mask |= kCGEventFlagMaskXButton2;

	return native_mask;
}
