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
#include "mouse_helper.h"
#include "touchpad_helper.h"

struct touchpad {
    struct libinput *context;
    struct libevdev_uinput *uinput;
    touchpad_button_proc dispatch_button;
    int fd;
    int slot_count;
    uint32_t touch_key_mask;
    bool grabbed;
    bool passthrough;
    bool contact_query_error_logged;
    double motion_x, motion_y, scroll_x, scroll_y;
};

static const unsigned int touch_keys[] = {
    BTN_TOUCH,
    BTN_TOOL_FINGER,
    BTN_TOOL_DOUBLETAP,
    BTN_TOOL_TRIPLETAP,
    BTN_TOOL_QUADTAP,
    BTN_TOOL_QUINTTAP
};

static bool bit_is_set(const unsigned long *bits, unsigned int bit) {
    const unsigned int bits_per_word = sizeof(*bits) * 8;
    return (bits[bit / bits_per_word] & (1UL << (bit % bits_per_word))) != 0;
}

static int touchpad_read_contact_state(const struct touchpad *touchpad, int fd, bool *active);

static int touchpad_configure_contact_query(struct touchpad *touchpad, int fd, bool *active) {
    struct input_absinfo slots;
    if (ioctl(fd, EVIOCGABS(ABS_MT_SLOT), &slots) == 0 && slots.maximum >= 0) {
        touchpad->slot_count = slots.maximum + 1;
    }

    unsigned long key_capabilities[(KEY_MAX + 1 + sizeof(unsigned long) * 8 - 1)
            / (sizeof(unsigned long) * 8)] = { 0 };
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_capabilities)), key_capabilities) == 0) {
        for (unsigned int i = 0; i < sizeof(touch_keys) / sizeof(touch_keys[0]); i++) {
            if (bit_is_set(key_capabilities, touch_keys[i])) {
                touchpad->touch_key_mask |= 1U << i;
            }
        }
    }

    if (touchpad->slot_count == 0 && touchpad->touch_key_mask == 0) {
        return -ENOTSUP;
    }

    return touchpad_read_contact_state(touchpad, fd, active);
}

static int touchpad_read_contact_state(const struct touchpad *touchpad, int fd, bool *active) {
    *active = false;

    if (touchpad->slot_count > 0) {
        size_t count = (size_t) touchpad->slot_count + 1;
        int32_t *values = calloc(count, sizeof(int32_t));
        if (values == NULL) {
            return -ENOMEM;
        }

        values[0] = ABS_MT_TRACKING_ID;
        int status = ioctl(fd, EVIOCGMTSLOTS(count * sizeof(int32_t)), values);
        if (status == 0) {
            for (size_t i = 1; i < count; i++) {
                if (values[i] >= 0) {
                    *active = true;
                    break;
                }
            }
        } else if (touchpad->touch_key_mask == 0) {
            int error = errno;
            free(values);
            return -error;
        }

        free(values);
    }

    if (touchpad->touch_key_mask != 0) {
        unsigned long key_states[(KEY_MAX + 1 + sizeof(unsigned long) * 8 - 1)
                / (sizeof(unsigned long) * 8)] = { 0 };
        if (ioctl(fd, EVIOCGKEY(sizeof(key_states)), key_states) < 0) {
            return -errno;
        }

        for (unsigned int i = 0; i < sizeof(touch_keys) / sizeof(touch_keys[0]); i++) {
            if ((touchpad->touch_key_mask & (1U << i)) != 0
                    && bit_is_set(key_states, touch_keys[i])) {
                *active = true;
                break;
            }
        }
    }

    return 0;
}

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

    touchpad->fd = fd;
    if (touchpad->uinput != NULL) {
        bool active = false;
        if (touchpad_configure_contact_query(touchpad, fd, &active) < 0 || active) {
            logger(LOG_LEVEL_WARN, "%s [%u]: Cannot safely grab active touchpad %s; observing only.\n",
                    __FUNCTION__, __LINE__, path);
            libevdev_uinput_destroy(touchpad->uinput);
            touchpad->uinput = NULL;
        }
    }

    if (touchpad->uinput != NULL && ioctl(fd, EVIOCGRAB, 1) < 0) {
        int error = errno;
        logger(LOG_LEVEL_WARN, "%s [%u]: Failed to grab touchpad: %s! (%d); observing only.\n",
                __FUNCTION__, __LINE__, path, error);
        libevdev_uinput_destroy(touchpad->uinput);
        touchpad->uinput = NULL;
    } else if (touchpad->uinput != NULL) {
        touchpad->grabbed = true;

        bool active = false;
        if (touchpad_read_contact_state(touchpad, fd, &active) < 0 || active) {
            logger(LOG_LEVEL_WARN, "%s [%u]: Touchpad became active while grabbing %s; observing only.\n",
                    __FUNCTION__, __LINE__, path);
            ioctl(fd, EVIOCGRAB, 0);
            touchpad->grabbed = false;
            libevdev_uinput_destroy(touchpad->uinput);
            touchpad->uinput = NULL;
        }
    }

    return fd;
}

static void touchpad_close(int fd, void *data) {
    struct touchpad *touchpad = data;
    if (touchpad->grabbed) {
        ioctl(fd, EVIOCGRAB, 0);
        touchpad->grabbed = false;
    }
    if (touchpad->fd == fd) {
        touchpad->fd = -1;
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
    touchpad->fd = -1;

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
                    if (touchpad->passthrough || !dispatch_mouse_move(&ev, dx, dy)) {
                        touchpad_emit(touchpad, EV_REL, REL_X, dx);
                        touchpad_emit(touchpad, EV_REL, REL_Y, dy);
                    }
                }
            } else if (type == LIBINPUT_EVENT_POINTER_BUTTON) {
                ev.type = EV_KEY;
                ev.code = libinput_event_pointer_get_button(pointer);
                ev.value = libinput_event_pointer_get_button_state(pointer) == LIBINPUT_BUTTON_STATE_PRESSED;
                if (touchpad->passthrough || !touchpad->dispatch_button(&ev)) {
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
                    if (steps != 0 && (touchpad->passthrough || !dispatch_mouse_wheel(&ev, steps,
                            i == 0 ? WHEEL_VERTICAL_DIRECTION : WHEEL_HORIZONTAL_DIRECTION))) {
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

bool touchpad_is_idle(struct touchpad *touchpad) {
    if (touchpad == NULL || !touchpad->grabbed) {
        return true;
    }

    bool active = true;
    if (touchpad_read_contact_state(touchpad, touchpad->fd, &active) < 0) {
        if (!touchpad->contact_query_error_logged) {
            logger(LOG_LEVEL_WARN, "%s [%u]: Failed to inspect active touchpad contacts; keeping device grabbed.\n",
                    __FUNCTION__, __LINE__);
            touchpad->contact_query_error_logged = true;
        }
        return false;
    }

    return !active;
}

void touchpad_set_passthrough(struct touchpad *touchpad) {
    if (touchpad != NULL) {
        touchpad->passthrough = true;
    }
}

int touchpad_get_fd(const struct touchpad *touchpad) {
    return libinput_get_fd(touchpad->context);
}

const char * touchpad_get_devnode(const struct touchpad *touchpad) {
    return touchpad->uinput != NULL ? libevdev_uinput_get_devnode(touchpad->uinput) : NULL;
}
