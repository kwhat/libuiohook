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

#ifndef _included_input_helper
#define _included_input_helper

#include <linux/input.h>
#include <stdbool.h>
#include <stdint.h>

/* Event timestamp source: real Unix epoch time, or a monotonic sequence counter. */
#ifdef USE_EPOCH_TIME

/* Convert event seconds and microseconds to Unix epoch milliseconds. */
extern uint64_t get_unix_timestamp(uint64_t seconds, uint64_t microseconds);
#else

/* Return the next sequence timestamp, wrapping to zero when the counter reaches UINT64_MAX. */
extern uint64_t get_seq_timestamp();
#endif

/* Initialize keyboard translation, then compute pointer bounds and seed the tracked pointer
 * position. */
extern void load_input_helper(void);

/* Release the keyboard translation resources owned by the keycode helper. */
extern void unload_input_helper(void);

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

struct libinput_backend;

/* Handle a translated button event through the hook's shared modifier and consumption logic; return
 * whether it was consumed. */
typedef bool (*libinput_backend_button_proc)(struct input_event *event);

/* Create a libinput pointer backend, falling back to observation when virtual forwarding is
 * unavailable; return UIOHOOK_SUCCESS or an error, clearing the output pointer on failure. */
extern int libinput_backend_create(const char *path, libinput_backend_button_proc dispatch_button, struct libinput_backend **backend);

/* Release the libinput backend and virtual mouse, then clear the caller's pointer; accept a null
 * backend. */
extern void libinput_backend_destroy(struct libinput_backend **backend);

/* Return whether the physical input and any forwarded buttons are released. */
extern bool libinput_backend_is_idle(struct libinput_backend *backend);

/* Forward input without dispatching callbacks while a stop is pending. */
extern void libinput_backend_set_passthrough(struct libinput_backend *backend);

/* Return the libinput descriptor to register with the hook manager's epoll instance. */
extern int libinput_backend_get_fd(const struct libinput_backend *backend);

/* Return the virtual-device node to exclude from hotplug enumeration, or NULL in observation mode. */
extern const char * libinput_backend_get_devnode(const struct libinput_backend *backend);

/* Drain and translate pending libinput events; return zero on success or a negative errno on
 * dispatch failure or device removal. */
extern int libinput_backend_dispatch(struct libinput_backend *backend);

#endif
