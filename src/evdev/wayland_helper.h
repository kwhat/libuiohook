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

#ifndef _included_wayland_helper
#define _included_wayland_helper

#include <stdint.h>
#include <uiohook.h>

// Enumerate monitors via the Wayland xdg-output protocol.  libwayland-client is loaded
// with dlopen() so it is a runtime, not a link-time, dependency; the library still builds
// and starts where Wayland is absent.  Returns a malloc'd screen_data array (the caller
// frees it) and sets count, or NULL when Wayland or the xdg-output protocol is
// unavailable.  Connects and disconnects per call.
extern screen_data *wayland_create_screen_info(unsigned char *count);

// Read the compositor's active keymap from a wl_seat/wl_keyboard as an
// XKB_KEYMAP_FORMAT_TEXT_V1 string (the caller frees it), or NULL when unavailable.  If
// repeat_rate / repeat_delay are non-NULL they receive the wl_keyboard repeat_info values
// (rate in keys per second, delay in ms), each -1 when the compositor does not report them.
extern char *wayland_get_keymap(int32_t *repeat_rate, int32_t *repeat_delay);

// Read just the wl_keyboard key-repeat settings (rate in keys per second, delay in ms).
// Returns UIOHOOK_SUCCESS when at least one value was reported, otherwise UIOHOOK_FAILURE.
extern int wayland_get_repeat_info(int32_t *repeat_rate, int32_t *repeat_delay);

// Read the current absolute pointer position (global layout coordinates) by briefly
// mapping a transparent full-screen zwlr_layer_shell_v1 overlay on every output and
// reading the wl_pointer.enter it produces (surface-local coords + the output's xdg-output
// origin).  One-shot; used to seed the self-tracker at startup.  Returns UIOHOOK_SUCCESS
// and sets x/y, or UIOHOOK_FAILURE when layer-shell is unavailable or no enter arrived.
extern int wayland_query_pointer_position(int16_t *x, int16_t *y);

#endif
