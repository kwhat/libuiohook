/* libUIOHook: Cross-platform keyboard and mouse hooking from userland.
 * Copyright (C) 2006-2022 Alexander Barker.  All Rights Reserved.
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
#define Button1     1
#define Button2     2
#define Button3     3
#define WheelUp     4
#define WheelDown   5
#define WheelLeft   6
#define WheelRight  7
#define XButton1    8
#define XButton2    9

// Helper display used by input helper, properties and post event.
extern Display *helper_disp;


/* Converts a X11 key code to the appropriate keyboard scan code. */
extern uint16_t keycode_to_scancode(KeyCode keycode);

/* Converts a keyboard scan code to the appropriate X11 key code. */
extern KeyCode scancode_to_keycode(uint16_t scancode);

/* Converts a X11 key code and event mask to the appropriate X11 key symbol. */
extern KeySym keycode_to_keysym(KeyCode keycode, unsigned int modifier_mask);

/* Set the native modifier mask for future events. */
extern void set_modifier_mask(uint16_t mask);

/* Unset the native modifier mask for future events. */
extern void unset_modifier_mask(uint16_t mask);

/* Get the current native modifier mask state. */
extern uint16_t get_modifiers();

#ifdef USE_EPOCH_TIME
/* Get the current timestamp in unix epoch time. */
extern uint64_t get_unix_timestamp();
#endif

/* Lookup a X11 buttons possible remapping and return that value. */
extern unsigned int button_map_lookup(unsigned int button);

/* Initialize items required for KeyCodeToKeySym() and KeySymToUnicode()
 * functionality.  This method is called by OnLibraryLoad() and may need to be
 * called in combination with UnloadInputHelper() if the native keyboard layout
 * is changed. */
extern void load_input_helper();

/* De-initialize items required for KeyCodeToKeySym() and KeySymToUnicode()
 * functionality.  This method is called by OnLibraryUnload() and may need to be
 * called in combination with LoadInputHelper() if the native keyboard layout
 * is changed. */
extern void unload_input_helper();

#endif
