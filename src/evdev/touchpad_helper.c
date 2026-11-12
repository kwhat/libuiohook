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
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <libinput.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <uiohook.h>
#include <unistd.h>

#include "dispatch_event.h"
#include "input_helper.h"
#include "logger.h"
#include "touchpad_helper.h"

struct touchpad {
    struct libinput *context;
    struct libevdev_uinput *uinput;
    touchpad_button_proc dispatch_button;
    double motion_x, motion_y, scroll_x, scroll_y;
};

// Only grab the device when a virtual mouse can forward its events. Otherwise
// leave desktop input untouched and observe events, as the raw evdev path does.
static int touchpad_open(const char *path, int flags, void *data) {
    struct touchpad *touchpad = data;
    int fd = open(path, flags | O_CLOEXEC);
    if (fd < 0) {
        int error = errno;
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to open touchpad: %s! (%d)\n",
                __FUNCTION__, __LINE__, path, error);
        return -error;
    }

    if (touchpad->uinput != NULL && ioctl(fd, EVIOCGRAB, 1) < 0) {
        int error = errno;
        logger(LOG_LEVEL_WARN, "%s [%u]: Failed to grab touchpad: %s! (%d); observing only.\n",
                __FUNCTION__, __LINE__, path, error);
        libevdev_uinput_destroy(touchpad->uinput);
        touchpad->uinput = NULL;
    }

    return fd;
}

static void touchpad_close(int fd, void *data) {
    struct touchpad *touchpad = data;
    if (touchpad->uinput != NULL) {
        ioctl(fd, EVIOCGRAB, 0);
    }
    close(fd);
}

static const struct libinput_interface touchpad_interface = {
    .open_restricted = touchpad_open,
    .close_restricted = touchpad_close
};

int touchpad_create(const char *path, touchpad_button_proc dispatch_button, struct touchpad **result) {
    *result = NULL;
    struct touchpad *touchpad = calloc(1, sizeof(struct touchpad));
    if (touchpad == NULL) {
        return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }

    touchpad->dispatch_button = dispatch_button;

    struct libevdev *mouse = libevdev_new();
    if (mouse == NULL) {
        free(touchpad);
        return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }

    libevdev_set_name(mouse, "libuiohook touchpad pointer");
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
        error = libevdev_uinput_create_from_device(mouse, LIBEVDEV_UINPUT_OPEN_MANAGED, &touchpad->uinput);
    }

    libevdev_free(mouse);
    if (error < 0) {
        touchpad->uinput = NULL;
        logger(LOG_LEVEL_WARN, "%s [%u]: Failed to create touchpad virtual mouse! (%d); "
                "observing only, event consumption is unavailable.\n",
                __FUNCTION__, __LINE__, error);
    }

    touchpad->context = libinput_path_create_context(&touchpad_interface, touchpad);
    if (touchpad->context == NULL) {
        touchpad_destroy(&touchpad);
        return UIOHOOK_FAILURE;
    }

    struct libinput_device *device = libinput_path_add_device(touchpad->context, path);
    if (device == NULL) {
        touchpad_destroy(&touchpad);
        return UIOHOOK_FAILURE;
    }

    libinput_device_config_tap_set_enabled(device, LIBINPUT_CONFIG_TAP_ENABLED);
    libinput_device_config_scroll_set_method(device, LIBINPUT_CONFIG_SCROLL_2FG);

    logger(LOG_LEVEL_INFO, "%s [%u]: Attached touchpad: %s (%s).\n",
            __FUNCTION__, __LINE__, path,
            touchpad->uinput != NULL ? "forwarding through virtual mouse" : "observing only");

    *result = touchpad;
    return UIOHOOK_SUCCESS;
}

/* Retain fractional motion between events so slow movement is not lost. */
static int touchpad_delta(double delta, double *remainder) {
    *remainder += delta;
    int value = (int) *remainder;
    *remainder -= value;

    return value;
}

static void touchpad_emit(struct touchpad *touchpad, unsigned int type, unsigned int code, int value) {
    if (touchpad->uinput != NULL) {
        libevdev_uinput_write_event(touchpad->uinput, type, code, value);
    }
}

int touchpad_dispatch(struct touchpad *touchpad) {
    int status = libinput_dispatch(touchpad->context);
    if (status < 0) {
        return status;
    }

    struct libinput_event *event;
    while ((event = libinput_get_event(touchpad->context)) != NULL) {
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
                int dx = touchpad_delta(libinput_event_pointer_get_dx(pointer), &touchpad->motion_x);
                int dy = touchpad_delta(libinput_event_pointer_get_dy(pointer), &touchpad->motion_y);
                if (dx != 0 || dy != 0) {
                    track_pointer_delta(dx, dy);
                    if (!dispatch_mouse_move(&ev, dx, dy)) {
                        touchpad_emit(touchpad, EV_REL, REL_X, dx);
                        touchpad_emit(touchpad, EV_REL, REL_Y, dy);
                    }
                }
            } else if (type == LIBINPUT_EVENT_POINTER_BUTTON) {
                ev.type = EV_KEY;
                ev.code = libinput_event_pointer_get_button(pointer);
                ev.value = libinput_event_pointer_get_button_state(pointer) == LIBINPUT_BUTTON_STATE_PRESSED;
                if (!touchpad->dispatch_button(&ev)) {
                    touchpad_emit(touchpad, EV_KEY, ev.code, ev.value);
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
                    double *carry = i == 0 ? &touchpad->scroll_y : &touchpad->scroll_x;
                    if (delta == 0) {
                        *carry = 0;
                        continue;
                    }

                    // libinput's conventional wheel step is 15 scroll units.
                    int steps = touchpad_delta(delta / 15.0, carry);
                    // Match evdev: positive vertical wheel values scroll up.
                    if (i == 0) {
                        steps = -steps;
                    }
                    if (steps != 0 && !dispatch_mouse_wheel(&ev, steps,
                            i == 0 ? WHEEL_VERTICAL_DIRECTION : WHEEL_HORIZONTAL_DIRECTION)) {
                        touchpad_emit(touchpad, EV_REL, i == 0 ? REL_WHEEL : REL_HWHEEL, steps);
                    }
                }
            }
            touchpad_emit(touchpad, EV_SYN, SYN_REPORT, 0);
        }

        libinput_event_destroy(event);
    }

    return 0;
}

void touchpad_destroy(struct touchpad **touchpad) {
    if (*touchpad != NULL) {
        // libinput closes its descriptors while the forwarding state still exists.
        if ((*touchpad)->context != NULL) {
            libinput_unref((*touchpad)->context);
        }
        if ((*touchpad)->uinput != NULL) {
            libevdev_uinput_destroy((*touchpad)->uinput);
        }
        free(*touchpad);
        *touchpad = NULL;
    }
}

int touchpad_get_fd(const struct touchpad *touchpad) {
    return libinput_get_fd(touchpad->context);
}

const char * touchpad_get_devnode(const struct touchpad *touchpad) {
    return touchpad->uinput != NULL ? libevdev_uinput_get_devnode(touchpad->uinput) : NULL;
}
