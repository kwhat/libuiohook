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

#ifndef _included_mouse_helper
#define _included_mouse_helper

#include <stdbool.h>
#include <stdint.h>

/* Query the absolute pointer position through Wayland, falling back to X11; write x/y and return
 * UIOHOOK_SUCCESS on success, otherwise an error code. */
extern int query_pointer_position(int16_t *x, int16_t *y);

/* Compute display bounds and seed the tracked pointer from a native query, falling back to the
 * cached minimum coordinates if the query fails. */
extern void init_pointer_tracking();

/* Add relative motion to the tracked pointer and constrain it to cached display bounds when
 * available. */
extern void track_pointer_delta(int dx, int dy);

/* Set the tracked absolute pointer position, constraining it to cached display bounds when
 * available. */
extern void set_pointer_position(int16_t x, int16_t y);

/* Write the current tracked absolute pointer coordinates to x and y. */
extern void get_pointer_position(int16_t *x, int16_t *y);

/* Return whether motion since the last button press should suppress a synthesized click. */
extern bool is_mouse_dragged(void);

/* Set whether motion since the last button press should suppress a synthesized click. */
extern void set_mouse_dragged(bool dragged);

#endif
