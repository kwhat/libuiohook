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


static const uint16_t keycode_scancode_table[][2] = {
	/* idx		{ keycode,				scancode				}, */
	/*   0 */	{ VC_A,					kVK_Undefined			},	// 0x00
	/*   1 */	{ VC_S,					kVK_Escape				},	// 0x01
	/*   2 */	{ VC_D,					kVK_ANSI_1				},	// 0x02
	/*   3 */	{ VC_F,					kVK_ANSI_2				},	// 0x03
	/*   4 */	{ VC_H,					kVK_ANSI_3				},	// 0x04
	/*   5 */	{ VC_G,					kVK_ANSI_4				},	// 0x05
	/*   6 */	{ VC_Z,					kVK_ANSI_5				},	// 0x07
	/*   7 */	{ VC_X,					kVK_ANSI_6				},	// 0x08
	/*   8 */	{ VC_C,					kVK_ANSI_7				},	// 0x09
	/*   9 */	{ VC_V,					kVK_ANSI_8				},	// 0x0A
	/*  10 */	{ VC_UNDEFINED,			kVK_ANSI_9				},	// 0x0B
	/*  11 */	{ VC_B,					kVK_ANSI_0				},	// 0x0C
	/*  12 */	{ VC_Q,					kVK_ANSI_Minus			},	// 0x0D
	/*  13 */	{ VC_W,					kVK_ANSI_Equal			},	// 0x0E
	/*  14 */	{ VC_E,					kVK_Delete				},	// 0x0F
	/*  15 */	{ VC_R,					kVK_Tab					},	// 0x10
	/*  16 */	{ VC_Y,					kVK_ANSI_Q				},	// 0x11
	/*  17 */	{ VC_T,					kVK_ANSI_W				},	// 0x12
	/*  18 */	{ VC_1,					kVK_ANSI_E				},	// 0x13
	/*  19 */	{ VC_2,					kVK_ANSI_R				},	// 0x14
	/*  20 */	{ VC_3,					kVK_ANSI_T				},	// 0x15
	/*  21 */	{ VC_4,					kVK_ANSI_Y				},	// 0x16
	/*  22 */	{ VC_6,					kVK_ANSI_U				},	// 0x17
	/*  23 */	{ VC_5,					kVK_ANSI_I				},	// 0x18
	/*  24 */	{ VC_EQUALS,			kVK_ANSI_O				},	// 0x19
	/*  25 */	{ VC_9,					kVK_ANSI_P				},	// 0x19
	/*  26 */	{ VC_7,					kVK_ANSI_LeftBracket	},	// 0x1A
	/*  27 */	{ VC_MINUS,				kVK_ANSI_RightBracket	},	// 0x1B
	/*  28 */	{ VC_8,					kVK_Return				},	// 0x1C
	/*  29 */	{ VC_0,					kVK_Control				},	// 0x1D
	/*  30 */	{ VC_CLOSE_BRACKET,		kVK_ANSI_A				},	// 0x1E
	/*  31 */	{ VC_O,					kVK_ANSI_S				},	// 0x1F
	/*  32 */	{ VC_U,					kVK_ANSI_D				},	// 0x20
	/*  33 */	{ VC_OPEN_BRACKET,		kVK_ANSI_F				},	// 0x21
	/*  34 */	{ VC_I,					kVK_ANSI_G				},	// 0x22
	/*  35 */	{ VC_P,					kVK_ANSI_H				},	// 0x23
	/*  36 */	{ VC_ENTER,				kVK_ANSI_J				},	// 0x24
	/*  37 */	{ VC_L,					kVK_ANSI_K				},	// 0x25
	/*  38 */	{ VC_J,					kVK_ANSI_L				},	// 0x26
	/*  39 */	{ VC_QUOTE,				kVK_ANSI_Semicolon		},	// 0x27
	/*  40 */	{ VC_K,					kVK_ANSI_Quote			},	// 0x28
	/*  41 */	{ VC_SEMICOLON,			kVK_ANSI_Grave			},	// 0x29
	/*  42 */	{ VC_BACK_SLASH,		kVK_Shift				},	// 0x2A
	/*  43 */	{ VC_COMMA,				kVK_ANSI_Backslash		},	// 0x2B
	/*  44 */	{ VC_SLASH,				kVK_ANSI_Z				},	// 0x2C
	/*  45 */	{ VC_N,					kVK_ANSI_X				},	// 0x2D
	/*  46 */	{ VC_M,					kVK_ANSI_C				},	// 0x2E
	/*  47 */	{ VC_PERIOD,			kVK_ANSI_V				},	// 0x2F
	/*  48 */	{ VC_TAB,				kVK_ANSI_B				},	// 0x30
	/*  49 */	{ VC_SPACE,				kVK_ANSI_N				},	// 0x31
	/*  50 */	{ VC_BACKQUOTE,			kVK_ANSI_M				},	// 0x32
	/*  51 */	{ VC_BACKSPACE,			kVK_ANSI_Comma			},	// 0x33
	/*  52 */	{ VC_UNDEFINED,			kVK_ANSI_Period			},	// 0x34
	/*  53 */	{ VC_ESCAPE,			kVK_ANSI_Slash			},	// 0x35
	/*  54 */	{ VC_META_R,			kVK_RightShift			},	// 0x36
	/*  55 */	{ VC_META_L,			kVK_ANSI_KeypadMultiply	},	// 0x37
	/*  56 */	{ VC_SHIFT_L,			kVK_Command				},	// 0x38
	/*  57 */	{ VC_CAPS_LOCK,			kVK_Space				},	// 0x39
	/*  58 */	{ VC_ALT_L,				kVK_CapsLock			},	// 0x3A
	/*  59 */	{ VC_CONTROL_L,			kVK_F1					},	// 0x41
	/*  60 */	{ VC_SHIFT_R,			kVK_F2					},	// 0x42
	/*  61 */	{ VC_ALT_R,				kVK_F3					},	// 0x43
	/*  62 */	{ VC_CONTROL_R,			kVK_F4					},	// 0x44
	/*  63 */	{ VC_UNDEFINED,			kVK_F5					},	// 0x45
	/*  64 */	{ VC_F17,				kVK_F6					},	// 0x46
	/*  65 */	{ VC_KP_SEPARATOR,		kVK_F7					},	// 0x47
	/*  66 */	{ VC_UNDEFINED,			kVK_F8					},	// 0x48
	/*  67 */	{ VC_KP_MULTIPLY,		kVK_F9					},	// 0x49
	/*  68 */	{ VC_UNDEFINED,			kVK_F10					},	// 0x4A
	/*  69 */	{ VC_KP_ADD,			kVK_Undefined			},	// 0x4B
	/*  70 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0x4C
	/*  71 */	{ VC_NUM_LOCK,			kVK_ANSI_Keypad7		},	// 0x4D
	/*  72 */	{ VC_VOLUME_UP,			kVK_ANSI_Keypad8		},	// 0x4E
	/*  73 */	{ VC_VOLUME_DOWN,		kVK_ANSI_Keypad9		},	// 0x4F
	/*  74 */	{ VC_VOLUME_MUTE,		kVK_ANSI_KeypadMinus	},	// 0x50
	/*  75 */	{ VC_KP_DIVIDE,			kVK_ANSI_Keypad4		},	// 0x51
	/*  76 */	{ VC_KP_ENTER,			kVK_ANSI_Keypad5		},	// 0x52
	/*  77 */	{ VC_UNDEFINED,			kVK_ANSI_Keypad6		},	// 0x53
	/*  78 */	{ VC_KP_SUBTRACT,		kVK_ANSI_KeypadPlus		},	// 0x54
	/*  79 */	{ VC_F18,				kVK_ANSI_Keypad1		},	// 0x55
	/*  80 */	{ VC_F19,				kVK_ANSI_Keypad2		},	// 0x56
	/*  81 */	{ VC_KP_EQUALS,			kVK_ANSI_Keypad3		},	// 0x57
	/*  82 */	{ VC_KP_0,				kVK_ANSI_Keypad0		},	// 0x58
	/*  83 */	{ VC_KP_1,				kVK_ANSI_KeypadDecimal	},	// 0x59
	/*  84 */	{ VC_KP_2,				kVK_Undefined			},	// 0x5A
	/*  85 */	{ VC_KP_3,				kVK_Undefined			},	// 0x5B
	/*  86 */	{ VC_KP_4,				kVK_Undefined			},	// 0x5C
	/*  87 */	{ VC_KP_5,				kVK_F11					},	// 0x5D
	/*  88 */	{ VC_KP_6,				kVK_F12					},	// 0x5E
	/*  89 */	{ VC_KP_7,				kVK_ANSI_KeypadClear	},	// 0x5F
	/*  90 */	{ VC_F20,				kVK_Undefined			},	// 0x60
	/*  91 */	{ VC_KP_8,				kVK_Undefined			},	// 0x61
	/*  92 */	{ VC_KP_9,				kVK_Undefined			},	// 0x62
	/*  93 */	{ VC_YEN,				kVK_Undefined			},	// 0x63
	/*  94 */	{ VC_UNDERSCORE,		kVK_Undefined			},	// 0x64
	/*  95 */	{ VC_KP_COMMA,			kVK_Undefined			},	// 0x65
	/*  96 */	{ VC_F5,				kVK_Undefined			},	// 0x66
	/*  97 */	{ VC_F6,				kVK_Undefined			},	// 0x67
	/*  98 */	{ VC_F7,				kVK_Undefined			},	// 0x68
	/*  99 */	{ VC_F3,				kVK_Undefined			},	// 0x69
	/* 100 */	{ VC_F8,				kVK_F13					},	// 0x6A
	/* 101 */	{ VC_F9,				kVK_F14					},	// 0x6B
	/* 102 */	{ VC_CAPS_LOCK,			kVK_F15					},	// 0x6C
	/* 103 */	{ VC_F11,				kVK_F16					},	// 0x6D
	/* 104 */	{ VC_KATAKANA,			kVK_F17					},	// 0x6E
	/* 105 */	{ VC_F13,				kVK_F18					},	// 0x6F
	/* 106 */	{ VC_F16,				kVK_F19					},	// 0x70
	/* 107 */	{ VC_F14,				kVK_F20					},	// 0x71
	/* 108 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0x72
	/* 109 */	{ VC_F10,				kVK_Undefined			},	// 0x73
	/* 110 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0x74
	/* 111 */	{ VC_F12,				kVK_Undefined			},	// 0x75
	/* 112 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0x76
	/* 113 */	{ VC_F15,				kVK_Undefined			},	// 0x77
	/* 114 */	{ VC_INSERT,			kVK_Undefined			},	// 0x78
	/* 115 */	{ VC_HOME,				kVK_JIS_Underscore		},	// 0x79
	/* 116 */	{ VC_PAGE_UP,			kVK_Undefined			},	// 0x7A
	/* 117 */	{ VC_DELETE,			kVK_Undefined			},	// 0x7B
	/* 118 */	{ VC_F4,				kVK_Undefined			},	// 0x7C
	/* 119 */	{ VC_END,				kVK_Undefined			},	// 0x7D
	/* 120 */	{ VC_F2,				kVK_Undefined			},	// 0x7E
	/* 121 */	{ VC_PAGE_DOWN,			kVK_Undefined			},	// 0x7F
	/* 122 */	{ VC_F1,				kVK_Undefined			},	// 0x80
	/* 123 */	{ VC_LEFT,				kVK_Undefined			},	// 0x81
	/* 124 */	{ VC_RIGHT,				kVK_Undefined			},	// 0x82
	/* 125 */	{ VC_DOWN,				kVK_JIS_Yen				},	// 0x83
	/* 126 */	{ VC_UP,				kVK_JIS_KeypadComma		},	// 0x84
	/* 127 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0x85

	//			No Offset				Offset i & 0x00FF + (128 - 13)

	/* 128 */	{ VC_F23,				kVK_ANSI_KeypadEquals	},	// 0x86
	/* 129 */	{ VC_F24,				kVK_Undefined			},	// 0x87
	/* 130 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0x88
	/* 131 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0x89
	/* 132 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0x8A
	/* 133 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0x8B
	/* 134 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0x8C
	/* 135 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0x8D
	/* 136 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0x8E
	/* 137 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0x8F
	/* 138 */	{ VC_NUM_LOCK,			kVK_Undefined			},	// 0x90
	/* 139 */	{ VC_SCROLL_LOCK,		kVK_Undefined			},	// 0x91
	/* 140 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0x92
	/* 141 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0x93
	/* 142 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0x94
	/* 143 */	{ VC_UNDEFINED,			kVK_ANSI_KeypadEnter	},	// 0x95
	/* 144 */	{ VC_UNDEFINED,			kVK_RightControl		},	// 0x96
	/* 145 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0x97
	/* 146 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0x98
	/* 147 */	{ VC_UNDEFINED,			kVK_Mute				},	// 0x99
	/* 152 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0x9A
	/* 153 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0x9B
	/* 154 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0x9C
	/* 155 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0x9D
	/* 156 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0x9F
	/* 157 */	{ VC_SHIFT_L,			kVK_Undefined			},	// 0xA0
	/* 158 */	{ VC_SHIFT_R,			kVK_Undefined			},	// 0xA1
	/* 159 */	{ VC_CONTROL_L,			kVK_Undefined			},	// 0xA2
	/* 160 */	{ VC_CONTROL_R,			kVK_Undefined			},	// 0xA3
	/* 161 */	{ VC_ALT_L,				kVK_VolumeDown			},	// 0xA4
	/* 162 */	{ VC_ALT_R,				kVK_Home				},	// 0xA5
	/* 163 */	{ VC_BROWSER_BACK,		kVK_VolumeUp			},	// 0xA6
	/* 164 */	{ VC_BROWSER_FORWARD,	kVK_Undefined			},	// 0xA7
	/* 165 */	{ VC_BROWSER_REFRESH,	kVK_Undefined			},	// 0xA8
	/* 166 */	{ VC_BROWSER_STOP,		kVK_Undefined			},	// 0xA9
	/* 167 */	{ VC_BROWSER_SEARCH,	kVK_Undefined			},	// 0xAA
	/* 168 */	{ VC_BROWSER_FAVORITES,	kVK_ANSI_KeypadDivide	},	// 0xAB
	/* 169 */	{ VC_BROWSER_HOME,		kVK_Undefined			},	// 0xAC
	/* 170 */	{ VC_VOLUME_MUTE,		kVK_Undefined			},	// 0xAD
	/* 171 */	{ VC_VOLUME_UP,			kVK_RightOption			},	// 0xAE
	/* 172 */	{ VC_VOLUME_DOWN,		kVK_Undefined			},	// 0xAF
	/* 173 */	{ VC_MEDIA_NEXT,		kVK_Undefined			},	// 0xB0
	/* 174 */	{ VC_MEDIA_PREVIOUS,	kVK_Undefined			},	// 0xB1
	/* 175 */	{ VC_MEDIA_STOP,		kVK_Undefined			},	// 0xB2
	/* 176 */	{ VC_MEDIA_PLAY,		kVK_Undefined			},	// 0xB3
	/* 177 */	{ VC_APP_MAIL,			kVK_Undefined			},	// 0xB4
	/* 178 */	{ VC_MEDIA_SELECT,		kVK_Undefined			},	// 0xB5
	/* 179 */	{ VC_APP_MAIL,			kVK_Undefined			},	// 0xB6
	/* 180 */	{ VC_APP_CALCULATOR,	kVK_Undefined			},	// 0xB7
	/* 181 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xB8
	/* 182 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xB9
	/* 183 */	{ VC_SEMICOLON,			kVK_Undefined			},	// 0xBA
	/* 184 */	{ VC_EQUALS,			kVK_Undefined			},	// 0xBB
	/* 185 */	{ VC_COMMA,				kVK_Undefined			},	// 0xBC
	/* 186 */	{ VC_MINUS,				kVK_Undefined			},	// 0xBD
	/* 187 */	{ VC_PERIOD,			kVK_UpArrow				},	// 0xBE
	/* 188 */	{ VC_SLASH,				kVK_PageUp				},	// 0xBF
	/* 189 */	{ VC_BACKQUOTE,			kVK_LeftArrow			},	// 0xC0
	/* 190 */	{ VC_UNDEFINED,			kVK_RightArrow			},	// 0xC1
	/* 191 */	{ VC_UNDEFINED,			kVK_End					},	// 0xC2
	/* 192 */	{ VC_UNDEFINED,			kVK_DownArrow			},	// 0xC3
	/* 193 */	{ VC_UNDEFINED,			kVK_PageDown			},	// 0xC4
	/* 194 */	{ VC_UNDEFINED,			kVK_Help				},	// 0xC5
	/* 195 */	{ VC_UNDEFINED,			kVK_ForwardDelete		},	// 0xC6
	/* 196 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xC7
	/* 197 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xC8
	/* 198 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xC9
	/* 199 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xCA
	/* 200 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xCB
	/* 201 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xCC
	/* 202 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xCD
	/* 203 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xCE
	/* 204 */	{ VC_UNDEFINED,			kVK_RightCommand		},	// 0xCF
	/* 205 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xD0
	/* 206 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xD1
	/* 207 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xD2
	/* 208 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xD3
	/* 209 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xD4
	/* 210 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xD5
	/* 211 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xD6
	/* 212 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xD7
	/* 213 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xD8
	/* 214 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xD9
	/* 215 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xDA
	/* 216 */	{ VC_OPEN_BRACKET,		kVK_Undefined			},	// 0xDB
	/* 217 */	{ VC_BACK_SLASH,		kVK_Undefined			},	// 0xDC
	/* 218 */	{ VC_CLOSE_BRACKET,		kVK_Undefined			},	// 0xDD
	/* 219 */	{ VC_QUOTE,				kVK_Undefined			},	// 0xDE
	/* 220 */	{ VC_YEN,				kVK_Undefined			},	// 0xDF
	/* 221 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xE0
	/* 222 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xE1
	/* 223 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xE2
	/* 224 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xE3
	/* 225 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xE4
	/* 226 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xE5
	/* 227 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xE6
	/* 228 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xE7
	/* 229 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xE8
	/* 230 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xE9
	/* 231 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xEA
	/* 232 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xEB
	/* 233 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xEC
	/* 234 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xED
	/* 235 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xEE
	/* 236 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xF0
	/* 237 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xF1
	/* 238 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xF2
	/* 239 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xF3
	/* 240 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xF4
	/* 241 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xF5
	/* 242 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xF6
	/* 243 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xF7
	/* 244 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xF8
	/* 245 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xF9
	/* 246 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xFA
	/* 247 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xFB
	/* 249 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xFC
	/* 250 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xFD
	/* 251 */	{ VC_UNDEFINED,			kVK_Undefined			},	// 0xFE
};

uint16_t keycode_to_scancode(UInt64 keycode) {
	uint16_t scancode = VC_UNDEFINED;

	// Bound check 0 <= keycode < 128
	if (keycode < sizeof(keycode_scancode_table) / sizeof(keycode_scancode_table[0])) {
		scancode = keycode_scancode_table[keycode][0];
	}

	return scancode;
}

UInt64 scancode_to_keycode(uint16_t scancode) {
	UInt64 keycode = kVK_Undefined;

	// Bound check 0 <= keycode < 128
	if (scancode < 128) {
		keycode = keycode_scancode_table[scancode][1];
	}
	else {
		// Calculate the upper offset.
		int i = (scancode & 0xFF) + (128 - 13);

		if (i < sizeof(keycode_scancode_table) / sizeof(keycode_scancode_table[0])) {
			keycode = keycode_scancode_table[scancode][1];
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
