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

/* Focused translation tests, without grabbing hardware or creating uinput nodes.
 * Include the backend to exercise its private event translation paths. */
#define libinput_path_create_context test_create_context
#define libinput_path_add_device test_add_device
#define libinput_device_config_tap_set_enabled test_enable_tap
#define libinput_device_config_scroll_set_method test_set_scroll_method
#define libinput_unref test_unref
#define libevdev_uinput_create_from_device test_create_uinput
#define libinput_dispatch test_dispatch
#define libinput_get_event test_get_event
#define libinput_event_get_type test_get_type
#define libinput_event_get_pointer_event test_get_pointer
#define libinput_event_pointer_get_dx test_dx
#define libinput_event_pointer_get_dy test_dy
#define libinput_event_pointer_get_button test_button
#define libinput_event_pointer_get_button_state test_button_state
#define libinput_event_pointer_has_axis test_has_axis
#define libinput_event_pointer_get_axis_value test_axis_value
#define libinput_event_destroy test_destroy
#define libevdev_uinput_write_event test_write
#define libevdev_uinput_destroy test_uinput_destroy
#define dispatch_mouse_wheel test_wheel
#define track_pointer_delta test_track
#include "test_assert.h"
#include <stdio.h>

#include "../src/evdev/input_hook.c"

static enum libinput_event_type test_event_type;
static double dx, dy, axis;
static bool pending, consume;
static int moves, wheels, presses, releases, tracked_x, tracked_y;
static int emitted_x, emitted_y, emitted_wheel, emitted_buttons, destroyed;
static enum libinput_button_state button_state;
static int uinput_destroyed;
static enum libinput_pointer_axis active_axis = LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL;
static int horizontal_wheels, emitted_hwheel;
static int uinput_status;
static bool context_failure, device_failure;
static struct input_hook *context_hook;
static int context_token, device_token, uinput_token;
static int context_destroyed, devices_added, tap_requests, scroll_requests;

int test_create_uinput(const struct libevdev *device, int fd, struct libevdev_uinput **uinput) {
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

struct libinput * test_create_context(const struct libinput_interface *interface, void *data) {
    TEST_ASSERT(interface->open_restricted == touchpad_open);
    TEST_ASSERT(interface->close_restricted == touchpad_close);
    context_hook = data;
    TEST_ASSERT(context_hook != NULL);
    return context_failure ? NULL : (struct libinput *) &context_token;
}

struct libinput_device * test_add_device(struct libinput *context, const char *path) {
    TEST_ASSERT(context == (struct libinput *) &context_token);
    TEST_ASSERT(strcmp(path, "/mock/touchpad") == 0);
    devices_added++;
    return device_failure ? NULL : (struct libinput_device *) &device_token;
}

enum libinput_config_status test_enable_tap(struct libinput_device *device,
        enum libinput_config_tap_state enabled) {
    TEST_ASSERT(device == (struct libinput_device *) &device_token);
    TEST_ASSERT(enabled == LIBINPUT_CONFIG_TAP_ENABLED);
    tap_requests++;
    return LIBINPUT_CONFIG_STATUS_SUCCESS;
}

enum libinput_config_status test_set_scroll_method(struct libinput_device *device,
        enum libinput_config_scroll_method method) {
    TEST_ASSERT(device == (struct libinput_device *) &device_token);
    TEST_ASSERT(method == LIBINPUT_CONFIG_SCROLL_2FG);
    scroll_requests++;
    return LIBINPUT_CONFIG_STATUS_SUCCESS;
}

struct libinput * test_unref(struct libinput *context) {
    TEST_ASSERT(context == (struct libinput *) &context_token);
    context_destroyed++;
    return NULL;
}

void test_uinput_destroy(struct libevdev_uinput *device) {
    TEST_ASSERT(device != NULL);
    uinput_destroyed++;
}

int test_dispatch(struct libinput *context) {
    (void) context;
    return 0;
}

struct libinput_event * test_get_event(struct libinput *context) {
    (void) context;
    if (!pending) {
        return NULL;
    }
    pending = false;
    return (struct libinput_event *) &pending;
}

enum libinput_event_type test_get_type(struct libinput_event *event) {
    (void) event;
    return test_event_type;
}

struct libinput_event_pointer * test_get_pointer(struct libinput_event *event) {
    return (void *) event;
}

double test_dx(struct libinput_event_pointer *event) {
    (void) event;
    return dx;
}

double test_dy(struct libinput_event_pointer *event) {
    (void) event;
    return dy;
}

uint32_t test_button(struct libinput_event_pointer *event) {
    (void) event;
    return BTN_LEFT;
}

enum libinput_button_state test_button_state(struct libinput_event_pointer *event) {
    (void) event;
    return button_state;
}

int test_has_axis(struct libinput_event_pointer *event, enum libinput_pointer_axis which) {
    (void) event;
    return which == active_axis;
}

double test_axis_value(struct libinput_event_pointer *event, enum libinput_pointer_axis which) {
    (void) event;
    (void) which;
    return axis;
}

void test_destroy(struct libinput_event *event) {
    (void) event;
    destroyed++;
}

int test_write(const struct libevdev_uinput *device, unsigned int type, unsigned int code, int value) {
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

bool test_wheel(struct input_event *const event, int16_t rotation, uint8_t direction) {
    (void) event;
    if (active_axis == LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL) {
        TEST_ASSERT(direction == WHEEL_VERTICAL_DIRECTION);
        wheels += rotation;
    } else {
        TEST_ASSERT(direction == WHEEL_HORIZONTAL_DIRECTION);
        horizontal_wheels += rotation;
    }
    return consume;
}

void test_track(int x, int y) {
    tracked_x += x;
    tracked_y += y;
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
            TEST_ASSERT((event->type == EVENT_MOUSE_DRAGGED) == ((get_modifiers() & 0x1F00) != 0));
            moves++;
            break;

        case EVENT_MOUSE_PRESSED:
            TEST_ASSERT(event->data.mouse.button == MOUSE_BUTTON1);
            presses++;
            break;

        case EVENT_MOUSE_RELEASED:
            TEST_ASSERT(event->data.mouse.button == MOUSE_BUTTON1);
            releases++;
            break;

        default:
            break;
    }

    if (consume) {
        event->mask |= MASK_CONSUMED;
    }
}

static void send_event(struct input_hook *hook, enum libinput_event_type type) {
    test_event_type = type;
    pending = true;
    TEST_ASSERT(touchpad_dispatch(hook) == 0);
}

static void test_initialization(void) {
    struct input_hook *hook = NULL;

    // Successful creation supplies the hook as callback data and cleans up both
    // the input context and its virtual mouse on teardown.
    TEST_ASSERT(create_hook("/mock/touchpad", 0, "touchpad", &hook) == UIOHOOK_SUCCESS);
    TEST_ASSERT(context_hook == hook);
    TEST_ASSERT(hook->uinput != NULL && hook->touchpad != NULL);
    TEST_ASSERT(strcmp(hook->devnode, "/mock/touchpad") == 0);
    TEST_ASSERT(devices_added == 1 && tap_requests == 1 && scroll_requests == 1);
    destroy_hook(&hook);
    TEST_ASSERT(hook == NULL && uinput_destroyed == 1 && context_destroyed == 1);

    // Missing uinput and permission failures must still create an observation
    // hook. Its open callback must receive that same hook and avoid grabbing.
    const int failures[] = { -ENOENT, -EACCES };
    for (unsigned int i = 0; i < sizeof(failures) / sizeof(failures[0]); i++) {
        uinput_status = failures[i];
        TEST_ASSERT(create_hook("/mock/touchpad", 0, "touchpad", &hook) == UIOHOOK_SUCCESS);
        TEST_ASSERT(context_hook == hook && hook->uinput == NULL && hook->touchpad != NULL);
        int fd = touchpad_interface.open_restricted("/dev/null", O_RDONLY, context_hook);
        TEST_ASSERT(fd >= 0);
        touchpad_interface.close_restricted(fd, context_hook);
        destroy_hook(&hook);
    }
    TEST_ASSERT(uinput_destroyed == 1 && context_destroyed == 3);

    uinput_status = 0;
    context_failure = true;
    TEST_ASSERT(create_hook("/mock/touchpad", 0, "touchpad", &hook) == UIOHOOK_FAILURE);
    destroy_hook(&hook);
    TEST_ASSERT(hook == NULL && uinput_destroyed == 2 && context_destroyed == 3);

    context_failure = false;
    device_failure = true;
    TEST_ASSERT(create_hook("/mock/touchpad", 0, "touchpad", &hook) == UIOHOOK_FAILURE);
    destroy_hook(&hook);
    TEST_ASSERT(hook == NULL && uinput_destroyed == 3 && context_destroyed == 4);
    TEST_ASSERT(tap_requests == 3 && scroll_requests == 3);

    // Failed input-device initialization must also clean up observation mode.
    uinput_status = -ENOENT;
    TEST_ASSERT(create_hook("/mock/touchpad", 0, "touchpad", &hook) == UIOHOOK_FAILURE);
    destroy_hook(&hook);
    TEST_ASSERT(hook == NULL && uinput_destroyed == 3 && context_destroyed == 5);
    puts("Touchpad initialization tests passed");
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "initialization") == 0) {
        test_initialization();
        return EXIT_SUCCESS;
    }
    TEST_ASSERT(argc == 1);

    hook_set_dispatch_proc(test_callback, NULL);

    struct input_hook hook = { 0 };
    hook.uinput = (struct libevdev_uinput *) &hook;
    dx = 0.75;
    dy = -0.75;
    send_event(&hook, LIBINPUT_EVENT_POINTER_MOTION);
    TEST_ASSERT(moves == 0);
    send_event(&hook, LIBINPUT_EVENT_POINTER_MOTION);
    TEST_ASSERT(moves == 1 && emitted_x == 1 && emitted_y == -1);
    consume = true;
    dx = 2;
    dy = -2;
    send_event(&hook, LIBINPUT_EVENT_POINTER_MOTION);
    TEST_ASSERT(moves == 2 && tracked_x == 3 && tracked_y == -3);
    TEST_ASSERT(emitted_x == 1 && emitted_y == -1);
    consume = false;
    axis = 7.5;
    send_event(&hook, LIBINPUT_EVENT_POINTER_AXIS);
    TEST_ASSERT(wheels == 0);
    send_event(&hook, LIBINPUT_EVENT_POINTER_AXIS);
    TEST_ASSERT(wheels == -1 && emitted_wheel == -1);
    consume = true;
    axis = 15;
    send_event(&hook, LIBINPUT_EVENT_POINTER_AXIS);
    TEST_ASSERT(wheels == -2 && emitted_wheel == -1);
    button_state = LIBINPUT_BUTTON_STATE_PRESSED;
    send_event(&hook, LIBINPUT_EVENT_POINTER_BUTTON);
    TEST_ASSERT(presses == 1 && emitted_buttons == 0);
    consume = false;
    button_state = LIBINPUT_BUTTON_STATE_RELEASED;
    send_event(&hook, LIBINPUT_EVENT_POINTER_BUTTON);
    TEST_ASSERT(releases == 1 && emitted_buttons == 1);
#ifdef HAVE_LIBINPUT_POINTER_SCROLL_FINGER
    send_event(&hook, LIBINPUT_EVENT_POINTER_SCROLL_FINGER);
    TEST_ASSERT(wheels == -2); // New scroll events must not duplicate legacy events.
#else
    send_event(&hook, LIBINPUT_EVENT_DEVICE_ADDED);
    TEST_ASSERT(wheels == -2); // Non-pointer events must not dispatch scrolling.
#endif
    pending = true;
    test_event_type = LIBINPUT_EVENT_DEVICE_REMOVED;
    TEST_ASSERT(touchpad_dispatch(&hook) == -ENODEV);
    TEST_ASSERT(destroyed == 10);
    consume = false;
    axis = 7.5;
    send_event(&hook, LIBINPUT_EVENT_POINTER_AXIS);
    axis = 0;
    send_event(&hook, LIBINPUT_EVENT_POINTER_AXIS);
    axis = 7.5;
    send_event(&hook, LIBINPUT_EVENT_POINTER_AXIS);
    TEST_ASSERT(wheels == -2); // End-of-scroll clears the partial wheel step.

    // Missing uinput must still allow callbacks, without injecting events.
    hook.uinput = NULL;
    dx = 3;
    dy = 3;
    send_event(&hook, LIBINPUT_EVENT_POINTER_MOTION);
    TEST_ASSERT(moves == 3 && emitted_x == 1 && emitted_y == -1);
    button_state = LIBINPUT_BUTTON_STATE_PRESSED;
    send_event(&hook, LIBINPUT_EVENT_POINTER_BUTTON);
    TEST_ASSERT(presses == 2 && emitted_buttons == 1);
    axis = 15;
    send_event(&hook, LIBINPUT_EVENT_POINTER_AXIS);
    TEST_ASSERT(wheels == -3 && emitted_wheel == -1);

    // Physical press -> motion -> release must produce a drag, not a click.
    TEST_ASSERT((get_modifiers() & MASK_BUTTON1) != 0);
    send_event(&hook, LIBINPUT_EVENT_POINTER_MOTION);
    TEST_ASSERT(last_mouse_type == EVENT_MOUSE_DRAGGED);
    button_state = LIBINPUT_BUTTON_STATE_RELEASED;
    send_event(&hook, LIBINPUT_EVENT_POINTER_BUTTON);
    TEST_ASSERT(last_mouse_type == EVENT_MOUSE_RELEASED);
    TEST_ASSERT((get_modifiers() & MASK_BUTTON1) == 0);
    send_event(&hook, LIBINPUT_EVENT_POINTER_MOTION);
    TEST_ASSERT(last_mouse_type == EVENT_MOUSE_MOVED);

    // Horizontal scroll uses the horizontal event direction and preserves its
    // sign in uinput. Fractional movement and consumption match the vertical path.
    hook.uinput = (struct libevdev_uinput *) &hook;
    active_axis = LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL;
    axis = 7.5;
    send_event(&hook, LIBINPUT_EVENT_POINTER_AXIS);
    TEST_ASSERT(horizontal_wheels == 0 && emitted_hwheel == 0);
    send_event(&hook, LIBINPUT_EVENT_POINTER_AXIS);
    TEST_ASSERT(horizontal_wheels == 1 && emitted_hwheel == 1);
    consume = true;
    axis = 15;
    send_event(&hook, LIBINPUT_EVENT_POINTER_AXIS);
    TEST_ASSERT(horizontal_wheels == 2 && emitted_hwheel == 1);
    consume = false;
    axis = -30;
    send_event(&hook, LIBINPUT_EVENT_POINTER_AXIS);
    TEST_ASSERT(horizontal_wheels == 0 && emitted_hwheel == -1);
    axis = 7.5;
    send_event(&hook, LIBINPUT_EVENT_POINTER_AXIS);
    axis = 0;
    send_event(&hook, LIBINPUT_EVENT_POINTER_AXIS);
    axis = 7.5;
    send_event(&hook, LIBINPUT_EVENT_POINTER_AXIS);
    TEST_ASSERT(horizontal_wheels == 0 && emitted_hwheel == -1);
    hook.uinput = NULL;

    // A regular file cannot be grabbed. Observation mode must not try to grab
    // it, and a failed grab in forwarding mode must downgrade to observation.
    int fd = touchpad_open("/dev/null", O_RDONLY, &hook);
    TEST_ASSERT(fd >= 0);
    touchpad_close(fd, &hook);
    hook.uinput = (struct libevdev_uinput *) &hook;
    fd = touchpad_open("/dev/null", O_RDONLY, &hook);
    TEST_ASSERT(fd >= 0 && hook.uinput == NULL && uinput_destroyed == 1);
    touchpad_close(fd, &hook);

    puts("Touchpad translation tests passed");
    return 0;
}
