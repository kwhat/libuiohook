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
#include <uiohook.h>

#include "input_helper.h"

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

/*
static const uint16_t keycode_to_scancode_table[128][2] = {
	{	kVK_Undefined,	VC_A },	// 0	VC_UNDEFINED	kVK_ANSI_A
	{	kVK_Escape,		VC_S }, // 1	VC_ESCAPE		kVK_ANSI_S
}
*/

static const uint16_t keycode_to_scancode_table[128] = {
//	value							idx key
	VC_A,						//	  0 kVK_ANSI_A
	VC_S,						//	  1 kVK_ANSI_S
	VC_D,						//	  2 kVK_ANSI_D
	VC_F,						//	  3 kVK_ANSI_F
	VC_H,						//	  4 kVK_ANSI_H
	VC_G,						//	  5 kVK_ANSI_G
	VC_Z,						//	  6 kVK_ANSI_Z
	VC_X,						//	  7 kVK_ANSI_X
	VC_C,						//	  8 kVK_ANSI_C
	VC_V,						//	  9 kVK_ANSI_V
	VC_UNDEFINED,				//	 10 kVK_ISO_Section (0x56)
	VC_B,						//	 11 kVK_ANSI_B
	VC_Q,						//	 12 kVK_ANSI_Q
	VC_W,						//	 13 kVK_ANSI_W
	VC_E,						//	 14 kVK_ANSI_E
	VC_R,						//	 15 kVK_ANSI_R
	VC_Y,						//	 16 kVK_ANSI_Y
	VC_T,						//	 17 kVK_ANSI_T
	VC_1,						//	 18 kVK_ANSI_1
	VC_2,						//	 19 kVK_ANSI_2
	VC_3,						//	 20 kVK_ANSI_3
	VC_4,						//	 21 kVK_ANSI_4
	VC_6,						//	 22 kVK_ANSI_6
	VC_5,						//	 23 kVK_ANSI_5
	VC_EQUALS,					//	 24 kVK_ANSI_Equal
	VC_9,						//	 25 kVK_ANSI_9
	VC_7,						//	 26 kVK_ANSI_7
	VC_MINUS,					//	 27 kVK_ANSI_Minus
	VC_8,						//	 28 kVK_ANSI_8
	VC_0,						//	 29 kVK_ANSI_0
	VC_CLOSE_BRACKET,			//	 30 kVK_ANSI_RightBracket
	VC_O,						//	 31 kVK_ANSI_O
	VC_U,						//	 32 kVK_ANSI_U
	VC_OPEN_BRACKET,			//	 33 kVK_ANSI_LeftBracket
	VC_I,						//	 34 kVK_ANSI_I
	VC_P,						//	 35 kVK_ANSI_P
	VC_ENTER,					//	 36 kVK_Return
	VC_L,						//	 37 kVK_ANSI_L
	VC_J,						//	 38 kVK_ANSI_J
	VC_QUOTE,					//	 39 kVK_ANSI_Quote
	VC_K,						//	 40 kVK_ANSI_K
	VC_SEMICOLON,				//	 41 kVK_ANSI_Semicolon
	VC_BACK_SLASH,				//	 42 kVK_ANSI_Backslash
	VC_COMMA,					//	 43 kVK_ANSI_Comma
	VC_SLASH,					//	 44 kVK_ANSI_Slash
	VC_N,						//	 45 kVK_ANSI_N
	VC_M,						//	 46 kVK_ANSI_M
	VC_PERIOD,					//	 47 kVK_ANSI_Period
	VC_TAB,						//	 48 kVK_Tab
	VC_SPACE,					//	 49 kVK_Space
	VC_BACKQUOTE,				//	 50 kVK_ANSI_Grave
	VC_BACKSPACE,				//	 51 kVK_Delete
	VC_UNDEFINED,				//	 52
	VC_ESCAPE,					//	 53 kVK_Escape
	VC_META_R,					//	 54 kVK_RightCommand
	VC_META_L,					//	 55 kVK_Command
	VC_SHIFT_L,					//	 56 kVK_Shift
	VC_CAPS_LOCK,				//	 57 kVK_CapsLock
	VC_ALT_L,					//	 58 kVK_Option
	VC_CONTROL_L,				//	 59 kVK_Control
	VC_SHIFT_R,					//	 60 kVK_RightShift
	VC_ALT_R,					//	 61 kVK_RightOption
	VC_CONTROL_R,				//	 62 kVK_RightControl
	VC_UNDEFINED,				//	 63 kVK_Function
	VC_F17,						//	 64 kVK_F17
	VC_KP_SEPARATOR,			//	 65 kVK_ANSI_KeypadDecimal
	VC_UNDEFINED,				//	 66 unused
	VC_KP_MULTIPLY,				//	 67 kVK_ANSI_KeypadMultiply
	VC_UNDEFINED,				//	 68 unused
	VC_KP_ADD,					//	 69 kVK_ANSI_KeypadPlus
	VC_UNDEFINED,				//	 70 unused
	VC_NUM_LOCK,				//	 71 kVK_ANSI_KeypadClear
	VC_VOLUME_UP,				//	 72 kVK_VolumeUp
	VC_VOLUME_DOWN,				//	 73 kVK_VolumeDown
	VC_VOLUME_MUTE,				//	 74 kVK_Mute
	VC_KP_DIVIDE,				//	 75 kVK_ANSI_KeypadDivide
	VC_KP_ENTER,				//	 76 kVK_ANSI_KeypadEnter

	VC_UNDEFINED,				//	 77 unused
	VC_KP_SUBTRACT,				//	 78 kVK_ANSI_KeypadMinus
	VC_F18,						//	 79 kVK_F18
	VC_F19,						//	 80 kVK_F19
	VC_KP_EQUALS,				//	 81 kVK_ANSI_KeypadEquals
	VC_KP_0,					//	 82 kVK_ANSI_Keypad0
	VC_KP_1,					//	 83 kVK_ANSI_Keypad1
	VC_KP_2,					//	 84 kVK_ANSI_Keypad2
	VC_KP_3,					//	 85 kVK_ANSI_Keypad3
	VC_KP_4,					//	 86 kVK_ANSI_Keypad4
	VC_KP_5,					//	 87 kVK_ANSI_Keypad5
	VC_KP_6,					//	 88 kVK_ANSI_Keypad6
	VC_KP_7,					//	 89 kVK_ANSI_Keypad7
	VC_F20,						//	 90 kVK_F20
	VC_KP_8,					//	 91 kVK_ANSI_Keypad8
	VC_KP_9,					//	 92 kVK_ANSI_Keypad9
	VC_YEN,						//	 93 kVK_JIS_Yen
	VC_UNDERSCORE,				//	 94 kVK_JIS_Underscore
	VC_KP_COMMA,				//	 95 kVK_JIS_KeypadComma
	VC_F5,						//	 96 kVK_F5
	VC_F6,						//	 97 kVK_F6
	VC_F7,						//	 98 kVK_F7
	VC_F3,						//	 99 kVK_F3
	VC_F8,						//	100 kVK_F8
	VC_F9,						//	101 kVK_F9
	VC_CAPS_LOCK,				//	102 kVK_JIS_Eisu
	VC_F11,						//	103 kVK_F11
	VC_KATAKANA,				//	104 kVK_JIS_Kana
	VC_F13,						//	105 kVK_F13
	VC_F16,						//	106 kVK_F16
	VC_F14,						//	107 kVK_F14
	VC_UNDEFINED,				//	108 unused
	VC_F10,						//	109 kVK_F10
	VC_UNDEFINED,				//	110 unused
	VC_F12,						//	111 kVK_F12
	VC_UNDEFINED,				//	112 unused
	VC_F15,						//	113 kVK_F15
	VC_INSERT,					//	114 kVK_Help
	VC_HOME,					//	115 kVK_Home
	VC_PAGE_UP,					//	116 kVK_PageUp
	VC_DELETE,					//	117 kVK_ForwardDelete
	VC_F4,						//	118 kVK_F4
	VC_END,						//	119 kVK_End
	VC_F2,						//	120 kVK_F2
	VC_PAGE_DOWN,				//	121 kVK_PageDown
	VC_F1,						//	122 kVK_F1
	VC_LEFT,					//	123 kVK_LeftArrow
	VC_RIGHT,					//	124 kVK_RightArrow
	VC_DOWN,					//	125 kVK_DownArrow
	VC_UP,						//	126 kVK_UpArrow
	VC_UNDEFINED				//	127
};

static const UInt64 scancode_to_keycode_table[207] = {
//	value							idx key
	kVK_Undefined,				//	  0 VC_UNDEFINED
	kVK_Escape,					//	  1 VC_ESCAPE
	kVK_ANSI_1,					//	  2 VC_1
	kVK_ANSI_2,					//	  3 VC_2
	kVK_ANSI_3,					//	  4 VC_3
	kVK_ANSI_4,					//	  5 VC_4
	kVK_ANSI_5,					//	  6 VC_5
	kVK_ANSI_6,					//	  7 VC_6
	kVK_ANSI_7,					//	  8 VC_7
	kVK_ANSI_8,					//	  9 VC_8
	kVK_ANSI_9,					//	 10 VC_9
	kVK_ANSI_0,					//	 11 VC_0
	kVK_ANSI_Minus,				//	 12 VC_MINUS
	kVK_ANSI_Equal,				//	 13 VC_EQUALS
	kVK_Delete,					//	 14 VC_BACKSPACE
	kVK_Tab,					//	 15 VC_TAB
	kVK_ANSI_Q,					//	 16 VC_Q
	kVK_ANSI_W,					//	 17 VC_W
	kVK_ANSI_E,					//	 18 VC_E
	kVK_ANSI_R,					//	 19 VC_R
	kVK_ANSI_T,					//	 20 VC_T
	kVK_ANSI_Y,					//	 21 VC_Y
	kVK_ANSI_U,					//	 22 VC_U
	kVK_ANSI_I,					//	 23 VC_I
	kVK_ANSI_O,					//	 24 VC_O
	kVK_ANSI_P,					//	 25 VC_P
	kVK_ANSI_LeftBracket,		//	 26 VC_OPEN_BRACKET
	kVK_ANSI_RightBracket,		//	 27 VC_CLOSE_BRACKET
	kVK_Return,					//	 28 VC_ENTER
	kVK_Control,				//	 29 (59)
	kVK_ANSI_A,					//	 30 VC_A
	kVK_ANSI_S,					//	 31 VC_S
	kVK_ANSI_D,					//	 32 VC_D
	kVK_ANSI_F,					//	 33 VC_F
	kVK_ANSI_G,					//	 34 VC_G
	kVK_ANSI_H,					//	 35 VC_H
	kVK_ANSI_J,					//	 36 VC_J
	kVK_ANSI_K,					//	 37 VC_K
	kVK_ANSI_L,					//	 38 VC_L
	kVK_ANSI_Semicolon,			//	 39 VC_SEMICOLON
	kVK_ANSI_Quote,				//	 40 VC_QUOTE
	kVK_ANSI_Grave,				//	 41 VC_BACKQUOTE
	kVK_Shift,					//	 42 (56)
	kVK_ANSI_Backslash,			//	 43 (42)
	kVK_ANSI_Z,					//	 44 VC_Z
	kVK_ANSI_X,					//	 45 VC_X
	kVK_ANSI_C,					//	 46 VC_C
	kVK_ANSI_V,					//	 47 VC_V
	kVK_ANSI_B,					//	 48 VC_B
	kVK_ANSI_N,					//	 49 VC_N
	kVK_ANSI_M,					//	 50 VC_M
	kVK_ANSI_Comma,				//	 51 VC_COMMA
	kVK_ANSI_Period,			//	 52 VC_PERIOD
	kVK_ANSI_Slash,				//	 53 VC_SLASH
	kVK_RightShift,				//	 54 (60
	kVK_ANSI_KeypadMultiply,	//	 55 VC_KP_MULTIPLY
	kVK_Command,				//	 56 (55
	kVK_Space,					//	 57 VC_SPACE
	kVK_CapsLock,				//	 58 VC_CAPS_LOCK
	kVK_F1,						//	 59 VC_F1
	kVK_F2,						//	 60 VC_F2
	kVK_F3,						//	 61 VC_F3
	kVK_F4,						//	 62 VC_F4
	kVK_F5,						//	 63 VC_F5
	kVK_F6,						//	 64 VC_F6
	kVK_F7,						//	 65 VC_F7
	kVK_F8,						//	 66 VC_F8
	kVK_F9,						//	 67 VC_F9
	kVK_F10,					//	 68 VC_F10
	kVK_Undefined,				//	 69	VC_PAUSE		FIXME No Apple Support
	kVK_Undefined,				//	 70 VC_SCROLL_LOCK	FIXME No Apple Support
	kVK_ANSI_Keypad7,			//	 71 VC_KP_7
	kVK_ANSI_Keypad8,			//	 72 VC_KP_8
	kVK_ANSI_Keypad9,			//	 73 VC_KP_9
	kVK_ANSI_KeypadMinus,		//	 74 VC_KP_SUBTRACT
	kVK_ANSI_Keypad4,			//	 75 VC_KP_4
	kVK_ANSI_Keypad5,			//	 76 VC_KP_5
	kVK_ANSI_Keypad6,			//	 77 VC_KP_6
	kVK_ANSI_KeypadPlus,		//	 78 VC_KP_ADD
	kVK_ANSI_Keypad1,			//	 79 VC_KP_1
	kVK_ANSI_Keypad2,			//	 80 VC_KP_2
	kVK_ANSI_Keypad3,			//	 81 VC_KP_3
	kVK_ANSI_Keypad0,			//	 82 VC_KP_0
	kVK_ANSI_KeypadDecimal,		//	 83 VC_KP_SEPARATOR
	kVK_Undefined,				//	 84
	kVK_Undefined,				//	 85
	kVK_Undefined,				//	 86 kVK_ISO_Section	FIXME What is kVK_ISO_Section?
	kVK_F11,					//	 87 VC_F11
	kVK_F12,					//	 88 VC_F12
	kVK_ANSI_KeypadClear,		//	 89	VC_NUM_LOCK
	kVK_Undefined,				//	 90
	kVK_Undefined,				//	 91
	kVK_Undefined,				//	 92
	kVK_Undefined,				//	 93
	kVK_Undefined,				//	 94
	kVK_Undefined,				//	 95
	kVK_Undefined,				//	 96
	kVK_Undefined,				//	 97
	kVK_Undefined,				//	 98
	kVK_Undefined,				//	 99
	kVK_F13,					//	100 VC_F13
	kVK_F14,					//	101 VC_F14
	kVK_F15,					//	102 VC_F15
	kVK_F16,					//	103 VC_F16
	kVK_F17,					//	104 VC_F17
	kVK_F18,					//	105 VC_F18
	kVK_F19,					//	106 VC_F19
	kVK_F20,					//	107 VC_F20
	kVK_Undefined,				//	108
	kVK_Undefined,				//	109
	kVK_Undefined,				//	110
	kVK_Undefined,				//	111
	kVK_Undefined,				//	112
	kVK_Undefined,				//	113 kVK_JIS_Kana
	kVK_Undefined,				//	114 kVK_JIS_Eisu
	kVK_JIS_Underscore,			//	115 VC_UNDERSCORE
	kVK_Undefined,				//	116
	kVK_Undefined,				//	117
	kVK_Undefined,				//	118
	kVK_Undefined,				//	119 VC_FURIGANA
	kVK_Undefined,				//	120
	kVK_Undefined,				//	121 VC_KANJI
	kVK_Undefined,				//	122
	kVK_Undefined,				//	123 VC_HIRAGANA
	kVK_Undefined,				//	124
	kVK_JIS_Yen,				//	125 VC_YEN
	kVK_JIS_KeypadComma,		//	126 VC_KP_COMMA
	kVK_Undefined				//	127

	// Offset i & 0x00FF + (128 - 13)

	kVK_ANSI_KeypadEquals,		//	 13 VC_KP_EQUALS
	kVK_Undefined,				//	 14
	kVK_Undefined,				//	 15
	kVK_Undefined,				//	 16
	kVK_Undefined,				//	 17
	kVK_Undefined,				//	 18
	kVK_Undefined,				//	 19
	kVK_Undefined,				//	 20
	kVK_Undefined,				//	 21
	kVK_Undefined,				//	 22
	kVK_Undefined,				//	 23
	kVK_Undefined,				//	 24
	kVK_Undefined,				//	 25
	kVK_Undefined,				//	 26
	kVK_Undefined,				//	 27
	kVK_ANSI_KeypadEnter,		//	 28 VC_KP_ENTER
	kVK_RightControl,			//	 29 VC_CONTROL_R
	kVK_Undefined,				//	 30
	kVK_Undefined,				//	 31
	kVK_Mute,					//	 32 VC_VOLUME_MUTE
	kVK_Undefined,				//	 33
	kVK_Undefined,				//	 34
	kVK_Undefined,				//	 35
	kVK_Undefined,				//	 36
	kVK_Undefined,				//	 37
	kVK_Undefined,				//	 38
	kVK_Undefined,				//	 39
	kVK_Undefined,				//	 40
	kVK_Undefined,				//	 41
	kVK_Undefined,				//	 42
	kVK_Undefined,				//	 43
	kVK_Undefined,				//	 44
	kVK_Undefined,				//	 45
	kVK_VolumeDown,				//	 46 VC_VOLUME_DOWN
	kVK_Home,					//	 47 VC_HOME
	kVK_VolumeUp,				//	 48 VC_VOLUME_UP
	kVK_Undefined,				//	 49
	kVK_Undefined,				//	 50
	kVK_Undefined,				//	 51
	kVK_Undefined,				//	 52
	kVK_ANSI_KeypadDivide,		//	 53 VC_KP_DIVIDE
	kVK_Undefined,				//	 54
	kVK_Undefined,				//	 55
	kVK_RightOption,			//	 56 VC_ALT_R
	kVK_Undefined,				//	 57
	kVK_Undefined,				//	 58
	kVK_Undefined,				//	 59
	kVK_Undefined,				//	 60
	kVK_Undefined,				//	 61
	kVK_Undefined,				//	 62
	kVK_Undefined,				//	 63
	kVK_Undefined,				//	 64
	kVK_Undefined,				//	 65
	kVK_Undefined,				//	 66
	kVK_Undefined,				//	 67
	kVK_Undefined,				//	 68
	kVK_Undefined,				//	 69
	kVK_Undefined,				//	 70
	kVK_Undefined,				//	 71
	kVK_UpArrow,				//	 72	VC_UP
	kVK_PageUp,					//	 73 VC_PAGE_UP
	kVK_LeftArrow,				//	 75 VC_LEFT
	kVK_RightArrow,				//	 77 VC_RIGHT
	kVK_End,					//	 79 VC_END
	kVK_DownArrow,				//	 80 VC_DOWN
	kVK_PageDown,				//	 81 VC_PAGE_DOWN
	kVK_Help,					//	 82 VC_INSERT
	kVK_ForwardDelete,			//	 83 VC_DELETE
	kVK_Undefined,				//	 84
	kVK_Undefined,				//	 85
	kVK_Undefined,				//	 86
	kVK_Undefined,				//	 87
	kVK_Undefined,				//	 88
	kVK_Undefined,				//	 89
	kVK_Undefined,				//	 90
	kVK_Undefined,				//	 91
	kVK_RightCommand,			//	 92 VC_META_R
};

uint16_t keycode_to_scancode(UInt64 keycode) {
	uint16_t scancode = VC_UNDEFINED;

	// Bound check 0 <= keycode < 128
	if (keycode < 128) {
		scancode = keycode_to_scancode_table[keycode];
	}

	return scancode;
}

UInt64 scancode_to_keycode(uint16_t scancode) {
	UInt64 keycode = kVK_Undefined;

	// Bound check 0 <= keycode < 128
	if (scancode < 128) {
		keycode = scancode_to_keycode_table[scancode];
	}
	else {
		// Calculate the upper offset.
		int i = (scancode & 0xFF) + (128 - 13);

		if () {
			keycode = scancode_to_keycode_table[i];
		}
	}

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
