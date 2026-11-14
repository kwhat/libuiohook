/* libUIOHook: Cross-platform keyboard and mouse hooking from userland.
 * Copyright (C) 2006-2026 Alexander Barker.  All Rights Reserved.
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

#ifndef _included_keycode_helper
#define _included_keycode_helper

#include <stddef.h>
#include <stdint.h>
#include <wchar.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>

// TODO It's unclear if these XKB_KEY's should still be used.
#ifndef XKB_KEY_osfPrior
#define XKB_KEY_osfPrior 0x1004FF55
#endif

#ifndef XKB_KEY_osfNext
#define XKB_KEY_osfNext 0x1004FF56
#endif

#ifndef XKB_KEY_apCopy
#define XKB_KEY_apCopy 0x1000FF02
#endif

#ifndef XKB_KEY_apCut
#define XKB_KEY_apCut 0x1000FF03
#endif

#ifndef XKB_KEY_apPaste
#define XKB_KEY_apPaste 0x1000FF04
#endif

/* The meaning of the input_event 'value' field. */
enum xkb_key_state_t {
    KEY_STATE_RELEASE = 0,
    KEY_STATE_PRESS = 1,
    KEY_STATE_REPEAT = 2
};

/* Map an XKB keysym to a uiohook virtual code, marking modifier keys as pressed and tagging keypad
 * navigation when Num Lock is off; return VC_UNDEFINED for an unknown symbol. */
extern uint16_t keysym_to_uiocode(xkb_keysym_t keysym);

/* Find an XKB keycode producing the virtual code at the base keymap level; return zero if no
 * mapping or keymap is available. */
extern xkb_keycode_t uiocode_to_keycode(uint16_t uiocode);

/* Write up to two UTF-16 code units into the wchar_t buffer using the current XKB state; return the
 * units written, or zero if conversion fails or the buffer is too small. */
extern size_t keycode_to_utf8(xkb_keycode_t keycode, wchar_t *surrogate, size_t length);

/* Convert an evdev key code to an XKB keycode by adding the minimum legal XKB keycode offset. */
extern xkb_keycode_t event_to_keycode(uint16_t code);

/* Convert an XKB keycode to an evdev key code; return zero for a keycode below the minimum legal
 * offset. */
extern uint16_t keycode_to_event(xkb_keycode_t keycode);

/* Read the key symbol using the current XKB state, then update key, compose, and lock state for the
 * event; return XKB_KEY_NoSymbol if no state is available. */
extern xkb_keysym_t event_to_keysym(xkb_keycode_t keycode, enum xkb_key_state_t key_state);

/* Create XKB context and compose resources, choose a Wayland, X11, or default keymap, and
 * initialize key and lock state. */
extern void load_keycode_helper(void);

/* Release the XKB key state, keymap, compose table, and context, and clear the borrowed X11 display
 * pointer. */
extern void unload_keycode_helper(void);

#endif
