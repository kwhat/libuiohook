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

// Reference: http://boredzo.org/blog/wp-content/uploads/2007/05/imtx-virtual-keycodes.png
// Reference: https://svn.blender.org/svnroot/bf-blender/branches/render25/intern/ghost/intern/GHOST_SystemCocoa.mm
// Reference: http://www.mactech.com/macintosh-c/chap02-1.html

#ifndef _included_input_helper
#define _included_input_helper

#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>	// For HIToolbox kVK_ keycodes and TIS funcitons.
#include <stdbool.h>

// These virtual key codes do not appear to be defined anywhere by Apple.
#define kVK_RightCommand				0x36
#define kVK_ContextMenu					0x6E	// AKA kMenuPowerGlyph
#define kVK_Undefined					0xFF

// These button codes do not appear to be defined anywhere by Apple.
#define kVK_LBUTTON						kCGMouseButtonLeft
#define kVK_RBUTTON						kCGMouseButtonRight
#define kVK_MBUTTON						kCGMouseButtonCenter
#define kVK_XBUTTON1					3
#define kVK_XBUTTON2					4

// These button masks do not appear to be defined anywhere by Apple.
#define kCGEventFlagMaskButtonLeft		1 << 0
#define kCGEventFlagMaskButtonRight		1 << 1
#define kCGEventFlagMaskButtonCenter	1 << 2
#define kCGEventFlagMaskXButton1		1 << 3
#define kCGEventFlagMaskXButton2		1 << 4

/* Converts an OSX key code and event mask to the appropriate Unicode character
 * representation.
 */
extern UniCharCount keycode_to_unicode(CGEventRef event_ref, UniChar *buffer, UniCharCount size);

/* Converts an OSX keycode to the appropriate UIOHook scancode constant.
 */
extern uint16_t keycode_to_scancode(UInt64 keycode);

/* Converts a UIOHook scancode constant to the appropriate OSX keycode.
 */
extern UInt64 scancode_to_keycode(uint16_t keycode);

/* Check for access to Apples accessibility API.
 */
extern bool is_accessibility_enabled();

/* Initialize items required for KeyCodeToKeySym() and KeySymToUnicode()
 * functionality.  This method is called by OnLibraryLoad() and may need to be
 * called in combination with UnloadInputHelper() if the native keyboard layout
 * is changed.
 */
extern void load_input_helper();

/* De-initialize items required for KeyCodeToKeySym() and KeySymToUnicode()
 * functionality.  This method is called by OnLibraryUnload() and may need to be
 * called in combination with LoadInputHelper() if the native keyboard layout
 * is changed.
 */
extern void unload_input_helper();

#endif
