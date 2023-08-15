/* libUIOHook: Cross-platform keyboard and mouse hooking from userland.
 * Copyright (C) 2006-2024 Alexander Barker.  All Rights Reserved.
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
#include <xkbcommon/xkbcommon.h>
#include <sys/time.h>

/* The meaning of the input_event 'value' field. */
enum xkb_key_state_t {
    KEY_STATE_RELEASE = 0,
    KEY_STATE_PRESS = 1,
    KEY_STATE_REPEAT = 2
};

// Helper display used by input helper, properties and post event.
#ifdef USE_EPOCH_TIME
extern uint64_t get_unix_timestamp(struct timeval *event_time);
#else
extern uint64_t get_seq_timestamp();
#endif

/* Converts a XKB key code to the appropriate uiohook virtual code. */
extern uint16_t keysym_to_uiocode(xkb_keysym_t keysym);

/* Converts a uiohook virtual code to the appropriate XKB key code. */
extern xkb_keycode_t uiocode_to_keycode(uint16_t uiocode);

/* FIXME Write Doc */
extern size_t keycode_to_utf8(xkb_keycode_t keycode, wchar_t *surrogate, size_t length);

/* FIXME Write Doc */
extern xkb_keycode_t event_to_keycode(uint16_t code);

/* FIXME Write Doc */
extern xkb_keysym_t event_to_keysym(xkb_keycode_t keycode, enum xkb_key_state_t key_state);

/* Set the native modifier mask for future events. */
extern void set_modifier_mask(uint16_t mask);

/* Unset the native modifier mask for future events. */
extern void unset_modifier_mask(uint16_t mask);

/* Get the current native modifier mask state. */
extern uint16_t get_modifiers();

/* Enable detectable auto-repeat for keys */
extern bool enable_key_repeat();

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
