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

#ifndef _included_touchpad_helper
#define _included_touchpad_helper

#include <linux/input.h>
#include <stdbool.h>

struct touchpad;

/* Handle a translated button event through the hook's shared modifier and consumption logic; return
 * whether it was consumed. */
typedef bool (*touchpad_button_proc)(struct input_event *event);

/* Create a libinput touchpad backend, falling back to observation when virtual forwarding is
 * unavailable; return UIOHOOK_SUCCESS or an error, clearing the output pointer on failure. */
extern int touchpad_create(const char *path, touchpad_button_proc dispatch_button, struct touchpad **touchpad);

/* Release the touchpad backend and virtual mouse, then clear the caller's pointer; accept a null
 * backend. */
extern void touchpad_destroy(struct touchpad **touchpad);

/* Return the libinput descriptor to register with the hook manager's epoll instance. */
extern int touchpad_get_fd(const struct touchpad *touchpad);

/* Return the virtual-device node to exclude from hotplug enumeration, or NULL in observation mode. */
extern const char * touchpad_get_devnode(const struct touchpad *touchpad);

/* Drain and translate pending libinput events; return zero on success or a negative errno on
 * dispatch failure or device removal. */
extern int touchpad_dispatch(struct touchpad *touchpad);

#endif
