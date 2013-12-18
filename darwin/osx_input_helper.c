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

#ifdef USE_COREFOUNDATION
#include <CoreFoundation/CoreFoundation.h>
#endif
#include <stdbool.h>

#include "osx_input_helper.h"

// Current dead key state.
#if defined(USE_CARBON_LEGACY) || defined(USE_COREFOUNDATION)
static UInt32 curr_deadkey_state = 0;
#endif

// Input source data for the keyboard.
#if defined(USE_CARBON_LEGACY)
static KeyboardLayoutRef prev_keyboard_layout = NULL;
#elif defined(USE_COREFOUNDATION)
static TISInputSourceRef prev_keyboard_layout = NULL;
#endif

// This method must be executed from the main runloop to avoid the seemingly random
// Exception detected while handling key input.  TSMProcessRawKeyCode failed (-192) errors.
// CFEqual(CFRunLoopGetCurrent(), CFRunLoopGetMain())
void keycode_to_string(CGEventRef event_ref, UniCharCount size, UniCharCount *length, UniChar *buffer) {
	#if defined(USE_CARBON_LEGACY) || defined(USE_COREFOUNDATION)
	#if defined(USE_CARBON_LEGACY)
	KeyboardLayoutRef curr_keyboard_layout;
	void *inputData = NULL;
	if (KLGetCurrentKeyboardLayout(&curr_keyboard_layout) == noErr) {
		if (KLGetKeyboardLayoutProperty(curr_keyboard_layout, kKLuchrData, (const void **) &inputData) != noErr) {
			inputData = NULL;
		}
	}
	#elif defined(USE_COREFOUNDATION)
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
		#ifdef USE_CARBON_LEGACY
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
	#if defined(USE_CARBON_LEGACY) || defined(USE_COREFOUNDATION)
	if (*length == 0) {
		CGEventKeyboardGetUnicodeString(event_ref, size, length, buffer);
	}
	#else
	CGEventKeyboardGetUnicodeString(event_ref, size, length, buffer);
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


static const uint16_t keycode_to_scancode_table[128] = {
    0x1E,			//   0 kVK_ANSI_A
    0x1F,			//   1 kVK_ANSI_S
    0x20,			//   2 kVK_ANSI_D
    0x21,			//   3 kVK_ANSI_F
    0x23,			//   4 kVK_ANSI_H
    0x22,			//   5 kVK_ANSI_G
    0x2C,			//   6 kVK_ANSI_Z
    0x2D,			//   7 kVK_ANSI_X
    0x2E,			//   8 kVK_ANSI_C
    0x2F,			//   9 kVK_ANSI_V
    0x56,			//  10 kVK_ISO_Section
    0x30,			//  11 kVK_ANSI_B
    0x10,			//  12 kVK_ANSI_Q
    0x11,			//  13 kVK_ANSI_W
    0x12,			//  14 kVK_ANSI_E
    0x13,			//  15 kVK_ANSI_R
    0x15,			//  16 kVK_ANSI_Y
    0x14,			//  17 kVK_ANSI_T
    0x02,			//  18 kVK_ANSI_1
    0x03,			//  19 kVK_ANSI_2
    0x04,			//  20 kVK_ANSI_3
    0x05,			//  21 kVK_ANSI_4
    0x07,			//	22 kVK_ANSI_6
    0x06,			//  23 kVK_ANSI_5
    0x0D,			//  24 kVK_ANSI_Equal
    0x0A,			//  25 kVK_ANSI_9
    0x08,			//  26 kVK_ANSI_7
    0x0C,			//  27 kVK_ANSI_Minus
    0x09,			//  28 kVK_ANSI_8
    0x0B,			//  29 kVK_ANSI_0
    0x1B,			//  30 kVK_ANSI_RightBracket
    0x18,			//  31 kVK_ANSI_O
    0x16,			//  32 kVK_ANSI_U
    0x1A,			//  33 kVK_ANSI_LeftBracket
    0x17,			//  34 kVK_ANSI_I
    0x19,			//  35 kVK_ANSI_P
    0x1C,			//  36 kVK_Return
    0x26,			//  37 kVK_ANSI_L
    0x24,			//  38 kVK_ANSI_J
    0x28,			//  39 kVK_ANSI_Quote
    0x25,			//  40 kVK_ANSI_K
    0x27,			//  41 kVK_ANSI_Semicolon
    0x2B,			//  42 kVK_ANSI_Backslash
    0x33,			//  43 kVK_ANSI_Comma
    0x35,			//  44 kVK_ANSI_Slash
    0x31,			//  45 kVK_ANSI_N
    0x32,			//  46 kVK_ANSI_M
    0x34,			//  47 kVK_ANSI_Period
    0x0F,			//  48 kVK_Tab
    0x39,			//  49 kVK_Space
    0x29,			//  50 kVK_ANSI_Grave
    0x0E,			//	51 kVK_Delete
    0x00,			//  52 unused
    0x01,			//  53 kVK_Escape
    0x38 | 0x100,	//  54 kVK_RightCommand
    0x38,			//  55 kVK_Command
    0x2A,			//  56 kVK_Shift
    0x3A,			//  57 kVK_CapsLock
    0x00,			//  58 kVK_Option
    0x1D,			//  59 kVK_Control
    0x36,			//  60 kVK_RightShift
    0x00,			//  61 kVK_RightOption
    0x1D | 0x100,	//  62 kVK_RightControl
    0x00,			//  63 kVK_Function
    0x68,			//  64 kVK_F17
    0x53,			//  65 kVK_ANSI_KeypadDecimal
    0x00,			//  66 unused
    0x37,			//  67 kVK_ANSI_KeypadMultiply
    0x00,			//	68 unused
    0x4E,			//  69 kVK_ANSI_KeypadPlus
    0x00,			//  70 unused
    0x59,			//  71 kVK_ANSI_KeypadClear
    0x00 | 0x100,	//  72 kVK_VolumeUp
    0x00 | 0x100,	//  73 kVK_VolumeDown
    0x00 | 0x100,	//  74 kVK_Mute
    0x35 | 0x100,	//  75 kVK_ANSI_KeypadDivide
    0x1C | 0x100,	//  76 kVK_ANSI_KeypadEnter
    0x00,			//  75 unused
    0x4A,			//  76 kVK_ANSI_KeypadMinus
    0x69,			//  77 kVK_F18
    0x6A,			//  78 kVK_F19
    0x0D | 0x100,	//  79 kVK_ANSI_KeypadEquals
    0x52,			//  80 kVK_ANSI_Keypad0
    0x4F,			//  81 kVK_ANSI_Keypad1
    0x50,			//  82 kVK_ANSI_Keypad2
    0x51,			//  83 kVK_ANSI_Keypad3
    0x4B,			//  84 kVK_ANSI_Keypad4
    0x4C,			//  85 kVK_ANSI_Keypad5
    0x4D,			//  86 kVK_ANSI_Keypad6
    0x47,			//  87 kVK_ANSI_Keypad7
    0x6B,			//  88 kVK_F20
    0x48,			//  89 kVK_ANSI_Keypad8
    0x49,			//  90 kVK_ANSI_Keypad9
    0x7D,			//  91 kVK_JIS_Yen
    0x73,			//  92 kVK_JIS_Underscore
    0x7E,			//  93 kVK_JIS_KeypadComma
    0x3F,			//  94 kVK_F5
    0x40,			//  95 kVK_F6
    0x41,			//  96 kVK_F7
    0x3D,			//  97 kVK_F3
    0x42,			//  98 kVK_F8
    0x43,			//  99 kVK_F9
    0x72,			// 100 kVK_JIS_Eisu
    0x57,			// 101 kVK_F11
    0x71,           // 102 kVK_JIS_Kana
    0x64,			// 103 kVK_F13
    0x67,			// 104 kVK_F16
    0x65,			// 105 kVK_F14
    0x00,			// 106 unused
    0x44,			// 107 kVK_F10
    0x00,			// 108 unused
    0x58,			// 109 kVK_F12
    0x00,			// 110 unused
    0x66,			// 111 kVK_F15
    0x52 | 0x100,	// 112 kVK_Help
    0x47 | 0x100,	// 113 kVK_Home
    0x49 | 0x100,	// 114 kVK_PageUp
    0x53 | 0x100,	// 115 kVK_ForwardDelete
    0x3E,			// 116 kVK_F4
    0x4F | 0x100,	// 117 kVK_End
    0x3C,			// 118 kVK_F2
    0x51 | 0x100,	// 119 kVK_PageDown
    0x3B,			// 120 kVK_F1
    0x4B | 0x100,	// 121 kVK_LeftArrow
    0x4D | 0x100,   // 122 kVK_RightArrow
    0x50 | 0x100,   // 123 kVK_DownArrow
    0x48 | 0x100,	// 124 kVK_UpArrow
};

static const uint16_t scancode_to_keycode_table[128] = {
    0x01,			//  53 kVK_Escape
    0x02,			//  18 kVK_ANSI_1
    0x03,			//  19 kVK_ANSI_2
    0x04,			//  20 kVK_ANSI_3
    0x05,			//  21 kVK_ANSI_4
    0x06,			//  23 kVK_ANSI_5
    0x07,			//	22 kVK_ANSI_6
    0x08,			//  26 kVK_ANSI_7
    0x09,			//  28 kVK_ANSI_8
    0x0A,			//  25 kVK_ANSI_9
    0x0B,			//  29 kVK_ANSI_0
    0x0C,			//  27 kVK_ANSI_Minus
    0x0D,			//  24 kVK_ANSI_Equal
	0x0E,			//	51 kVK_Delete
    0x0F,			//  48 kVK_Tab
	0x10,			//  12 kVK_ANSI_Q
	0x11,			//  13 kVK_ANSI_W
    0x12,			//  14 kVK_ANSI_E
    0x13,			//  15 kVK_ANSI_R
    0x14,			//  17 kVK_ANSI_T
	0x15,			//  16 kVK_ANSI_Y
	0x16,			//  32 kVK_ANSI_U
	0x17,			//  34 kVK_ANSI_I
	0x18,			//  31 kVK_ANSI_O
    0x19,			//  35 kVK_ANSI_P
    0x1A,			//  33 kVK_ANSI_LeftBracket
    0x1B,			//  30 kVK_ANSI_RightBracket
    0x1C,			//  36 kVK_Return	
    0x1D,			//  59 kVK_Control	
    0x1E,			//   0 kVK_ANSI_A
    0x1F,			//   1 kVK_ANSI_S
	0x20,			//   2 kVK_ANSI_D
    0x21,			//   3 kVK_ANSI_F
    0x22,			//   5 kVK_ANSI_G
    0x23,			//   4 kVK_ANSI_H
    0x24,			//  38 kVK_ANSI_J
    0x25,			//  40 kVK_ANSI_K
	0x26,			//  37 kVK_ANSI_L
	0x27,			//  41 kVK_ANSI_Semicolon
    0x28,			//  39 kVK_ANSI_Quote
    0x29,			//  50 kVK_ANSI_Grave
    0x2A,			//  56 kVK_Shift
    0x2B,			//  42 kVK_ANSI_Backslash
    0x2C,			//   6 kVK_ANSI_Z
    0x2D,			//   7 kVK_ANSI_X
    0x2E,			//   8 kVK_ANSI_C
    0x2F,			//   9 kVK_ANSI_V
	0x30,			//  11 kVK_ANSI_B
	0x31,			//  45 kVK_ANSI_N
    0x32,			//  46 kVK_ANSI_M
	0x33,			//  43 kVK_ANSI_Comma
    0x34,			//  47 kVK_ANSI_Period
	0x35,			//  44 kVK_ANSI_Slash
	0x36,			//  60 kVK_RightShift
	0x37,			//  67 kVK_ANSI_KeypadMultiply
	0x38,			//  55 kVK_Command
	0x39,			//  49 kVK_Space
	0x3A,			//  57 kVK_CapsLock	
	0x3B,			// 120 kVK_F1
	0x3C,			// 118 kVK_F2
	0x3D,			//  97 kVK_F3
    0x3E,			// 116 kVK_F4
	0x3F,			//  94 kVK_F5
	0x40,			//  95 kVK_F6
    0x41,			//  96 kVK_F7
	0x42,			//  98 kVK_F8
	0x43,			//  99 kVK_F9
	0x44,			// 107 kVK_F10
	
    0x47,			//  87 kVK_ANSI_Keypad7    
    0x48,			//  89 kVK_ANSI_Keypad8
    0x49,			//  90 kVK_ANSI_Keypad9

	0x4B,			//  84 kVK_ANSI_Keypad4
    0x4C,			//  85 kVK_ANSI_Keypad5
    0x4D,			//  86 kVK_ANSI_Keypad6
	0x4F,			//  81 kVK_ANSI_Keypad1
    0x50,			//  82 kVK_ANSI_Keypad2
    0x51,			//  83 kVK_ANSI_Keypad3
    0x52,			//  80 kVK_ANSI_Keypad0
	
	0x4A,			//  76 kVK_ANSI_KeypadMinus
	0x4E,			//  69 kVK_ANSI_KeypadPlus

	0x53,			//  65 kVK_ANSI_KeypadDecimal
    0x56,			//  10 kVK_ISO_Section
	0x57,			// 101 kVK_F11
	0x58,			// 109 kVK_F12
	0x59,			//  71 kVK_ANSI_KeypadClear
   
	0x64,			// 103 kVK_F13
    0x65,			// 105 kVK_F14
	0x66,			// 111 kVK_F15
	0x67,			// 104 kVK_F16
    0x68,			//  64 kVK_F17
	0x69,			//  77 kVK_F18
	0x6A,			//  78 kVK_F19
    0x6B,			//  88 kVK_F20


	0x71,           // 102 kVK_JIS_Kana
    0x72,			// 100 kVK_JIS_Eisu
    0x73,			//  92 kVK_JIS_Underscore
	
	0x7D,			//  91 kVK_JIS_Yen
    0x7E,			//  93 kVK_JIS_KeypadComma


	
    
	0x0D | 0x100,	//  79 kVK_ANSI_KeypadEquals
	0x1C | 0x100,	//  76 kVK_ANSI_KeypadEnter
	0x1D | 0x100,	//  62 kVK_RightControl
	0x35 | 0x100,	//  75 kVK_ANSI_KeypadDivide
	0x38 | 0x100,	//  54 kVK_RightCommand
	0x47 | 0x100,	// 113 kVK_Home
	0x48 | 0x100,	// 124 kVK_UpArrow
    0x49 | 0x100,	// 114 kVK_PageUp
    0x4B | 0x100,	// 121 kVK_LeftArrow
    0x4D | 0x100,   // 122 kVK_RightArrow
    0x4F | 0x100,	// 117 kVK_End
	0x50 | 0x100,   // 123 kVK_DownArrow
	0x51 | 0x100,	// 119 kVK_PageDown
    0x52 | 0x100,	// 112 kVK_Help
    0x53 | 0x100,	// 115 kVK_ForwardDelete
    
    
    0x00,			//  52 unused

    0x00,			//  58 kVK_Option
    
    0x00,			//  61 kVK_RightOption
    
    0x00,			//  63 kVK_Function

    0x00,			//  66 unused
    
    0x00,			//	68 unused
    
    0x00,			//  70 unused

    0x00 | 0x100,	//  72 kVK_VolumeUp
    0x00 | 0x100,	//  73 kVK_VolumeDown
    0x00 | 0x100,	//  74 kVK_Mute
	0x00,			//  75 unused
	
	0x00,			// 106 unused
    0x00,			// 108 unused
    0x00,			// 110 unused
    

};

uint16_t keycode_to_scancode(UInt64 keycode) {
	uint16_t scancode = 0x00;
	
	// Bound check 0 <= keycode < 128
	if (keycode < 128) {
		scancode = keycode_to_scancode_table[keycode];
	}
	
	return scancode;
}

UInt64 scancode_to_keycode(uint16_t keycode) {
	return keycode;
}

void load_input_helper() {
	#if defined(USE_CARBON_LEGACY) || defined(USE_COREFOUNDATION)
	// Start with a fresh dead key state.
	curr_deadkey_state = 0;
	#endif
}

void unload_input_helper() {
	#if defined(USE_CARBON_LEGACY) || defined(USE_COREFOUNDATION)
	if (prev_keyboard_layout != NULL) {
		// Cleanup tracking of the previous layout.
		CFRelease(prev_keyboard_layout);
	}
	#endif
}
