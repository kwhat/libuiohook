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

#ifndef _included_input_activity
#define _included_input_activity

#include <stdbool.h>

/* Physical state used for both grab and ungrab boundaries. */
struct input_activity {
    int fd;
    int slot_count;
    bool has_keys;
};

/* Inspect device capabilities; return zero or a negative errno. */
extern int input_activity_init(struct input_activity *activity, int fd);

/* Return 1 when all keys, buttons and contacts are released, 0 when active,
 * or a negative errno when the state cannot be read. */
extern int input_activity_is_idle(const struct input_activity *activity);

/* Wait before grabbing, discarding events already delivered to the desktop.
 * The descriptor must be nonblocking and not yet owned by an event reader.
 * During shutdown use is_idle while the normal reader forwards releases. */
extern int input_activity_wait(const struct input_activity *activity);

#endif
