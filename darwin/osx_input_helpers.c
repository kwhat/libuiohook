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

#include <stdbool.h>
#include "osx_input_helpers.h"
#ifdef COREFOUNDATION
#include <CoreFoundation/CoreFoundation.h>
#endif

// Current dead key state.
#if defined(CARBON_LEGACY) || defined(COREFOUNDATION)
static UInt32 curr_deadkey_state = 0;
#endif

// Input source data for the keyboard.
#if defined(CARBON_LEGACY)
static KeyboardLayoutRef prev_keyboard_layout = NULL;
#elif defined(COREFOUNDATION)
static TISInputSourceRef prev_keyboard_layout = NULL;
#endif

// This method must be executed from the main runloop to avoid the seemingly random
// Exception detected while handling key input.  TSMProcessRawKeyCode failed (-192) errors.
// CFEqual(CFRunLoopGetCurrent(), CFRunLoopGetMain())
void keycode_to_string(CGEventRef event, UniCharCount size, UniCharCount *length, UniChar *buffer) {
	#if defined(CARBON_LEGACY) || defined(COREFOUNDATION)
	#if defined(CARBON_LEGACY)
	KeyboardLayoutRef curr_keyboard_layout;
	void *inputData = NULL;
	if (KLGetCurrentKeyboardLayout(&curr_keyboard_layout) == noErr) {
		if (KLGetKeyboardLayoutProperty(curr_keyboard_layout, kKLuchrData, (const void **) &inputData) != noErr) {
			inputData = NULL;
		}
	}
	#elif defined(COREFOUNDATION)
	TISInputSourceRef curr_keyboard_layout = TISCopyCurrentKeyboardLayoutInputSource();
	CFDataRef inputData = NULL;
	if (curr_keyboard_layout != NULL && CFGetTypeID(curr_keyboard_layout) == TISInputSourceGetTypeID()) {
		CFDataRef data = (CFDataRef) TISGetInputSourceProperty(curr_keyboard_layout, kTISPropertyUnicodeKeyLayoutData);
		if (data != NULL && CFGetTypeID(data) == CFDataGetTypeID() && CFDataGetLength(data) > 0) {
			inputData = (CFDataRef) data;
		}
	}

	// Check if the keyboard layout has changed to see if the dead key state needs to be discarded.
	if (prev_keyboard_layout != NULL && curr_keyboard_layout != NULL && CFEqual(curr_keyboard_layout, prev_keyboard_layout) == false) {
		curr_deadkey_state = 0;
	}

	// Release the previous keyboard layout.
	if (prev_keyboard_layout != NULL) {
		CFRelease(prev_keyboard_layout);
	}

	// Set the previous keyboard layout to the current layout.
	if (curr_keyboard_layout != NULL) {
		prev_keyboard_layout = curr_keyboard_layout;
	}
	#endif

	if (inputData != NULL) {
		#ifdef CARBON_LEGACY
		const UCKeyboardLayout *keyboard_layout = (const UCKeyboardLayout *) inputData;
		#else
		const UCKeyboardLayout *keyboard_layout = (const UCKeyboardLayout*) CFDataGetBytePtr(inputData);
		#endif

		if (keyboard_layout != NULL) {
			//Extract keycode and modifier information.
			CGKeyCode keycode = CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
			CGEventFlags modifiers = CGEventGetFlags(event);

			// Disable all command modifiers for translation.  This is required
			// so UCKeyTranslate will provide a keysym for the separate event.
			static const CGEventFlags cmd_modifiers = kCGEventFlagMaskCommand | 
					kCGEventFlagMaskControl | kCGEventFlagMaskAlternate;
			modifiers &= ~cmd_modifiers;

			// I don't know why but UCKeyTranslate does not process the
			// kCGEventFlagMaskAlphaShift (A.K.A. Caps Lock Mask) correctly.
			// We need to basically turn off the mask and process the capital
			// letters after UCKeyTranslate().  Think Different, not because it
			// makes sense but because you want to be a hipster.
			bool is_caps_lock = modifiers & kCGEventFlagMaskAlphaShift;
			modifiers &= ~kCGEventFlagMaskAlphaShift;


			OSStatus status = noErr;
			if (curr_deadkey_state == 0) {
				// No previous deadkey, attempt a lookup.
				status = UCKeyTranslate(
									keyboard_layout,
									keycode,
									kUCKeyActionDown,
									(modifiers >> 16) & 0xFF, //(modifiers >> 16) & 0xFF, || (modifiers >> 8) & 0xFF,
									LMGetKbdType(),
									kNilOptions, //kNilOptions, //kUCKeyTranslateNoDeadKeysMask
									&curr_deadkey_state,
									size,
									length,
									buffer);
			}
			else {
				// The previous key was a deadkey, lookup what it should be.
				status = UCKeyTranslate(
									keyboard_layout,
									keycode,
									kUCKeyActionDown,
									(modifiers >> 16) & 0xFF, //No Modifier
									LMGetKbdType(),
									kNilOptions, //kNilOptions, //kUCKeyTranslateNoDeadKeysMask
									&curr_deadkey_state,
									size,
									length,
									buffer);
			}

			if (status == noErr && *length > 0) {
				if (is_caps_lock) {
					// We *had* a caps lock mask so we need to convert to uppercase.
					CFMutableStringRef keytxt = CFStringCreateMutableWithExternalCharactersNoCopy(kCFAllocatorDefault, buffer, *length, size, kCFAllocatorNull);
					if (keytxt != NULL) {
						CFLocaleRef locale = CFLocaleCopyCurrent();
						CFStringUppercase(keytxt, locale);
						CFRelease(locale);
						CFRelease(keytxt);
					}
					else {
						// There was an problem creating the CFMutableStringRef.
						*length = 0;
					}
				}
			}
			else {
				// Make sure the buffer length is zero if an error occurred.
				*length = 0;
			}
		}
	}
	#endif

	// Fallback to CGEventKeyboardGetUnicodeString if we were unable to use UCKeyTranslate().
	#if defined(CARBON_LEGACY) || defined(COREFOUNDATION)
	if (*length == 0) {
		CGEventKeyboardGetUnicodeString(event, size, length, buffer);
	}
	#else
	CGEventKeyboardGetUnicodeString(event, size, length, buffer);
	#endif

	// The following codes should not be processed because they are invalid.
	// FIXME This entire function is ugly, hard to follow and needs to be reworked.
	if (*length == 1) {
		switch (buffer[0]) {
			case 0x01:		// Home
			case 0x04:		// End
			case 0x05:		// Help Key
			case 0x10:		// Function Keys
			case 0x0B:		// Page Up
			case 0x0C:		// Page Down
				*length = 0;
		}
	}
}

void load_input_helper() {
	#if defined(CARBON_LEGACY) || defined(COREFOUNDATION)
	// Start with a fresh dead key state.
	curr_deadkey_state = 0;
	#endif
}

void unload_input_helper() {
	#if defined(CARBON_LEGACY) || defined(COREFOUNDATION)
	if (prev_keyboard_layout != NULL) {
		// Cleanup tracking of the previous layout.
		CFRelease(prev_keyboard_layout);
	}
	#endif
}
