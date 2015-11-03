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

#ifndef _included_input_helper
#define _included_input_helper

#include <stdint.h>
#include <X11/Xlib.h>

// Virtual button codes that are not defined by X11.
#define Button1			1
#define Button2			2
#define Button3			3
#define WheelUp			4
#define WheelDown		5
#define WheelLeft		6
#define WheelRight		7
#define XButton1		8
#define XButton2		9

// Structure to represent screen info.
// NOTE This has been moved to public API as of 1.1.
typedef struct _screen_data {
	uint8_t number;
	int16_t x;
	int16_t y;
	uint16_t width;
	uint16_t height;
} screen_data;

/* Create an array of screen_data structures and return the number of
 * populated elements to count.  You are responsible for freeing the returned
 * memory.
 * NOTE This has been moved to public API as of 1.1.
 */
extern screen_data* hook_create_screen_info(uint8_t *count);

/* Converts an X11 key symbol to a single Unicode character.  No direct X11
 * functionality exists to provide this information.
 */
extern size_t keysym_to_unicode(KeySym keysym, wchar_t *buffer, size_t size);

/* Convert a single Unicode character to an X11 key symbol.  This function
 * provides a better translation than XStringToKeysym() for Unicode characters.
 */
extern KeySym unicode_to_keysym(wchar_t unicode);

/* Converts an X11 key code to the appropriate keyboard scan code.
 */
extern uint16_t keycode_to_scancode(KeyCode keycode);

/* Converts a keyboard scan code to the appropriate X11 key code.
 */
extern KeyCode scancode_to_keycode(uint16_t scancode);

/* Converts an X11 key code and event mask to the appropriate X11 key symbol.
 * This functions in much the same way as XKeycodeToKeysym() but allows for a
 * faster and more flexible lookup.
 */
extern KeySym keycode_to_keysym(KeyCode keycode, unsigned int modifier_mask);

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
