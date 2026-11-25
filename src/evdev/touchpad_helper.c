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
#include <stdint.h>
#include <stdlib.h>
#include <uiohook.h>
#include <unistd.h>

#include "dispatch_event.h"
#include "input_activity.h"
#include "logger.h"
#include "mouse_helper.h"
#include "touchpad_helper.h"

// Deliberately small, fixed gesture policy. Coordinates are converted to mm
// using device resolution; devices without it assume a 100 by 60 mm surface.
static const double pointer_counts_per_mm = 1000.0 / 25.4;
static const double scroll_mm_per_step = 5.0;
static const double tap_travel_mm = 2.0;
static const uint64_t tap_time_ms = 180;
static const uint64_t drag_time_ms = 250;

struct touchpad {
    struct libevdev *evdev;
    struct libevdev_uinput *uinput;
    touchpad_button_proc dispatch_button;
    struct input_activity activity;
    int fd;
    bool grabbed, passthrough, activity_query_error_logged;
    bool multitouch;
    double resolution_x, resolution_y;
    int fingers, max_fingers;
    uint64_t contacts;
    double x, y, travel;
    double motion_x, motion_y, scroll_x, scroll_y;
    uint64_t touch_start, last_tap;
    bool tap_possible, recent_tap, tap_drag, button_in_frame;
};

struct touch_sample {
    int fingers;
    uint64_t contacts;
    double x, y;
    bool palm;
};

static double axis_resolution(struct libevdev *device, unsigned int code, double size_mm) {
    int resolution = libevdev_get_abs_resolution(device, code);
    if (resolution > 0) {
        return resolution;
    }
    double range = (double)libevdev_get_abs_maximum(device, code) - libevdev_get_abs_minimum(device, code);
    if (range > 0) {
        return range / size_mm;
    }
    return 1;
}

static struct touch_sample touchpad_sample(struct touchpad *touchpad) {
    struct touch_sample sample = {0};
    if (touchpad->multitouch) {
        int slots = libevdev_get_num_slots(touchpad->evdev);
        sample.contacts = UINT64_C(14695981039346656037);
        for (int slot = 0; slot < slots; slot++) {
            int id = libevdev_get_slot_value(touchpad->evdev, slot, ABS_MT_TRACKING_ID);
            if (id < 0) {
                continue;
            }
            if (libevdev_has_event_code(touchpad->evdev, EV_ABS, ABS_MT_TOOL_TYPE) &&
                libevdev_get_slot_value(touchpad->evdev, slot, ABS_MT_TOOL_TYPE) == MT_TOOL_PALM) {
                sample.palm = true;
                continue;
            }
            if (libevdev_has_event_code(touchpad->evdev, EV_ABS, ABS_MT_DISTANCE) &&
                libevdev_get_slot_value(touchpad->evdev, slot, ABS_MT_DISTANCE) > 0) {
                continue;
            }
            sample.fingers++;
            sample.x += libevdev_get_slot_value(touchpad->evdev, slot, ABS_MT_POSITION_X);
            sample.y += libevdev_get_slot_value(touchpad->evdev, slot, ABS_MT_POSITION_Y);
            sample.contacts ^= (uint32_t)id;
            sample.contacts *= UINT64_C(1099511628211);
        }
        if (sample.fingers > 0) {
            sample.x /= sample.fingers;
            sample.y /= sample.fingers;
        }
    } else if (libevdev_get_event_value(touchpad->evdev, EV_KEY, BTN_TOUCH)) {
        sample.fingers = 1;
        sample.x = libevdev_get_event_value(touchpad->evdev, EV_ABS, ABS_X);
        sample.y = libevdev_get_event_value(touchpad->evdev, EV_ABS, ABS_Y);
    }

    // Semi-multitouch devices can report more fingers than tracked positions.
    if (sample.fingers > 0) {
        const unsigned int tools[] = {BTN_TOOL_DOUBLETAP, BTN_TOOL_TRIPLETAP, BTN_TOOL_QUADTAP, BTN_TOOL_QUINTTAP};
        for (unsigned int i = 0; i < sizeof(tools) / sizeof(tools[0]); i++) {
            if (libevdev_get_event_value(touchpad->evdev, EV_KEY, tools[i]) && sample.fingers < (int)i + 2) {
                sample.fingers = (int)i + 2;
            }
        }
    }
    sample.x /= touchpad->resolution_x;
    sample.y /= touchpad->resolution_y;
    return sample;
}

static void touchpad_emit(struct touchpad *touchpad, unsigned int type, unsigned int code, int value) {
    if (touchpad->uinput != NULL) {
        libevdev_uinput_write_event(touchpad->uinput, type, code, value);
    }
}

static void touchpad_button(struct touchpad *touchpad, const struct input_event *timestamp, unsigned int code,
                            int value) {
    struct input_event event = *timestamp;
    event.type = EV_KEY;
    event.code = code;
    event.value = value;
    if (touchpad->passthrough || !touchpad->dispatch_button(&event)) {
        touchpad_emit(touchpad, EV_KEY, code, value);
        touchpad_emit(touchpad, EV_SYN, SYN_REPORT, 0);
    }
}

static int touchpad_delta(double value, double *carry) {
    *carry += value;
    if (*carry > INT16_MAX) {
        *carry = 0;
        return INT16_MAX;
    }
    if (*carry < INT16_MIN) {
        *carry = 0;
        return INT16_MIN;
    }
    int delta = (int)*carry;
    *carry -= delta;
    return delta;
}

static uint64_t event_time(const struct input_event *event) {
    return (uint64_t)event->input_event_sec * 1000 + event->input_event_usec / 1000;
}

static bool physical_button_down(struct touchpad *touchpad) {
    for (unsigned int code = BTN_LEFT; code <= BTN_MIDDLE; code++) {
        if (libevdev_get_event_value(touchpad->evdev, EV_KEY, code)) {
            return true;
        }
    }
    return false;
}

static void touchpad_scroll(struct touchpad *touchpad, struct input_event *event, double delta, double *carry,
                            unsigned int code, uint8_t direction) {
    int steps = touchpad_delta(delta / scroll_mm_per_step, carry);
    if (steps != 0 && (touchpad->passthrough || !dispatch_mouse_wheel(event, steps, direction))) {
        touchpad_emit(touchpad, EV_REL, code, steps);
    }
}

static void touchpad_frame(struct touchpad *touchpad, struct input_event *event) {
    struct touch_sample sample = touchpad_sample(touchpad);
    uint64_t time = event_time(event);
    if (sample.palm || touchpad->button_in_frame || physical_button_down(touchpad)) {
        touchpad->tap_possible = false;
    }
    if (sample.palm) {
        // Do not turn a hardware-labelled palm into motion, scrolling or a tap.
        sample.fingers = 0;
    }
    if (sample.fingers > 0 && touchpad->fingers == 0) {
        touchpad->touch_start = time;
        touchpad->max_fingers = sample.fingers;
        touchpad->travel = 0;
        touchpad->tap_possible =
                !touchpad->passthrough && !sample.palm && !touchpad->button_in_frame && !physical_button_down(touchpad);
        if (sample.fingers == 1 && touchpad->recent_tap && time >= touchpad->last_tap &&
            time - touchpad->last_tap <= drag_time_ms && touchpad->tap_possible) {
            touchpad->tap_drag = true;
            touchpad->tap_possible = false;
            touchpad_button(touchpad, event, BTN_LEFT, 1);
        }
        touchpad->recent_tap = false;
    } else if (sample.fingers == 0 && touchpad->fingers > 0) {
        if (touchpad->tap_drag) {
            touchpad_button(touchpad, event, BTN_LEFT, 0);
            touchpad->tap_drag = false;
        } else if (touchpad->tap_possible && time >= touchpad->touch_start &&
                   time - touchpad->touch_start <= tap_time_ms && touchpad->max_fingers <= 3) {
            unsigned int button = BTN_LEFT;
            if (touchpad->max_fingers == 2) {
                button = BTN_RIGHT;
            } else if (touchpad->max_fingers == 3) {
                button = BTN_MIDDLE;
            }
            touchpad_button(touchpad, event, button, 1);
            touchpad_button(touchpad, event, button, 0);
            touchpad->last_tap = time;
            touchpad->recent_tap = touchpad->max_fingers == 1;
        }
        touchpad->tap_possible = false;
    } else if (sample.fingers > 0 && sample.fingers == touchpad->fingers && sample.contacts == touchpad->contacts) {
        double dx = sample.x - touchpad->x;
        double dy = sample.y - touchpad->y;
        double travel_x = dx;
        double travel_y = dy;
        if (travel_x < 0) {
            travel_x = -travel_x;
        }
        if (travel_y < 0) {
            travel_y = -travel_y;
        }
        touchpad->travel += travel_x + travel_y;
        if (touchpad->travel > tap_travel_mm) {
            touchpad->tap_possible = false;
        }
        if (sample.fingers == 1) {
            int motion_x = touchpad_delta(dx * pointer_counts_per_mm, &touchpad->motion_x);
            int motion_y = touchpad_delta(dy * pointer_counts_per_mm, &touchpad->motion_y);
            if (motion_x != 0 || motion_y != 0) {
                track_pointer_delta(motion_x, motion_y);
                if (touchpad->passthrough || !dispatch_mouse_move(event, motion_x, motion_y)) {
                    touchpad_emit(touchpad, EV_REL, REL_X, motion_x);
                    touchpad_emit(touchpad, EV_REL, REL_Y, motion_y);
                }
            }
        } else if (sample.fingers == 2 && !touchpad->tap_drag) {
            touchpad_scroll(touchpad, event, -dy, &touchpad->scroll_y, REL_WHEEL, WHEEL_VERTICAL_DIRECTION);
            touchpad_scroll(touchpad, event, dx, &touchpad->scroll_x, REL_HWHEEL, WHEEL_HORIZONTAL_DIRECTION);
        }
    } else if (sample.fingers == touchpad->fingers && sample.contacts != touchpad->contacts) {
        // Replacing a contact must not turn the new position into a pointer jump.
        touchpad->tap_possible = false;
    }
    if (sample.fingers > touchpad->max_fingers) {
        touchpad->max_fingers = sample.fingers;
    }
    if (sample.fingers != touchpad->fingers || sample.contacts != touchpad->contacts) {
        touchpad->motion_x = touchpad->motion_y = 0;
        touchpad->scroll_x = touchpad->scroll_y = 0;
    }
    touchpad->button_in_frame = false;
    touchpad->fingers = sample.fingers;
    touchpad->contacts = sample.contacts;
    touchpad->x = sample.x;
    touchpad->y = sample.y;
    touchpad_emit(touchpad, EV_SYN, SYN_REPORT, 0);
}

int touchpad_create(const char *path, touchpad_button_proc dispatch_button, struct touchpad **result) {
    *result = NULL;
    struct touchpad *touchpad = calloc(1, sizeof(*touchpad));
    if (touchpad == NULL) {
        return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }
    touchpad->fd = open(path, O_RDONLY | O_CLOEXEC | O_NONBLOCK);
    touchpad->dispatch_button = dispatch_button;
    if (touchpad->fd < 0) {
        touchpad_destroy(&touchpad);
        return UIOHOOK_FAILURE;
    }
    int status = input_activity_init(&touchpad->activity, touchpad->fd);
    if (status == 0) {
        status = input_activity_wait(&touchpad->activity);
    }
    if (libevdev_new_from_fd(touchpad->fd, &touchpad->evdev) < 0) {
        touchpad_destroy(&touchpad);
        return UIOHOOK_FAILURE;
    }
    touchpad->multitouch = libevdev_get_num_slots(touchpad->evdev) > 0 &&
                           libevdev_has_event_code(touchpad->evdev, EV_ABS, ABS_MT_TRACKING_ID) &&
                           libevdev_has_event_code(touchpad->evdev, EV_ABS, ABS_MT_POSITION_X) &&
                           libevdev_has_event_code(touchpad->evdev, EV_ABS, ABS_MT_POSITION_Y);
    unsigned int axis_x = ABS_X, axis_y = ABS_Y;
    if (touchpad->multitouch) {
        axis_x = ABS_MT_POSITION_X;
        axis_y = ABS_MT_POSITION_Y;
    } else if (!libevdev_has_event_code(touchpad->evdev, EV_ABS, ABS_X) ||
               !libevdev_has_event_code(touchpad->evdev, EV_ABS, ABS_Y) ||
               !libevdev_has_event_code(touchpad->evdev, EV_KEY, BTN_TOUCH)) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Unsupported touchpad contact format: %s.\n", __FUNCTION__, __LINE__, path);
        touchpad_destroy(&touchpad);
        return UIOHOOK_FAILURE;
    }
    touchpad->resolution_x = axis_resolution(touchpad->evdev, axis_x, 100.0);
    touchpad->resolution_y = axis_resolution(touchpad->evdev, axis_y, 60.0);
    struct touch_sample sample = touchpad_sample(touchpad);
    touchpad->button_in_frame = false;
    touchpad->fingers = sample.fingers;
    touchpad->contacts = sample.contacts;
    touchpad->x = sample.x;
    touchpad->y = sample.y;

    struct libevdev *mouse = libevdev_new();
    if (mouse == NULL) {
        touchpad_destroy(&touchpad);
        return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }
    libevdev_set_name(mouse, "libuiohook touchpad pointer");
    libevdev_set_id_bustype(mouse, BUS_VIRTUAL);
    const int codes[] = {REL_X, REL_Y, REL_WHEEL, REL_HWHEEL};
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
    if (error == 0 && status == 0) {
        error = libevdev_uinput_create_from_device(mouse, LIBEVDEV_UINPUT_OPEN_MANAGED, &touchpad->uinput);
    }
    libevdev_free(mouse);
    if (error == 0 && touchpad->uinput != NULL) {
        error = libevdev_grab(touchpad->evdev, LIBEVDEV_GRAB);
        if (error == 0) {
            touchpad->grabbed = true;
            if (input_activity_is_idle(&touchpad->activity) != 1) {
                error = -EBUSY;
            }
        }
        if (error != 0) {
            if (touchpad->grabbed) {
                libevdev_grab(touchpad->evdev, LIBEVDEV_UNGRAB);
            }
            touchpad->grabbed = false;
            libevdev_uinput_destroy(touchpad->uinput);
            touchpad->uinput = NULL;
        }
    }
    if (touchpad->uinput == NULL) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Touchpad %s observing only; forwarding and consumption unavailable.\n",
               __FUNCTION__, __LINE__, path);
    }
    *result = touchpad;
    return UIOHOOK_SUCCESS;
}

int touchpad_dispatch(struct touchpad *touchpad) {
    struct input_event event;
    unsigned int flags = LIBEVDEV_READ_FLAG_NORMAL;
    bool syncing = false;
    for (;;) {
        int status = libevdev_next_event(touchpad->evdev, flags, &event);
        if (status == LIBEVDEV_READ_STATUS_SYNC) {
            syncing = true;
            flags = LIBEVDEV_READ_FLAG_SYNC;
            touchpad->tap_possible = touchpad->recent_tap = false;
        }
        if (status == -EAGAIN) {
            if (syncing) {
                // Rebase after dropped events, never infer a tap from missing data.
                struct touch_sample sample = touchpad_sample(touchpad);
                touchpad->fingers = sample.fingers;
                touchpad->contacts = sample.contacts;
                touchpad->x = sample.x;
                touchpad->y = sample.y;
                touchpad->motion_x = touchpad->motion_y = 0;
                touchpad->scroll_x = touchpad->scroll_y = 0;
                if (touchpad->tap_drag && sample.fingers == 0) {
                    touchpad_button(touchpad, &event, BTN_LEFT, 0);
                    touchpad->tap_drag = false;
                }
                syncing = false;
                flags = LIBEVDEV_READ_FLAG_NORMAL;
                continue;
            }
            return 0;
        }
        if (status < 0) {
            return status;
        }
        if (event.type == EV_KEY && event.code >= BTN_LEFT && event.code <= BTN_MIDDLE && event.value <= 1) {
            touchpad->button_in_frame = true;
            touchpad->tap_possible = touchpad->recent_tap = false;
            if (touchpad->tap_drag) {
                touchpad_button(touchpad, &event, BTN_LEFT, 0);
                touchpad->tap_drag = false;
            }
            touchpad_button(touchpad, &event, event.code, event.value);
        } else if (!syncing && event.type == EV_SYN && event.code == SYN_REPORT) {
            touchpad_frame(touchpad, &event);
        }
    }
}

void touchpad_destroy(struct touchpad **touchpad) {
    if (*touchpad != NULL) {
        if ((*touchpad)->grabbed) {
            libevdev_grab((*touchpad)->evdev, LIBEVDEV_UNGRAB);
        }
        if ((*touchpad)->uinput != NULL) {
            libevdev_uinput_destroy((*touchpad)->uinput);
        }
        if ((*touchpad)->evdev != NULL) {
            libevdev_free((*touchpad)->evdev);
        }
        if ((*touchpad)->fd >= 0) {
            close((*touchpad)->fd);
        }
        free(*touchpad);
        *touchpad = NULL;
    }
}

bool touchpad_is_idle(struct touchpad *touchpad) {
    if (touchpad == NULL || !touchpad->grabbed) {
        return true;
    }
    int idle = input_activity_is_idle(&touchpad->activity);
    if (idle < 0 && !touchpad->activity_query_error_logged) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Failed to inspect input state; keeping device grabbed.\n", __FUNCTION__,
               __LINE__);
        touchpad->activity_query_error_logged = true;
    }
    return idle == 1 && !touchpad->tap_drag;
}

void touchpad_set_passthrough(struct touchpad *touchpad) {
    if (touchpad != NULL) {
        touchpad->passthrough = true;
        touchpad->tap_possible = touchpad->recent_tap = false;
    }
}

int touchpad_get_fd(const struct touchpad *touchpad) {
    return touchpad->fd;
}

const char *touchpad_get_devnode(const struct touchpad *touchpad) {
    if (touchpad->uinput != NULL) {
        return libevdev_uinput_get_devnode(touchpad->uinput);
    }
    return NULL;
}
