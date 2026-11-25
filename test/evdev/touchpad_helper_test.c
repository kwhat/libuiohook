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
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "dispatch_event.h"
#include "input_state.h"
#include "mouse_helper.h"
#include "test_assert.h"
#include "touchpad_helper.h"

// Use real libevdev device templates and state accessors. Only physical device
// creation/reads/grabs and uinput output are replaced; no input hardware is used.
struct source {
    struct touchpad *touchpad;
    struct libevdev *device;
    int pipes[2], fd;
    struct input_event events[64];
    unsigned int count, next;
    int error, sync_reads;
    bool sync;
};
static struct source sources[4];
static struct source *creating;
static bool legacy, missing_resolution, unsupported, consume, active_after_grab, activate_on_grab;
static int query_error, device_error, uinput_error, grab_error;
static int grabs, ungrabs, clones, destroyed;
static int moves, presses, releases, clicks, wheels, horizontal_wheels;
static int emitted_x, emitted_y, emitted_wheel, emitted_hwheel, emitted_buttons;
static int last_dx, last_dy, last_button;
static event_type last_type;
static int uinput_token;
static uint64_t now_ms;

static void bit_set(unsigned long *bits, unsigned int bit) {
    unsigned int width = sizeof(*bits) * 8;
    bits[bit / width] |= 1UL << (bit % width);
}

int ioctl(int fd, unsigned long request, ...) {
    va_list args;
    va_start(args, request);
    void *value = va_arg(args, void *);
    va_end(args);
    if (query_error) {
        errno = query_error;
        return -1;
    }
    struct libevdev *device = NULL;
    for (unsigned int i = 0; i < 4; i++) {
        if (sources[i].device != NULL && sources[i].fd == fd) {
            device = sources[i].device;
        }
    }
    if (_IOC_NR(request) == _IOC_NR(EVIOCGBIT(0, 0))) {
        bit_set(value, EV_KEY);
        bit_set(value, EV_ABS);
    } else if (_IOC_NR(request) == _IOC_NR(EVIOCGBIT(EV_ABS, 0))) {
        if (!legacy) {
            bit_set(value, ABS_MT_SLOT);
        }
    } else if (request == EVIOCGABS(ABS_MT_SLOT)) {
        ((struct input_absinfo *)value)->maximum = 2;
    } else if (_IOC_NR(request) == _IOC_NR(EVIOCGKEY(0))) {
        if (device != NULL) {
            for (unsigned int code = 0; code <= KEY_MAX; code++) {
                if (libevdev_get_event_value(device, EV_KEY, code)) {
                    bit_set(value, code);
                }
            }
        }
        if (active_after_grab) {
            bit_set(value, BTN_TOUCH);
        }
    } else if (_IOC_NR(request) == _IOC_NR(EVIOCGMTSLOTS(0))) {
        int32_t *slots = value;
        for (unsigned int i = 1; i <= 3; i++) {
            slots[i] = -1;
            if (device != NULL) {
                slots[i] = libevdev_get_slot_value(device, i - 1, ABS_MT_TRACKING_ID);
            }
        }
    } else {
        TEST_ASSERT(false);
    }
    return 0;
}

int libevdev_new_from_fd(int fd, struct libevdev **result) {
    TEST_ASSERT(creating != NULL);
    creating->fd = fd;
    if (device_error) {
        return device_error;
    }
    struct libevdev *device = libevdev_new();
    TEST_ASSERT(device != NULL && creating != NULL);
    const unsigned int keys[] = {BTN_TOUCH, BTN_TOOL_FINGER, BTN_TOOL_DOUBLETAP, BTN_TOOL_TRIPLETAP,
                                 BTN_LEFT,  BTN_RIGHT,       BTN_MIDDLE};
    for (unsigned int i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        TEST_ASSERT(libevdev_enable_event_code(device, EV_KEY, keys[i], NULL) == 0);
    }
    struct input_absinfo axis = {.maximum = 10000, .resolution = 100};
    if (missing_resolution) {
        axis.resolution = 0;
    }
    if (!unsupported) {
        TEST_ASSERT(libevdev_enable_event_code(device, EV_ABS, ABS_X, &axis) == 0);
        if (!legacy) {
            TEST_ASSERT(libevdev_enable_event_code(device, EV_ABS, ABS_MT_POSITION_X, &axis) == 0);
        }
        axis.maximum = 6000;
        TEST_ASSERT(libevdev_enable_event_code(device, EV_ABS, ABS_Y, &axis) == 0);
        if (!legacy) {
            TEST_ASSERT(libevdev_enable_event_code(device, EV_ABS, ABS_MT_POSITION_Y, &axis) == 0);
            axis.maximum = 2;
            TEST_ASSERT(libevdev_enable_event_code(device, EV_ABS, ABS_MT_SLOT, &axis) == 0);
            axis.maximum = 65535;
            axis.minimum = -1;
            axis.value = -1;
            TEST_ASSERT(libevdev_enable_event_code(device, EV_ABS, ABS_MT_TRACKING_ID, &axis) == 0);
            axis.maximum = MT_TOOL_PALM;
            axis.minimum = 0;
            axis.value = 0;
            TEST_ASSERT(libevdev_enable_event_code(device, EV_ABS, ABS_MT_TOOL_TYPE, &axis) == 0);
            for (unsigned int slot = 0; slot < 3; slot++) {
                TEST_ASSERT(libevdev_set_slot_value(device, slot, ABS_MT_TRACKING_ID, -1) == 0);
            }
        }
    }
    creating->device = device;
    creating->fd = fd;
    *result = device;
    return 0;
}

int libevdev_grab(struct libevdev *device, enum libevdev_grab_mode mode) {
    TEST_ASSERT(device != NULL);
    if (mode == LIBEVDEV_GRAB) {
        grabs++;
        active_after_grab = activate_on_grab;
        return grab_error;
    }
    ungrabs++;
    return 0;
}

int libevdev_next_event(struct libevdev *device, unsigned int flags, struct input_event *event) {
    struct source *source = NULL;
    for (unsigned int i = 0; i < 4; i++) {
        if (sources[i].device == device) {
            source = &sources[i];
        }
    }
    TEST_ASSERT(source != NULL);
    if (flags == LIBEVDEV_READ_FLAG_SYNC) {
        source->sync_reads++;
    }
    if (source->next == source->count) {
        if (flags == LIBEVDEV_READ_FLAG_SYNC) {
            source->sync = false;
            return -EAGAIN;
        }
        if (source->error) {
            return source->error;
        }
        return -EAGAIN;
    }
    *event = source->events[source->next++];
    if (event->type == EV_SYN && event->code == SYN_DROPPED) {
        source->sync = true;
    }
    if (event->type == EV_KEY || event->type == EV_ABS) {
        int status;
        if (event->type == EV_ABS && event->code >= ABS_MT_TOUCH_MAJOR) {
            status = libevdev_set_slot_value(device, libevdev_get_current_slot(device), event->code, event->value);
        } else {
            status = libevdev_set_event_value(device, event->type, event->code, event->value);
        }
        TEST_ASSERT(status == 0);
    }
    if (source->sync) {
        return LIBEVDEV_READ_STATUS_SYNC;
    }
    return LIBEVDEV_READ_STATUS_SUCCESS;
}

int libevdev_uinput_create_from_device(const struct libevdev *device, int fd, struct libevdev_uinput **uinput) {
    TEST_ASSERT(fd == LIBEVDEV_UINPUT_OPEN_MANAGED);
    TEST_ASSERT(libevdev_has_event_code(device, EV_REL, REL_X));
    TEST_ASSERT(libevdev_has_event_code(device, EV_REL, REL_HWHEEL));
    TEST_ASSERT(libevdev_has_event_code(device, EV_KEY, BTN_MIDDLE));
    if (uinput_error) {
        return uinput_error;
    }
    *uinput = (struct libevdev_uinput *)&uinput_token;
    clones++;
    return 0;
}

void libevdev_uinput_destroy(struct libevdev_uinput *device) {
    TEST_ASSERT(device == (struct libevdev_uinput *)&uinput_token);
    destroyed++;
}

const char *libevdev_uinput_get_devnode(struct libevdev_uinput *device) {
    TEST_ASSERT(device == (struct libevdev_uinput *)&uinput_token);
    return "/mock/virtual-mouse";
}

int libevdev_uinput_write_event(const struct libevdev_uinput *device, unsigned int type, unsigned int code, int value) {
    TEST_ASSERT(device == (struct libevdev_uinput *)&uinput_token);
    if (type == EV_KEY) {
        emitted_buttons++;
    }
    if (type == EV_REL) {
        if (code == REL_X) {
            emitted_x += value;
        }
        if (code == REL_Y) {
            emitted_y += value;
        }
        if (code == REL_WHEEL) {
            emitted_wheel += value;
        }
        if (code == REL_HWHEEL) {
            emitted_hwheel += value;
        }
    }
    return 0;
}

static void callback(uiohook_event *const event, void *data) {
    (void)data;
    last_type = event->type;
    if (event->type == EVENT_MOUSE_MOVED || event->type == EVENT_MOUSE_DRAGGED) {
        moves++;
        last_dx = event->data.mouse.dx;
        last_dy = event->data.mouse.dy;
    } else if (event->type == EVENT_MOUSE_PRESSED) {
        presses++;
        last_button = event->data.mouse.button;
    } else if (event->type == EVENT_MOUSE_RELEASED) {
        releases++;
    } else if (event->type == EVENT_MOUSE_CLICKED) {
        clicks++;
    } else if (event->type == EVENT_MOUSE_WHEEL) {
        if (event->data.wheel.direction == WHEEL_VERTICAL_DIRECTION) {
            wheels += event->data.wheel.rotation / 120;
        } else {
            horizontal_wheels += event->data.wheel.rotation / 120;
        }
    }
    if (consume) {
        event->mask |= MASK_CONSUMED;
    }
}

static bool button_callback(struct input_event *event) {
    uint16_t button = MOUSE_BUTTON1, mask = MASK_BUTTON1;
    if (event->code == BTN_RIGHT) {
        button = MOUSE_BUTTON2;
        mask = MASK_BUTTON2;
    }
    if (event->code == BTN_MIDDLE) {
        button = MOUSE_BUTTON3;
        mask = MASK_BUTTON3;
    }
    if (event->value) {
        set_modifier_mask(mask);
        return dispatch_mouse_press(event, button);
    }
    unset_modifier_mask(mask);
    return dispatch_mouse_release(event, button);
}

static struct source *create_source(void) {
    struct source *source = NULL;
    for (unsigned int i = 0; i < 4; i++) {
        if (sources[i].device == NULL && sources[i].touchpad == NULL) {
            source = &sources[i];
            break;
        }
    }
    TEST_ASSERT(source != NULL);
    memset(source, 0, sizeof(*source));
    source->fd = -1;
    TEST_ASSERT(pipe(source->pipes) == 0);
    char path[64];
    snprintf(path, sizeof(path), "/proc/self/fd/%d", source->pipes[0]);
    creating = source;
    TEST_ASSERT(touchpad_create(path, button_callback, &source->touchpad) == UIOHOOK_SUCCESS);
    creating = NULL;
    TEST_ASSERT(source->touchpad != NULL && touchpad_get_fd(source->touchpad) >= 0);
    return source;
}

static void destroy_source(struct source *source) {
    int fd = touchpad_get_fd(source->touchpad);
    touchpad_destroy(&source->touchpad);
    TEST_ASSERT(fcntl(fd, F_GETFD) == -1 && errno == EBADF);
    touchpad_destroy(&source->touchpad);
    source->device = NULL;
    source->fd = -1;
    close(source->pipes[0]);
    close(source->pipes[1]);
}

static void queue(struct source *source, unsigned int type, unsigned int code, int value) {
    TEST_ASSERT(source->count < 64);
    struct input_event event = {0};
    event.input_event_sec = 1700000000 + now_ms / 1000;
    event.input_event_usec = (now_ms % 1000) * 1000;
    event.type = type;
    event.code = code;
    event.value = value;
    source->events[source->count++] = event;
}

static void frame(struct source *source, unsigned int elapsed) {
    now_ms += elapsed;
    queue(source, EV_SYN, SYN_REPORT, 0);
    TEST_ASSERT(touchpad_dispatch(source->touchpad) == 0);
    source->count = source->next = 0;
}

static void contact(struct source *source, int slot, int id, int x, int y) {
    queue(source, EV_ABS, ABS_MT_SLOT, slot);
    queue(source, EV_ABS, ABS_MT_TRACKING_ID, id);
    if (id >= 0) {
        queue(source, EV_ABS, ABS_MT_POSITION_X, x);
        queue(source, EV_ABS, ABS_MT_POSITION_Y, y);
    }
}

static void test_translation(void) {
    struct source *source = create_source();
    TEST_ASSERT(strcmp(touchpad_get_devnode(source->touchpad), "/mock/virtual-mouse") == 0);
    contact(source, 0, 10, 1000, 1000);
    frame(source, 10);
    TEST_ASSERT(moves == 0 && !touchpad_is_idle(source->touchpad));
    contact(source, 0, 10, 1100, 1200);
    frame(source, 10);
    TEST_ASSERT(moves == 1 && last_dx == 39 && last_dy == 78);
    TEST_ASSERT(emitted_x == 39 && emitted_y == 78);
    contact(source, 0, 10, 1101, 1200);
    frame(source, 10);
    TEST_ASSERT(moves == 1);
    contact(source, 0, 10, 1102, 1200);
    frame(source, 10);
    TEST_ASSERT(moves == 2 && last_dx == 1 && emitted_x == 40);
    consume = true;
    contact(source, 0, 10, 1202, 1200);
    frame(source, 10);
    TEST_ASSERT(moves == 3 && emitted_x == 40);
    consume = false;
    // A newly added contact changes the centroid but must not cause a jump.
    contact(source, 1, 11, 2202, 2200);
    frame(source, 10);
    TEST_ASSERT(moves == 3);
    contact(source, 0, 10, 1702, 1700);
    contact(source, 1, 11, 2702, 2700);
    frame(source, 10);
    TEST_ASSERT(wheels == -1 && horizontal_wheels == 1);
    TEST_ASSERT(emitted_wheel == -1 && emitted_hwheel == 1);
    consume = true;
    contact(source, 0, 10, 2202, 2200);
    contact(source, 1, 11, 3202, 3200);
    frame(source, 10);
    TEST_ASSERT(wheels == -2 && horizontal_wheels == 2);
    TEST_ASSERT(emitted_wheel == -1 && emitted_hwheel == 1);
    consume = false;
    contact(source, 1, -1, 0, 0);
    frame(source, 10);
    TEST_ASSERT(moves == 3);
    contact(source, 0, 99, 9000, 5000);
    frame(source, 10);
    TEST_ASSERT(moves == 3); // Replaced tracking ID also rebases motion.
    contact(source, 0, -1, 0, 0);
    frame(source, 200);
    TEST_ASSERT(touchpad_is_idle(source->touchpad) && presses == 0);
    destroy_source(source);

    // Missing resolution uses the documented dimensions. Legacy BTN_TOUCH/ABS
    // devices retain physical-button dragging and tool-count scrolling.
    legacy = missing_resolution = true;
    source = create_source();
    queue(source, EV_KEY, BTN_TOUCH, 1);
    queue(source, EV_ABS, ABS_X, 1000);
    queue(source, EV_ABS, ABS_Y, 1000);
    frame(source, 10);
    queue(source, EV_KEY, BTN_LEFT, 1);
    frame(source, 10);
    queue(source, EV_ABS, ABS_X, 1100);
    frame(source, 10);
    TEST_ASSERT(last_type == EVENT_MOUSE_DRAGGED && last_dx == 39);
    queue(source, EV_KEY, BTN_LEFT, 0);
    frame(source, 10);
    queue(source, EV_KEY, BTN_TOOL_DOUBLETAP, 1);
    frame(source, 10);
    queue(source, EV_ABS, ABS_X, 1600);
    queue(source, EV_ABS, ABS_Y, 1500);
    frame(source, 10);
    TEST_ASSERT(wheels == -3 && horizontal_wheels == 3);
    queue(source, EV_KEY, BTN_TOOL_DOUBLETAP, 0);
    queue(source, EV_KEY, BTN_TOUCH, 0);
    frame(source, 10);
    TEST_ASSERT(presses == 1 && releases == 1);
    destroy_source(source);

    // Independent carries for simultaneous devices.
    legacy = missing_resolution = false;
    source = create_source();
    struct source *other = create_source();
    int before = moves;
    contact(source, 0, 10, 1000, 1000);
    frame(source, 1);
    contact(other, 0, 10, 1000, 1000);
    frame(other, 1);
    contact(source, 0, 10, 1002, 1000);
    frame(source, 1);
    contact(other, 0, 10, 1002, 1000);
    frame(other, 1);
    TEST_ASSERT(moves == before);
    contact(source, 0, 10, 1003, 1000);
    frame(source, 1);
    TEST_ASSERT(moves == before + 1 && last_dx == 1);
    destroy_source(other);
    destroy_source(source);
}

static void test_gestures(void) {
    struct source *source = create_source();
    contact(source, 0, 1, 1000, 1000);
    frame(source, 10);
    contact(source, 0, -1, 0, 0);
    frame(source, 100);
    TEST_ASSERT(presses == 1 && releases == 1 && emitted_buttons == 2);
    contact(source, 0, 2, 1000, 1000);
    frame(source, 50);
    TEST_ASSERT(presses == 2); // Second touch holds the synthetic drag button.
    contact(source, 0, 2, 1200, 1000);
    frame(source, 10);
    TEST_ASSERT(last_type == EVENT_MOUSE_DRAGGED);
    contact(source, 0, -1, 0, 0);
    frame(source, 10);
    TEST_ASSERT(releases == 2 && emitted_buttons == 4);
    TEST_ASSERT(touchpad_is_idle(source->touchpad));
    contact(source, 0, 3, 1000, 1000);
    contact(source, 1, 4, 2000, 2000);
    frame(source, 300);
    contact(source, 0, -1, 0, 0);
    contact(source, 1, -1, 0, 0);
    frame(source, 50);
    TEST_ASSERT(presses == 3 && last_button == MOUSE_BUTTON2);
    contact(source, 0, 10, 1000, 1000);
    contact(source, 1, 11, 2000, 2000);
    contact(source, 2, 12, 3000, 3000);
    frame(source, 300);
    contact(source, 0, -1, 0, 0);
    contact(source, 1, -1, 0, 0);
    contact(source, 2, -1, 0, 0);
    frame(source, 50);
    TEST_ASSERT(presses == 4 && releases == 4 && last_button == MOUSE_BUTTON3);
    consume = true;
    contact(source, 0, 5, 1000, 1000);
    frame(source, 300);
    contact(source, 0, -1, 0, 0);
    frame(source, 50);
    TEST_ASSERT(presses == 5 && releases == 5 && emitted_buttons == 8);
    consume = false;
    contact(source, 0, 6, 1000, 1000);
    frame(source, 300);
    contact(source, 0, -1, 0, 0);
    frame(source, 200);
    TEST_ASSERT(presses == 5); // Long touches do not tap.
    int before_palms = moves;
    contact(source, 0, 7, 1000, 1000);
    queue(source, EV_ABS, ABS_MT_TOOL_TYPE, MT_TOOL_PALM);
    frame(source, 300);
    contact(source, 0, 7, 2000, 2000);
    frame(source, 10);
    contact(source, 0, -1, 0, 0);
    frame(source, 10);
    TEST_ASSERT(presses == 5 && moves == before_palms); // Hardware-labelled palms neither tap nor move.
    destroy_source(source);
}

static void test_passthrough(void) {
    struct source *source = create_source();
    contact(source, 0, 1, 1000, 1000);
    frame(source, 10);
    contact(source, 0, -1, 0, 0);
    frame(source, 50);
    contact(source, 0, 2, 1000, 1000);
    frame(source, 50);
    TEST_ASSERT(presses == 2 && releases == 1);
    touchpad_set_passthrough(source->touchpad);
    consume = true;
    contact(source, 0, 2, 1100, 1000);
    frame(source, 10);
    TEST_ASSERT(moves == 0 && emitted_x == 39);
    contact(source, 0, -1, 0, 0);
    frame(source, 10);
    TEST_ASSERT(presses == 2 && releases == 1 && emitted_buttons == 4);
    TEST_ASSERT(touchpad_is_idle(source->touchpad));
    // No new taps/callbacks may be synthesized during the stop wait.
    contact(source, 0, 3, 1000, 1000);
    frame(source, 10);
    contact(source, 0, -1, 0, 0);
    frame(source, 50);
    TEST_ASSERT(emitted_buttons == 4 && presses == 2);
    contact(source, 0, 4, 1000, 1000);
    contact(source, 1, 5, 2000, 2000);
    frame(source, 10);
    contact(source, 0, 4, 1500, 1500);
    contact(source, 1, 5, 2500, 2500);
    frame(source, 10);
    TEST_ASSERT(wheels == 0 && horizontal_wheels == 0);
    TEST_ASSERT(emitted_wheel == -1 && emitted_hwheel == 1);
    destroy_source(source);
}

static void test_sync(void) {
    struct source *source = create_source();
    contact(source, 0, 1, 1000, 1000);
    frame(source, 10);
    queue(source, EV_SYN, SYN_DROPPED, 0);
    contact(source, 0, 1, 9000, 5000);
    frame(source, 10);
    TEST_ASSERT(moves == 0 && source->sync_reads > 0);
    contact(source, 0, 1, 9100, 5000);
    frame(source, 10);
    TEST_ASSERT(moves == 1 && last_dx == 39);
    contact(source, 0, -1, 0, 0);
    frame(source, 10);
    TEST_ASSERT(presses == 0);
    source->error = -ENODEV;
    TEST_ASSERT(touchpad_dispatch(source->touchpad) == -ENODEV);
    destroy_source(source);
}

static void test_idle(void) {
    struct source *source = create_source();
    contact(source, 0, 1, 1000, 1000);
    frame(source, 10);
    queue(source, EV_KEY, BTN_LEFT, 1);
    frame(source, 10);
    TEST_ASSERT(!touchpad_is_idle(source->touchpad));
    query_error = EIO;
    TEST_ASSERT(!touchpad_is_idle(source->touchpad));
    query_error = 0;
    touchpad_set_passthrough(source->touchpad);
    contact(source, 0, -1, 0, 0);
    frame(source, 10);
    TEST_ASSERT(!touchpad_is_idle(source->touchpad)); // Still holding a physical button.
    queue(source, EV_KEY, BTN_LEFT, 0);
    frame(source, 10);
    TEST_ASSERT(touchpad_is_idle(source->touchpad));
    TEST_ASSERT(presses == 1 && releases == 0 && emitted_buttons == 2);
    destroy_source(source);
}

static void failed_source(void) {
    struct source *source = &sources[0];
    memset(source, 0, sizeof(*source));
    TEST_ASSERT(pipe(source->pipes) == 0);
    char path[64];
    snprintf(path, sizeof(path), "/proc/self/fd/%d", source->pipes[0]);
    creating = source;
    TEST_ASSERT(touchpad_create(path, button_callback, &source->touchpad) == UIOHOOK_FAILURE);
    TEST_ASSERT(source->touchpad == NULL);
    TEST_ASSERT(fcntl(source->fd, F_GETFD) == -1 && errno == EBADF);
    source->device = NULL;
    source->fd = -1;
    creating = NULL;
    close(source->pipes[0]);
    close(source->pipes[1]);
}

static void test_initialization(void) {
    struct touchpad *touchpad = NULL;
    TEST_ASSERT(touchpad_create("/does-not-exist", button_callback, &touchpad) == UIOHOOK_FAILURE && touchpad == NULL);
    struct source *source = create_source();
    TEST_ASSERT(grabs == 1 && clones == 1);
    destroy_source(source);
    TEST_ASSERT(ungrabs == 1 && destroyed == 1);
    uinput_error = -EACCES;
    source = create_source();
    TEST_ASSERT(touchpad_get_devnode(source->touchpad) == NULL && touchpad_is_idle(source->touchpad));
    contact(source, 0, 1, 1000, 1000);
    frame(source, 10);
    contact(source, 0, 1, 1100, 1000);
    frame(source, 10);
    TEST_ASSERT(moves == 1 && emitted_x == 0);
    destroy_source(source);
    uinput_error = 0;
    grab_error = -EBUSY;
    source = create_source();
    TEST_ASSERT(touchpad_get_devnode(source->touchpad) == NULL);
    destroy_source(source);
    grab_error = 0;
    query_error = EIO;
    source = create_source();
    TEST_ASSERT(touchpad_get_devnode(source->touchpad) == NULL);
    destroy_source(source);
    query_error = 0;
    activate_on_grab = true;
    source = create_source();
    TEST_ASSERT(touchpad_get_devnode(source->touchpad) == NULL);
    active_after_grab = activate_on_grab = false;
    destroy_source(source);
    device_error = -EIO;
    failed_source();
    device_error = 0;
    unsupported = true;
    failed_source();
    unsupported = false;
    TEST_ASSERT(clones == destroyed);
}

int main(int argc, char **argv) {
    for (unsigned int i = 0; i < 4; i++) {
        sources[i].fd = -1;
    }
    hook_set_dispatch_proc(callback, NULL);
    set_pointer_position(0, 0);
    unset_modifier_mask(UINT16_MAX);
    if (argc == 2 && strcmp(argv[1], "initialization") == 0) {
        test_initialization();
    } else if (argc == 2 && strcmp(argv[1], "gestures") == 0) {
        test_gestures();
    } else if (argc == 2 && strcmp(argv[1], "idle") == 0) {
        test_idle();
    } else if (argc == 2 && strcmp(argv[1], "passthrough") == 0) {
        test_passthrough();
    } else if (argc == 2 && strcmp(argv[1], "sync") == 0) {
        test_sync();
    } else {
        TEST_ASSERT(argc == 1);
        test_translation();
    }
    puts("Evdev touchpad tests passed");
    return EXIT_SUCCESS;
}
