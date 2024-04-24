/* libUIOHook: Cross-platform keyboard and mouse hooking from userland.
 * Copyright (C) 2006-2023 Alexander Barker.  All Rights Reserved.
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

// Reference: http://boredzo.org/blog/wp-content/uploads/2007/05/imtx-virtual-keycodes.png
// Reference: https://svn.blender.org/svnroot/bf-blender/branches/render25/intern/ghost/intern/GHOST_SystemCocoa.mm
// Reference: http://www.mactech.com/macintosh-c/chap02-1.html

#ifndef _included_input_helper
#define _included_input_helper

#ifdef USE_APPLICATION_SERVICES
#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h> // For HIToolbox kVK_ key codes and TIS functions.
#else
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#endif

#ifdef USE_IOKIT
#include <IOKit/hidsystem/ev_keymap.h>
#endif
#include <stdbool.h>

#ifndef USE_APPLICATION_SERVICES
// If we are not using ApplicationServices, we don't need Carbon.HIToolbox but we still need some of the constants.

/*
 *  Summary:
 *    Virtual keycodes
 *
 *  Discussion:
 *    These constants are the virtual keycodes defined originally in
 *    Inside Mac Volume V, pg. V-191. They identify physical keys on a
 *    keyboard. Those constants with "ANSI" in the name are labeled
 *    according to the key position on an ANSI-standard US keyboard.
 *    For example, kVK_ANSI_A indicates the virtual keycode for the key
 *    with the letter 'A' in the US keyboard layout. Other keyboard
 *    layouts may have the 'A' key label on a different physical key;
 *    in this case, pressing 'A' will generate a different virtual
 *    keycode.
 */
enum {
  kVK_ANSI_A                    = 0x00,
  kVK_ANSI_S                    = 0x01,
  kVK_ANSI_D                    = 0x02,
  kVK_ANSI_F                    = 0x03,
  kVK_ANSI_H                    = 0x04,
  kVK_ANSI_G                    = 0x05,
  kVK_ANSI_Z                    = 0x06,
  kVK_ANSI_X                    = 0x07,
  kVK_ANSI_C                    = 0x08,
  kVK_ANSI_V                    = 0x09,
  kVK_ANSI_B                    = 0x0B,
  kVK_ANSI_Q                    = 0x0C,
  kVK_ANSI_W                    = 0x0D,
  kVK_ANSI_E                    = 0x0E,
  kVK_ANSI_R                    = 0x0F,
  kVK_ANSI_Y                    = 0x10,
  kVK_ANSI_T                    = 0x11,
  kVK_ANSI_1                    = 0x12,
  kVK_ANSI_2                    = 0x13,
  kVK_ANSI_3                    = 0x14,
  kVK_ANSI_4                    = 0x15,
  kVK_ANSI_6                    = 0x16,
  kVK_ANSI_5                    = 0x17,
  kVK_ANSI_Equal                = 0x18,
  kVK_ANSI_9                    = 0x19,
  kVK_ANSI_7                    = 0x1A,
  kVK_ANSI_Minus                = 0x1B,
  kVK_ANSI_8                    = 0x1C,
  kVK_ANSI_0                    = 0x1D,
  kVK_ANSI_RightBracket         = 0x1E,
  kVK_ANSI_O                    = 0x1F,
  kVK_ANSI_U                    = 0x20,
  kVK_ANSI_LeftBracket          = 0x21,
  kVK_ANSI_I                    = 0x22,
  kVK_ANSI_P                    = 0x23,
  kVK_ANSI_L                    = 0x25,
  kVK_ANSI_J                    = 0x26,
  kVK_ANSI_Quote                = 0x27,
  kVK_ANSI_K                    = 0x28,
  kVK_ANSI_Semicolon            = 0x29,
  kVK_ANSI_Backslash            = 0x2A,
  kVK_ANSI_Comma                = 0x2B,
  kVK_ANSI_Slash                = 0x2C,
  kVK_ANSI_N                    = 0x2D,
  kVK_ANSI_M                    = 0x2E,
  kVK_ANSI_Period               = 0x2F,
  kVK_ANSI_Grave                = 0x32,
  kVK_ANSI_KeypadDecimal        = 0x41,
  kVK_ANSI_KeypadMultiply       = 0x43,
  kVK_ANSI_KeypadPlus           = 0x45,
  kVK_ANSI_KeypadClear          = 0x47,
  kVK_ANSI_KeypadDivide         = 0x4B,
  kVK_ANSI_KeypadEnter          = 0x4C,
  kVK_ANSI_KeypadMinus          = 0x4E,
  kVK_ANSI_KeypadEquals         = 0x51,
  kVK_ANSI_Keypad0              = 0x52,
  kVK_ANSI_Keypad1              = 0x53,
  kVK_ANSI_Keypad2              = 0x54,
  kVK_ANSI_Keypad3              = 0x55,
  kVK_ANSI_Keypad4              = 0x56,
  kVK_ANSI_Keypad5              = 0x57,
  kVK_ANSI_Keypad6              = 0x58,
  kVK_ANSI_Keypad7              = 0x59,
  kVK_ANSI_Keypad8              = 0x5B,
  kVK_ANSI_Keypad9              = 0x5C
};

/* keycodes for keys that are independent of keyboard layout*/
enum {
  kVK_Return                    = 0x24,
  kVK_Tab                       = 0x30,
  kVK_Space                     = 0x31,
  kVK_Delete                    = 0x33,
  kVK_Escape                    = 0x35,
  kVK_Command                   = 0x37,
  kVK_Shift                     = 0x38,
  kVK_CapsLock                  = 0x39,
  kVK_Option                    = 0x3A,
  kVK_Control                   = 0x3B,
  kVK_RightShift                = 0x3C,
  kVK_RightOption               = 0x3D,
  kVK_RightControl              = 0x3E,
  kVK_Function                  = 0x3F,
  kVK_F17                       = 0x40,
  kVK_VolumeUp                  = 0x48,
  kVK_VolumeDown                = 0x49,
  kVK_Mute                      = 0x4A,
  kVK_F18                       = 0x4F,
  kVK_F19                       = 0x50,
  kVK_F20                       = 0x5A,
  kVK_F5                        = 0x60,
  kVK_F6                        = 0x61,
  kVK_F7                        = 0x62,
  kVK_F3                        = 0x63,
  kVK_F8                        = 0x64,
  kVK_F9                        = 0x65,
  kVK_F11                       = 0x67,
  kVK_F13                       = 0x69,
  kVK_F16                       = 0x6A,
  kVK_F14                       = 0x6B,
  kVK_F10                       = 0x6D,
  kVK_F12                       = 0x6F,
  kVK_F15                       = 0x71,
  kVK_Help                      = 0x72,
  kVK_Home                      = 0x73,
  kVK_PageUp                    = 0x74,
  kVK_ForwardDelete             = 0x75,
  kVK_F4                        = 0x76,
  kVK_End                       = 0x77,
  kVK_F2                        = 0x78,
  kVK_PageDown                  = 0x79,
  kVK_F1                        = 0x7A,
  kVK_LeftArrow                 = 0x7B,
  kVK_RightArrow                = 0x7C,
  kVK_DownArrow                 = 0x7D,
  kVK_UpArrow                   = 0x7E
};

/* ISO keyboards only*/
enum {
  kVK_ISO_Section               = 0x0A
};

/* JIS keyboards only*/
enum {
  kVK_JIS_Yen                   = 0x5D,
  kVK_JIS_Underscore            = 0x5E,
  kVK_JIS_KeypadComma           = 0x5F,
  kVK_JIS_Eisu                  = 0x66,
  kVK_JIS_Kana                  = 0x68
};

#endif


#ifndef USE_IOKIT
// Some of the system key codes that maybe missing from IOKit.  They appear to have shown up over the years.

#ifndef NX_NOSPECIALKEY
#define NX_NOSPECIALKEY                0xFFFF
#endif

#ifndef NX_KEYTYPE_SOUND_UP
#define NX_KEYTYPE_SOUND_UP            0x00
#endif

#ifndef NX_KEYTYPE_SOUND_DOWN
#define NX_KEYTYPE_SOUND_DOWN          0x01
#endif

#ifndef NX_KEYTYPE_BRIGHTNESS_UP
#define NX_KEYTYPE_BRIGHTNESS_UP       0x02
#endif

#ifndef NX_KEYTYPE_BRIGHTNESS_DOWN
#define NX_KEYTYPE_BRIGHTNESS_DOWN     0x03
#endif

#ifndef NX_KEYTYPE_CAPS_LOCK
#define NX_KEYTYPE_CAPS_LOCK           0x04
#endif

#ifndef NX_KEYTYPE_HELP
#define NX_KEYTYPE_HELP                0x05
#endif

#ifndef NX_POWER_KEY
#define NX_POWER_KEY                   0x06
#endif

#ifndef NX_KEYTYPE_MUTE
#define NX_KEYTYPE_MUTE                0x07
#endif

#ifndef NX_UP_ARROW_KEY
#define NX_UP_ARROW_KEY                0x0B
#endif

#ifndef NX_DOWN_ARROW_KEY
#define NX_DOWN_ARROW_KEY              0x09
#endif

#ifndef NX_KEYTYPE_NUM_LOCK
#define NX_KEYTYPE_NUM_LOCK            0x0A
#endif

#ifndef NX_KEYTYPE_CONTRAST_UP
#define NX_KEYTYPE_CONTRAST_UP         0x0B
#endif

#ifndef NX_KEYTYPE_CONTRAST_DOWN
#define NX_KEYTYPE_CONTRAST_DOWN       0x0C
#endif

#ifndef NX_KEYTYPE_LAUNCH_PANEL
#define NX_KEYTYPE_LAUNCH_PANEL        0x0D
#endif

#ifndef NX_KEYTYPE_EJECT
#define NX_KEYTYPE_EJECT               0x0E
#endif

#ifndef NX_KEYTYPE_VIDMIRROR
#define NX_KEYTYPE_VIDMIRROR           0x0F
#endif

#ifndef NX_KEYTYPE_PLAY
#define NX_KEYTYPE_PLAY                0x10
#endif

#ifndef NX_KEYTYPE_NEXT
#define NX_KEYTYPE_NEXT                0x11
#endif

#ifndef NX_KEYTYPE_PREVIOUS
#define NX_KEYTYPE_PREVIOUS            0x12
#endif

#ifndef NX_KEYTYPE_FAST
#define NX_KEYTYPE_FAST                0x13
#endif

#ifndef NX_KEYTYPE_REWIND
#define NX_KEYTYPE_REWIND              0x14
#endif

#ifndef NX_KEYTYPE_ILLUMINATION_UP
#define NX_KEYTYPE_ILLUMINATION_UP     0x15
#endif

#ifndef NX_KEYTYPE_ILLUMINATION_DOWN
#define NX_KEYTYPE_ILLUMINATION_DOWN   0x16
#endif

#ifndef NX_KEYTYPE_ILLUMINATION_TOGGLE
#define NX_KEYTYPE_ILLUMINATION_TOGGLE 0x17
#endif

#ifndef NX_NUMSPECIALKEYS
#define NX_NUMSPECIALKEYS              0x18 /* Maximum number of special keys */
#endif

#endif


// These virtual key codes do not appear to be defined by Apple.
#define kVK_NX_Power                 0xE0 | NX_POWER_KEY        /* 0xE6 */
#define kVK_NX_Eject                 0xE0 | NX_KEYTYPE_EJECT    /* 0xEE */

#define kVK_MEDIA_Play               0xE0 | NX_KEYTYPE_PLAY     /* 0xF0 */
#define kVK_MEDIA_Next               0xE0 | NX_KEYTYPE_NEXT     /* 0xF1 */
#define kVK_MEDIA_Previous           0xE0 | NX_KEYTYPE_PREVIOUS /* 0xF2 */

#define kVK_RightCommand             0x36
#define kVK_ContextMenu              0x6E // AKA kMenuPowerGlyph
#define kVK_Undefined                0xFF

// These button codes do not appear to be defined by Apple.
#define kVK_LBUTTON                  kCGMouseButtonLeft
#define kVK_RBUTTON                  kCGMouseButtonRight
#define kVK_MBUTTON                  kCGMouseButtonCenter
#define kVK_XBUTTON1                 3
#define kVK_XBUTTON2                 4

// These button masks do not appear to be defined by Apple.
#define kCGEventFlagMaskButtonLeft   1 << 0
#define kCGEventFlagMaskButtonRight  1 << 1
#define kCGEventFlagMaskButtonCenter 1 << 2
#define kCGEventFlagMaskXButton1     1 << 3
#define kCGEventFlagMaskXButton2     1 << 4


/* Check for access to Apples accessibility API. */
extern bool is_accessibility_enabled();

/* Converts an OSX keycode to the appropriate UIOHook scancode constant. */
extern uint16_t keycode_to_uiocode(CGKeyCode keycode);

/* Converts a UIOHook scancode constant to the appropriate OSX keycode. */
extern CGKeyCode uiocode_to_keycode(uint16_t uiocode);

/* Fill the buffer with unicode chars produced by the event_ref, returns how many chars where written */
extern UniCharCount event_to_unicode(CGEventRef event_ref, UniChar *buffer, UniCharCount size);

/* Get objc subtype and data1 fields of a cg event. */
extern void event_to_objc(CGEventRef event_ref, UInt32 *subtype, UInt32 *data1);

/* Set the native modifier mask for future events. */
extern void set_modifier_mask(uint16_t mask);

/* Unset the native modifier mask for future events. */
extern void unset_modifier_mask(uint16_t mask);

/* Get the current native modifier mask state. */
extern uint16_t get_modifiers();

/* Returns a boolean flag that represents whether the mouse is being dragged. */
extern bool is_mouse_dragged();

/* Set a flag that represents whether the mouse is being dragged. */
extern void set_mouse_dragged(bool dragged);

/* Initialize items required for KeyCodeToKeySym() and KeySymToUnicode()
 * functionality.  This method is called by OnLibraryLoad() and may need to be
 * called in combination with UnloadInputHelper() if the native keyboard layout
 * is changed. */
extern int load_input_helper();

/* De-initialize items required for KeyCodeToKeySym() and KeySymToUnicode()
 * functionality.  This method is called by OnLibraryUnload() and may need to be
 * called in combination with LoadInputHelper() if the native keyboard layout
 * is changed. */
extern void unload_input_helper();

#endif
