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
#include <stdint.h>

struct touchpad;

// Use the hook's shared button handler for modifier state and event consumption.
// Source flags describe this event only and must not enter shared modifier state.
typedef bool (*touchpad_button_proc)(struct input_event *event, uint32_t source_mask);

// Creates a touchpad backend, falling back to observation if forwarding is
// unavailable. On failure, resources are released and *touchpad is NULL.
extern int touchpad_create(const char *path, touchpad_button_proc dispatch_button, struct touchpad **touchpad);

// Releases the libinput context and virtual mouse, and clears *touchpad.
extern void touchpad_destroy(struct touchpad **touchpad);

// Descriptor registered with the hook manager's epoll instance.
extern int touchpad_get_fd(const struct touchpad *touchpad);

// Virtual-device node to exclude from hotplug enumeration, or NULL when observing.
extern const char * touchpad_get_devnode(const struct touchpad *touchpad);

// Drains translated events. Returns 0 on success or a negative errno on failure
// or device removal, allowing the hook manager to detach the device.
extern int touchpad_dispatch(struct touchpad *touchpad);

#endif
