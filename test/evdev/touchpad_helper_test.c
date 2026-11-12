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
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <uiohook.h>

#include "dispatch_event.h"
#include "input_helper.h"
#include "input_state.h"
#include "mouse_helper.h"
#include "test_assert.h"
#include "touchpad_helper.h"

// Native dependencies are supplied at link time. The real helper is compiled
// separately; tests only use its declared interface and never inspect its state.
static enum libinput_event_type test_event_type;
static double dx, dy, axis;
static bool pending, consume;
static int moves, wheels, presses, releases;
static int last_dx, last_dy;
static int emitted_x, emitted_y, emitted_wheel, emitted_buttons, destroyed;
static enum libinput_button_state button_state;
static int uinput_destroyed;
static enum libinput_pointer_axis active_axis = LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL;
static int horizontal_wheels, emitted_hwheel;
static int uinput_status;
static bool context_failure, device_failure;
static void *context_data;
static const struct libinput_interface *context_interface;
static int dispatch_status;
static int context_fd = -1;
static int context_token, device_token, uinput_token;
static int context_destroyed, devices_added, tap_requests, scroll_requests;

int gettimeofday(struct timeval *timestamp, void *timezone) {
    (void) timezone;
    timestamp->tv_sec = 1700000000;
    timestamp->tv_usec = 123456;
    return 0;
}

int libevdev_uinput_create_from_device(const struct libevdev *device, int fd, struct libevdev_uinput **uinput) {
    TEST_ASSERT(fd == LIBEVDEV_UINPUT_OPEN_MANAGED);
    TEST_ASSERT(libevdev_has_event_code(device, EV_REL, REL_X));
    TEST_ASSERT(libevdev_has_event_code(device, EV_REL, REL_Y));
    TEST_ASSERT(libevdev_has_event_code(device, EV_REL, REL_WHEEL));
    TEST_ASSERT(libevdev_has_event_code(device, EV_REL, REL_HWHEEL));
    TEST_ASSERT(libevdev_has_event_code(device, EV_KEY, BTN_LEFT));
    TEST_ASSERT(libevdev_has_event_code(device, EV_KEY, BTN_RIGHT));
    TEST_ASSERT(libevdev_has_event_code(device, EV_KEY, BTN_MIDDLE));
    if (uinput_status == 0) {
        *uinput = (struct libevdev_uinput *) &uinput_token;
    }
    return uinput_status;
}

struct libinput * libinput_path_create_context(const struct libinput_interface *interface, void *data) {
    TEST_ASSERT(interface->open_restricted != NULL);
    TEST_ASSERT(interface->close_restricted != NULL);
    context_interface = interface;
    context_data = data;
    TEST_ASSERT(context_data != NULL);
    return context_failure ? NULL : (struct libinput *) &context_token;
}

struct libinput_device * libinput_path_add_device(struct libinput *context, const char *path) {
    TEST_ASSERT(context == (struct libinput *) &context_token);
    TEST_ASSERT(strcmp(path, "/mock/touchpad") == 0);
    devices_added++;
    return device_failure ? NULL : (struct libinput_device *) &device_token;
}

enum libinput_config_status libinput_device_config_tap_set_enabled(struct libinput_device *device,
        enum libinput_config_tap_state enabled) {
    TEST_ASSERT(device == (struct libinput_device *) &device_token);
    TEST_ASSERT(enabled == LIBINPUT_CONFIG_TAP_ENABLED);
    tap_requests++;
    return LIBINPUT_CONFIG_STATUS_SUCCESS;
}

enum libinput_config_status libinput_device_config_scroll_set_method(struct libinput_device *device,
        enum libinput_config_scroll_method method) {
    TEST_ASSERT(device == (struct libinput_device *) &device_token);
    TEST_ASSERT(method == LIBINPUT_CONFIG_SCROLL_2FG);
    scroll_requests++;
    return LIBINPUT_CONFIG_STATUS_SUCCESS;
}

struct libinput * libinput_unref(struct libinput *context) {
    TEST_ASSERT(context == (struct libinput *) &context_token);
    // libinput closes its physical descriptor while unref releases the context.
    if (context_fd >= 0) {
        context_interface->close_restricted(context_fd, context_data);
        context_fd = -1;
    }
    context_destroyed++;
    return NULL;
}

void libevdev_uinput_destroy(struct libevdev_uinput *device) {
    TEST_ASSERT(device != NULL);
    uinput_destroyed++;
}

int libinput_dispatch(struct libinput *context) {
    (void) context;
    return dispatch_status;
}

struct libinput_event * libinput_get_event(struct libinput *context) {
    (void) context;
    if (!pending) {
        return NULL;
    }
    pending = false;
    return (struct libinput_event *) &pending;
}

enum libinput_event_type libinput_event_get_type(struct libinput_event *event) {
    (void) event;
    return test_event_type;
}

struct libinput_event_pointer * libinput_event_get_pointer_event(struct libinput_event *event) {
    return (void *) event;
}

double libinput_event_pointer_get_dx(struct libinput_event_pointer *event) {
    (void) event;
    return dx;
}

double libinput_event_pointer_get_dy(struct libinput_event_pointer *event) {
    (void) event;
    return dy;
}

uint32_t libinput_event_pointer_get_button(struct libinput_event_pointer *event) {
    (void) event;
    return BTN_LEFT;
}

enum libinput_button_state libinput_event_pointer_get_button_state(struct libinput_event_pointer *event) {
    (void) event;
    return button_state;
}

int libinput_event_pointer_has_axis(struct libinput_event_pointer *event, enum libinput_pointer_axis which) {
    (void) event;
    return which == active_axis;
}

double libinput_event_pointer_get_axis_value(struct libinput_event_pointer *event, enum libinput_pointer_axis which) {
    (void) event;
    (void) which;
    return axis;
}

void libinput_event_destroy(struct libinput_event *event) {
    (void) event;
    destroyed++;
}

int libevdev_uinput_write_event(const struct libevdev_uinput *device, unsigned int type, unsigned int code, int value) {
    (void) device;
    if (type == EV_REL && code == REL_X) {
        emitted_x += value;
    }
    if (type == EV_REL && code == REL_Y) {
        emitted_y += value;
    }
    if (type == EV_REL && code == REL_WHEEL) {
        emitted_wheel += value;
    }
    if (type == EV_REL && code == REL_HWHEEL) {
        emitted_hwheel += value;
    }
    if (type == EV_KEY) {
        emitted_buttons++;
    }
    return 0;
}

int libinput_get_fd(struct libinput *context) {
    TEST_ASSERT(context == (struct libinput *) &context_token);
    return 42;
}

const char * libevdev_uinput_get_devnode(struct libevdev_uinput *device) {
    TEST_ASSERT(device == (struct libevdev_uinput *) &uinput_token);
    return "/mock/virtual-mouse";
}

// The hook manager supplies this callback in production. Check that the helper
// delivers a native button event, then use real dispatch to exercise drag state.
static bool test_button_dispatch(struct input_event *event) {
    TEST_ASSERT(event->type == EV_KEY && event->code == BTN_LEFT);
    TEST_ASSERT(event->input_event_sec == 1700000000);
    TEST_ASSERT(event->input_event_usec == 123456);
    if (event->value > 0) {
        set_modifier_mask(MASK_BUTTON1);
        return dispatch_mouse_press(event, MOUSE_BUTTON1);
    }
    unset_modifier_mask(MASK_BUTTON1);
    return dispatch_mouse_release(event, MOUSE_BUTTON1);
}

// Exercise the real mouse dispatcher and modifier state, including drag detection.
static event_type last_mouse_type;

static void test_callback(uiohook_event *const event, void *data) {
    (void) data;
    last_mouse_type = event->type;
    switch (event->type) {
        case EVENT_MOUSE_MOVED:
        case EVENT_MOUSE_DRAGGED:
            TEST_ASSERT(event->data.mouse.button == MOUSE_NOBUTTON);
            last_dx = event->data.mouse.dx;
            last_dy = event->data.mouse.dy;
            TEST_ASSERT((event->type == EVENT_MOUSE_DRAGGED) == ((get_modifiers() & 0x1F00) != 0));
            moves++;
            break;

        case EVENT_MOUSE_PRESSED:
            TEST_ASSERT(event->data.mouse.button == MOUSE_BUTTON1);
            TEST_ASSERT(event->data.mouse.dx == 0 && event->data.mouse.dy == 0);
            presses++;
            break;

        case EVENT_MOUSE_RELEASED:
            TEST_ASSERT(event->data.mouse.button == MOUSE_BUTTON1);
            TEST_ASSERT(event->data.mouse.dx == 0 && event->data.mouse.dy == 0);
            releases++;
            break;

        case EVENT_MOUSE_WHEEL:
            TEST_ASSERT(event->data.wheel.delta == 120);
            if (active_axis == LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL) {
                TEST_ASSERT(event->data.wheel.direction == WHEEL_VERTICAL_DIRECTION);
                wheels += event->data.wheel.rotation / 120;
            } else {
                TEST_ASSERT(event->data.wheel.direction == WHEEL_HORIZONTAL_DIRECTION);
                horizontal_wheels += event->data.wheel.rotation / 120;
            }
            break;

        default:
            break;
    }

    if (consume) {
        event->mask |= MASK_CONSUMED;
    }
}

static void send_event(struct touchpad *touchpad, enum libinput_event_type type) {
    test_event_type = type;
    pending = true;
    TEST_ASSERT(touchpad_dispatch(touchpad) == 0);
}

static struct touchpad * create_test_touchpad(void) {
    struct touchpad *touchpad = NULL;
    TEST_ASSERT(touchpad_create("/mock/touchpad", test_button_dispatch, &touchpad) == UIOHOOK_SUCCESS);
    TEST_ASSERT(touchpad != NULL);
    return touchpad;
}

static void test_initialization(void) {
    struct touchpad *touchpad = create_test_touchpad();
    TEST_ASSERT(context_data == touchpad);
    TEST_ASSERT(touchpad_get_fd(touchpad) == 42);
    TEST_ASSERT(strcmp(touchpad_get_devnode(touchpad), "/mock/virtual-mouse") == 0);
    TEST_ASSERT(devices_added == 1 && tap_requests == 1 && scroll_requests == 1);
    touchpad_destroy(&touchpad);
    TEST_ASSERT(touchpad == NULL && uinput_destroyed == 1 && context_destroyed == 1);
    touchpad_destroy(&touchpad);
    TEST_ASSERT(uinput_destroyed == 1 && context_destroyed == 1);

    // Missing or inaccessible uinput still permits event observation. The open
    // callback gets the opaque helper state and must leave the device ungrabbed.
    const int failures[] = { -ENOENT, -EACCES };
    for (unsigned int i = 0; i < sizeof(failures) / sizeof(failures[0]); i++) {
        uinput_status = failures[i];
        touchpad = create_test_touchpad();
        TEST_ASSERT(context_data == touchpad && touchpad_get_devnode(touchpad) == NULL);
        int fd = context_interface->open_restricted("/dev/null", O_RDONLY, context_data);
        TEST_ASSERT(fd >= 0);
        context_interface->close_restricted(fd, context_data);
        touchpad_destroy(&touchpad);
    }
    TEST_ASSERT(uinput_destroyed == 1 && context_destroyed == 3);

    uinput_status = 0;
    context_failure = true;
    TEST_ASSERT(touchpad_create("/mock/touchpad", test_button_dispatch, &touchpad) == UIOHOOK_FAILURE);
    TEST_ASSERT(touchpad == NULL && uinput_destroyed == 2 && context_destroyed == 3);

    context_failure = false;
    device_failure = true;
    TEST_ASSERT(touchpad_create("/mock/touchpad", test_button_dispatch, &touchpad) == UIOHOOK_FAILURE);
    TEST_ASSERT(touchpad == NULL && uinput_destroyed == 3 && context_destroyed == 4);
    TEST_ASSERT(tap_requests == 3 && scroll_requests == 3);

    uinput_status = -ENOENT;
    TEST_ASSERT(touchpad_create("/mock/touchpad", test_button_dispatch, &touchpad) == UIOHOOK_FAILURE);
    TEST_ASSERT(touchpad == NULL && uinput_destroyed == 3 && context_destroyed == 5);

    // A forwarding device that cannot be grabbed must become observation-only.
    uinput_status = 0;
    device_failure = false;
    touchpad = create_test_touchpad();
    int fd = context_interface->open_restricted("/dev/null", O_RDONLY, context_data);
    TEST_ASSERT(fd >= 0 && touchpad_get_devnode(touchpad) == NULL && uinput_destroyed == 4);
    context_fd = fd;
    touchpad_destroy(&touchpad);
    TEST_ASSERT(fcntl(fd, F_GETFD) == -1 && errno == EBADF);
    TEST_ASSERT(touchpad == NULL && uinput_destroyed == 4 && context_destroyed == 6);
    puts("Touchpad initialization tests passed");
}

static void test_translation(void) {
    hook_set_dispatch_proc(test_callback, NULL);
    set_pointer_position(0, 0);
    unset_modifier_mask(UINT16_MAX);

    struct touchpad *touchpad = create_test_touchpad();
    dx = 0.75;
    dy = -0.75;
    send_event(touchpad, LIBINPUT_EVENT_POINTER_MOTION);
    TEST_ASSERT(moves == 0);
    send_event(touchpad, LIBINPUT_EVENT_POINTER_MOTION);
    TEST_ASSERT(moves == 1 && emitted_x == 1 && emitted_y == -1);
    TEST_ASSERT(last_dx == 1 && last_dy == -1);
    consume = true;
    dx = 2;
    dy = -2;
    send_event(touchpad, LIBINPUT_EVENT_POINTER_MOTION);
    int16_t x, y;
    get_pointer_position(&x, &y);
    TEST_ASSERT(moves == 2 && x == 3 && y == -3);
    TEST_ASSERT(last_dx == 2 && last_dy == -2);
    TEST_ASSERT(emitted_x == 1 && emitted_y == -1);
    consume = false;
    axis = 7.5;
    send_event(touchpad, LIBINPUT_EVENT_POINTER_AXIS);
    TEST_ASSERT(wheels == 0);
    send_event(touchpad, LIBINPUT_EVENT_POINTER_AXIS);
    TEST_ASSERT(wheels == -1 && emitted_wheel == -1);
    consume = true;
    axis = 15;
    send_event(touchpad, LIBINPUT_EVENT_POINTER_AXIS);
    TEST_ASSERT(wheels == -2 && emitted_wheel == -1);
    button_state = LIBINPUT_BUTTON_STATE_PRESSED;
    send_event(touchpad, LIBINPUT_EVENT_POINTER_BUTTON);
    TEST_ASSERT(presses == 1 && emitted_buttons == 0);
    consume = false;
    button_state = LIBINPUT_BUTTON_STATE_RELEASED;
    send_event(touchpad, LIBINPUT_EVENT_POINTER_BUTTON);
    TEST_ASSERT(releases == 1 && emitted_buttons == 1);
#ifdef HAVE_LIBINPUT_POINTER_SCROLL_FINGER
    send_event(touchpad, LIBINPUT_EVENT_POINTER_SCROLL_FINGER);
    TEST_ASSERT(wheels == -2);
#else
    send_event(touchpad, LIBINPUT_EVENT_DEVICE_ADDED);
    TEST_ASSERT(wheels == -2);
#endif
    pending = true;
    test_event_type = LIBINPUT_EVENT_DEVICE_REMOVED;
    TEST_ASSERT(touchpad_dispatch(touchpad) == -ENODEV);
    TEST_ASSERT(destroyed == 10);
    dispatch_status = -EIO;
    TEST_ASSERT(touchpad_dispatch(touchpad) == -EIO && destroyed == 10);
    dispatch_status = 0;

    axis = 7.5;
    send_event(touchpad, LIBINPUT_EVENT_POINTER_AXIS);
    axis = 0;
    send_event(touchpad, LIBINPUT_EVENT_POINTER_AXIS);
    axis = 7.5;
    send_event(touchpad, LIBINPUT_EVENT_POINTER_AXIS);
    TEST_ASSERT(wheels == -2); // Scroll stop clears the fractional step.
    touchpad_destroy(&touchpad);

    // Observation still dispatches events but must never write to uinput.
    uinput_status = -ENOENT;
    touchpad = create_test_touchpad();
    dx = 3;
    dy = 3;
    send_event(touchpad, LIBINPUT_EVENT_POINTER_MOTION);
    TEST_ASSERT(moves == 3 && emitted_x == 1 && emitted_y == -1);
    TEST_ASSERT(last_dx == 3 && last_dy == 3);
    button_state = LIBINPUT_BUTTON_STATE_PRESSED;
    send_event(touchpad, LIBINPUT_EVENT_POINTER_BUTTON);
    TEST_ASSERT(presses == 2 && emitted_buttons == 1);
    axis = 15;
    send_event(touchpad, LIBINPUT_EVENT_POINTER_AXIS);
    TEST_ASSERT(wheels == -3 && emitted_wheel == -1);

    TEST_ASSERT((get_modifiers() & MASK_BUTTON1) != 0);
    send_event(touchpad, LIBINPUT_EVENT_POINTER_MOTION);
    TEST_ASSERT(last_mouse_type == EVENT_MOUSE_DRAGGED);
    TEST_ASSERT(last_dx == 3 && last_dy == 3);
    button_state = LIBINPUT_BUTTON_STATE_RELEASED;
    send_event(touchpad, LIBINPUT_EVENT_POINTER_BUTTON);
    TEST_ASSERT(last_mouse_type == EVENT_MOUSE_RELEASED);
    TEST_ASSERT((get_modifiers() & MASK_BUTTON1) == 0);
    send_event(touchpad, LIBINPUT_EVENT_POINTER_MOTION);
    TEST_ASSERT(last_mouse_type == EVENT_MOUSE_MOVED);
    touchpad_destroy(&touchpad);

    uinput_status = 0;
    touchpad = create_test_touchpad();
    active_axis = LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL;
    axis = 7.5;
    send_event(touchpad, LIBINPUT_EVENT_POINTER_AXIS);
    TEST_ASSERT(horizontal_wheels == 0 && emitted_hwheel == 0);
    send_event(touchpad, LIBINPUT_EVENT_POINTER_AXIS);
    TEST_ASSERT(horizontal_wheels == 1 && emitted_hwheel == 1);
    consume = true;
    axis = 15;
    send_event(touchpad, LIBINPUT_EVENT_POINTER_AXIS);
    TEST_ASSERT(horizontal_wheels == 2 && emitted_hwheel == 1);
    consume = false;
    axis = -30;
    send_event(touchpad, LIBINPUT_EVENT_POINTER_AXIS);
    TEST_ASSERT(horizontal_wheels == 0 && emitted_hwheel == -1);
    axis = 7.5;
    send_event(touchpad, LIBINPUT_EVENT_POINTER_AXIS);
    axis = 0;
    send_event(touchpad, LIBINPUT_EVENT_POINTER_AXIS);
    axis = 7.5;
    send_event(touchpad, LIBINPUT_EVENT_POINTER_AXIS);
    TEST_ASSERT(horizontal_wheels == 0 && emitted_hwheel == -1);
    touchpad_destroy(&touchpad);
    // Fractional deltas belong to each helper, not to shared/global state.
    struct touchpad *other = create_test_touchpad();
    touchpad = create_test_touchpad();
    int previous_moves = moves;
    int previous_x = emitted_x;
    dx = 0.6;
    dy = 0;
    send_event(touchpad, LIBINPUT_EVENT_POINTER_MOTION);
    send_event(other, LIBINPUT_EVENT_POINTER_MOTION);
    TEST_ASSERT(moves == previous_moves && emitted_x == previous_x);
    send_event(touchpad, LIBINPUT_EVENT_POINTER_MOTION);
    send_event(other, LIBINPUT_EVENT_POINTER_MOTION);
    TEST_ASSERT(moves == previous_moves + 2 && emitted_x == previous_x + 2);
    touchpad_destroy(&touchpad);
    touchpad_destroy(&other);
    puts("Touchpad translation tests passed");
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "initialization") == 0) {
        test_initialization();
    } else {
        TEST_ASSERT(argc == 1);
        test_translation();
    }
    return EXIT_SUCCESS;
}
