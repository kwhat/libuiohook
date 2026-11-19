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

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <libinput.h>
#include <poll.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <uiohook.h>
#include <unistd.h>

#include "dispatch_event.h"
#include "input_helper.h"
#include "logger.h"
#include "keycode_helper.h"
#include "mouse_helper.h"

#ifdef USE_EPOCH_TIME
/* Get the current timestamp in unix epoch time. */
uint64_t get_unix_timestamp(uint64_t seconds, uint64_t microseconds) {
    // Convert the event time to a Unix epoch in MS.
    uint64_t timestamp = (seconds * 1000) + (microseconds / 1000);

    return timestamp;
}
#else
static uint64_t seq_timestamp = 0;
uint64_t get_seq_timestamp() {
    if (seq_timestamp == UINT64_MAX) {
        // TODO Warning
        seq_timestamp = 0;
    }
    return seq_timestamp++;
}
#endif

void load_input_helper(void) {
    if (load_keycode_helper()) {
        init_pointer_tracking();
    }
}

void unload_input_helper(void) {
    unload_keycode_helper();
}

#define BIT_WORDS(count) (((count) + sizeof(unsigned long) * 8 - 1) / (sizeof(unsigned long) * 8))

static bool bit_is_set(const unsigned long *bits, unsigned int bit) {
    const unsigned int width = sizeof(*bits) * 8;
    return (bits[bit / width] & (1UL << (bit % width))) != 0;
}

int input_activity_init(struct input_activity *activity, int fd) {
    *activity = (struct input_activity) { .fd = fd };
    unsigned long types[BIT_WORDS(EV_CNT)] = { 0 };
    if (ioctl(fd, EVIOCGBIT(0, sizeof(types)), types) < 0) {
        return -errno;
    }
    activity->has_keys = bit_is_set(types, EV_KEY);

    if (bit_is_set(types, EV_ABS)) {
        unsigned long axes[BIT_WORDS(ABS_CNT)] = { 0 };
        if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(axes)), axes) < 0) {
            return -errno;
        }
        if (bit_is_set(axes, ABS_MT_SLOT)) {
            struct input_absinfo slots;
            if (ioctl(fd, EVIOCGABS(ABS_MT_SLOT), &slots) < 0) {
                return -errno;
            }
            if (slots.maximum < 0 || slots.maximum == INT_MAX) {
                return -EINVAL;
            }
            activity->slot_count = slots.maximum + 1;
        }
    }
    return 0;
}

int input_activity_is_idle(const struct input_activity *activity) {
    if (activity->has_keys) {
        unsigned long keys[BIT_WORDS(KEY_CNT)] = { 0 };
        if (ioctl(activity->fd, EVIOCGKEY(sizeof(keys)), keys) < 0) {
            return -errno;
        }
        for (unsigned int code = 0; code <= KEY_MAX; code++) {
            if (bit_is_set(keys, code)) {
                return 0;
            }
        }
    }

    if (activity->slot_count > 0) {
        size_t count = (size_t) activity->slot_count + 1;
        int32_t *values = calloc(count, sizeof(*values));
        if (values == NULL) {
            return -ENOMEM;
        }
        values[0] = ABS_MT_TRACKING_ID;
        int status = ioctl(activity->fd, EVIOCGMTSLOTS(count * sizeof(*values)), values);
        if (status < 0) {
            int error = errno;
            free(values);
            return -error;
        }
        bool active = false;
        for (size_t i = 1; i < count; i++) {
            active |= values[i] >= 0;
        }
        free(values);
        if (active) {
            return 0;
        }
    }
    return 1;
}

int input_activity_wait(const struct input_activity *activity) {
    for (;;) {
        // Drain pre-grab events before testing state so they cannot be replayed
        // through the clone after the desktop has already received them.
        struct input_event events[32];
        ssize_t size;
        do {
            size = read(activity->fd, events, sizeof(events));
        } while (size > 0 || (size < 0 && errno == EINTR));
        if (size == 0) {
            return -ENODEV;
        }
        if (errno != EAGAIN) {
            return -errno;
        }

        int idle = input_activity_is_idle(activity);
        if (idle != 0) {
            return idle < 0 ? idle : 0;
        }

        struct pollfd event = { .fd = activity->fd, .events = POLLIN };
        int status = poll(&event, 1, -1);
        if (status < 0 && errno != EINTR) {
            return -errno;
        }
        if (status > 0 && (event.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            return -ENODEV;
        }
    }
}

struct libinput_backend {
    struct libinput *context;
    struct libevdev_uinput *uinput;
    libinput_backend_button_proc dispatch_button;
    int fd;
    struct input_activity activity;
    unsigned int buttons_down;
    bool grabbed;
    bool passthrough;
    bool activity_query_error_logged;
    double motion_x, motion_y, scroll_x, scroll_y;
};

// Only grab the device when a virtual mouse can forward its events. Otherwise
// leave desktop input untouched and observe events, as the raw evdev path does.
static int libinput_backend_open(const char *path, int flags, void *data) {
    struct libinput_backend *backend = data;
    int fd = open(path, flags | O_CLOEXEC | O_NONBLOCK);
    if (fd < 0) {
        int error = errno;
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to open libinput device: %s! (%d)\n",
                __FUNCTION__, __LINE__, path, error);
        return -error;
    }

    backend->fd = fd;
    if (backend->uinput != NULL) {
        if (input_activity_init(&backend->activity, fd) < 0
                || input_activity_wait(&backend->activity) < 0) {
            logger(LOG_LEVEL_WARN, "%s [%u]: Cannot wait for idle input device %s; observing only.\n",
                    __FUNCTION__, __LINE__, path);
            libevdev_uinput_destroy(backend->uinput);
            backend->uinput = NULL;
        }
    }

    if (backend->uinput != NULL && ioctl(fd, EVIOCGRAB, 1) < 0) {
        int error = errno;
        logger(LOG_LEVEL_WARN, "%s [%u]: Failed to grab libinput device: %s! (%d); observing only.\n",
                __FUNCTION__, __LINE__, path, error);
        libevdev_uinput_destroy(backend->uinput);
        backend->uinput = NULL;
    } else if (backend->uinput != NULL) {
        backend->grabbed = true;

        if (input_activity_is_idle(&backend->activity) != 1) {
            logger(LOG_LEVEL_WARN, "%s [%u]: Input device became active while grabbing %s; observing only.\n",
                    __FUNCTION__, __LINE__, path);
            ioctl(fd, EVIOCGRAB, 0);
            backend->grabbed = false;
            libevdev_uinput_destroy(backend->uinput);
            backend->uinput = NULL;
        }
    }

    return fd;
}

static void libinput_backend_close(int fd, void *data) {
    struct libinput_backend *backend = data;
    if (backend->grabbed) {
        ioctl(fd, EVIOCGRAB, 0);
        backend->grabbed = false;
    }
    if (backend->fd == fd) {
        backend->fd = -1;
    }
    close(fd);
}

static const struct libinput_interface libinput_backend_interface = {
    .open_restricted = libinput_backend_open,
    .close_restricted = libinput_backend_close
};

int libinput_backend_create(const char *path, libinput_backend_button_proc dispatch_button, struct libinput_backend **result) {
    *result = NULL;
    struct libinput_backend *backend = calloc(1, sizeof(struct libinput_backend));
    if (backend == NULL) {
        return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }

    backend->dispatch_button = dispatch_button;
    backend->fd = -1;

    struct libevdev *mouse = libevdev_new();
    if (mouse == NULL) {
        free(backend);
        return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }

    libevdev_set_name(mouse, "libuiohook libinput pointer");
    libevdev_set_id_bustype(mouse, BUS_VIRTUAL);

    const int codes[] = { REL_X, REL_Y, REL_WHEEL, REL_HWHEEL };
    int error = 0;
    for (unsigned int i = 0; i < sizeof(codes) / sizeof(codes[0]); i++) {
        if (libevdev_enable_event_code(mouse, EV_REL, codes[i], NULL) < 0) {
            error = -1;
        }
    }

    for (int code = BTN_LEFT; code <= BTN_MIDDLE; code++) {
        if (libevdev_enable_event_code(mouse, EV_KEY, code, NULL) < 0) {
            error = -1;
        }
    }

    if (error == 0) {
        error = libevdev_uinput_create_from_device(mouse, LIBEVDEV_UINPUT_OPEN_MANAGED, &backend->uinput);
    }

    libevdev_free(mouse);
    if (error < 0) {
        backend->uinput = NULL;
        logger(LOG_LEVEL_WARN, "%s [%u]: Failed to create libinput virtual mouse! (%d); "
                "observing only, event consumption is unavailable.\n",
                __FUNCTION__, __LINE__, error);
    }

    backend->context = libinput_path_create_context(&libinput_backend_interface, backend);
    if (backend->context == NULL) {
        libinput_backend_destroy(&backend);
        return UIOHOOK_FAILURE;
    }

    struct libinput_device *device = libinput_path_add_device(backend->context, path);
    if (device == NULL) {
        libinput_backend_destroy(&backend);
        return UIOHOOK_FAILURE;
    }

    libinput_device_config_tap_set_enabled(device, LIBINPUT_CONFIG_TAP_ENABLED);
    libinput_device_config_scroll_set_method(device, LIBINPUT_CONFIG_SCROLL_2FG);

    logger(LOG_LEVEL_INFO, "%s [%u]: Attached libinput device: %s (%s).\n",
            __FUNCTION__, __LINE__, path,
            backend->uinput != NULL ? "forwarding through virtual mouse" : "observing only");

    *result = backend;

    return UIOHOOK_SUCCESS;
}

/* Retain fractional motion between events so slow movement is not lost. */
static int libinput_backend_delta(double delta, double *remainder) {
    *remainder += delta;
    int value = (int) *remainder;
    *remainder -= value;

    return value;
}

static void libinput_backend_emit(struct libinput_backend *backend, unsigned int type, unsigned int code, int value) {
    if (backend->uinput != NULL) {
        if (libevdev_uinput_write_event(backend->uinput, type, code, value) == 0
                && type == EV_KEY && code >= BTN_LEFT && code <= BTN_MIDDLE) {
            unsigned int bit = 1U << (code - BTN_LEFT);
            if (value != 0) {
                backend->buttons_down |= bit;
            } else {
                backend->buttons_down &= ~bit;
            }
        }
    }
}

int libinput_backend_dispatch(struct libinput_backend *backend) {
    int status = libinput_dispatch(backend->context);
    if (status < 0) {
        return status;
    }

    struct libinput_event *event;
    while ((event = libinput_get_event(backend->context)) != NULL) {
        enum libinput_event_type type = libinput_event_get_type(event);
        if (type == LIBINPUT_EVENT_DEVICE_REMOVED) {
            libinput_event_destroy(event);
            return -ENODEV;
        }

        // Use only the legacy axis event, which libinput also emits alongside
        // newer scroll events. Processing both would double the scroll distance.
        if (type == LIBINPUT_EVENT_POINTER_MOTION || type == LIBINPUT_EVENT_POINTER_BUTTON
                || type == LIBINPUT_EVENT_POINTER_AXIS) {
            struct libinput_event_pointer *pointer = libinput_event_get_pointer_event(event);
            struct input_event ev = { 0 };

            // libinput timestamps are monotonic; dispatch expects wall time when
            // USE_EPOCH_TIME is enabled, just like the existing evdev path.
            struct timeval timestamp;
            gettimeofday(&timestamp, NULL);
            ev.input_event_sec = timestamp.tv_sec;
            ev.input_event_usec = timestamp.tv_usec;

            if (type == LIBINPUT_EVENT_POINTER_MOTION) {
                int dx = libinput_backend_delta(libinput_event_pointer_get_dx(pointer), &backend->motion_x);
                int dy = libinput_backend_delta(libinput_event_pointer_get_dy(pointer), &backend->motion_y);
                if (dx != 0 || dy != 0) {
                    track_pointer_delta(dx, dy);
                    if (backend->passthrough || !dispatch_mouse_move(&ev, dx, dy)) {
                        libinput_backend_emit(backend, EV_REL, REL_X, dx);
                        libinput_backend_emit(backend, EV_REL, REL_Y, dy);
                    }
                }
            } else if (type == LIBINPUT_EVENT_POINTER_BUTTON) {
                ev.type = EV_KEY;
                ev.code = libinput_event_pointer_get_button(pointer);
                ev.value = libinput_event_pointer_get_button_state(pointer) == LIBINPUT_BUTTON_STATE_PRESSED;
                if (backend->passthrough || !backend->dispatch_button(&ev)) {
                    libinput_backend_emit(backend, EV_KEY, ev.code, ev.value);
                }
            } else {
                const enum libinput_pointer_axis axes[] = {
                    LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL,
                    LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL
                };
                for (int i = 0; i < 2; i++) {
                    if (!libinput_event_pointer_has_axis(pointer, axes[i])) {
                        continue;
                    }
                    double delta = libinput_event_pointer_get_axis_value(pointer, axes[i]);
                    double *carry = i == 0 ? &backend->scroll_y : &backend->scroll_x;
                    if (delta == 0) {
                        *carry = 0;
                        continue;
                    }

                    // libinput's conventional wheel step is 15 scroll units.
                    int steps = libinput_backend_delta(delta / 15.0, carry);
                    // Match evdev: positive vertical wheel values scroll up.
                    if (i == 0) {
                        steps = -steps;
                    }
                    if (steps != 0 && (backend->passthrough || !dispatch_mouse_wheel(&ev, steps,
                            i == 0 ? WHEEL_VERTICAL_DIRECTION : WHEEL_HORIZONTAL_DIRECTION))) {
                        libinput_backend_emit(backend, EV_REL, i == 0 ? REL_WHEEL : REL_HWHEEL, steps);
                    }
                }
            }
            libinput_backend_emit(backend, EV_SYN, SYN_REPORT, 0);
        }

        libinput_event_destroy(event);
    }

    return 0;
}

void libinput_backend_destroy(struct libinput_backend **backend) {
    if (*backend != NULL) {
        // libinput closes its descriptors while the forwarding state still exists.
        if ((*backend)->context != NULL) {
            libinput_unref((*backend)->context);
        }
        if ((*backend)->uinput != NULL) {
            libevdev_uinput_destroy((*backend)->uinput);
        }
        free(*backend);
        *backend = NULL;
    }
}

bool libinput_backend_is_idle(struct libinput_backend *backend) {
    if (backend == NULL || !backend->grabbed) {
        return true;
    }

    int idle = input_activity_is_idle(&backend->activity);
    if (idle < 0) {
        if (!backend->activity_query_error_logged) {
            logger(LOG_LEVEL_WARN, "%s [%u]: Failed to inspect input state; keeping device grabbed.\n",
                    __FUNCTION__, __LINE__);
            backend->activity_query_error_logged = true;
        }
        return false;
    }

    // libinput may schedule a tap release after physical contacts have ended.
    return idle == 1 && backend->buttons_down == 0;
}

void libinput_backend_set_passthrough(struct libinput_backend *backend) {
    if (backend != NULL) {
        backend->passthrough = true;
    }
}

int libinput_backend_get_fd(const struct libinput_backend *backend) {
    return libinput_get_fd(backend->context);
}

const char * libinput_backend_get_devnode(const struct libinput_backend *backend) {
    return backend->uinput != NULL ? libevdev_uinput_get_devnode(backend->uinput) : NULL;
}
