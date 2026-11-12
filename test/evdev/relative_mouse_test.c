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

#include <limits.h>
#include <uiohook.h>

#include "dispatch_event.h"
#include "input_helper.h"
#include "mock_desktop.h"
#include "test_assert.h"

static uiohook_event received;
static bool consume;
static int clicks;

static void capture_event(uiohook_event *const event, void *data) {
    (void) data;
    received = *event;
    if (event->type == EVENT_MOUSE_PRESSED || event->type == EVENT_MOUSE_RELEASED
            || event->type == EVENT_MOUSE_CLICKED) {
        TEST_ASSERT(event->data.mouse.dx == 0 && event->data.mouse.dy == 0);
    }
    if (event->type == EVENT_MOUSE_CLICKED) {
        clicks++;
    }
    if (consume) {
        event->mask |= MASK_CONSUMED;
    }
}

static void initialize_pointer(void) {
    mock_desktop_reset();
    mock_desktop.wayland_screens = true;
    setenv("WAYLAND_DISPLAY", "mock-wayland", 1);
    unset_modifier_mask(UINT16_MAX);
    init_pointer_tracking();
}

static void test_relative_motion(struct input_event *event) {
    initialize_pointer();

    // A physical mouse's accumulated REL_X/REL_Y values are passed directly to
    // dispatch, rather than reconstructed by subtracting absolute coordinates.
    track_pointer_delta(8, -5);
    TEST_ASSERT(!dispatch_mouse_move(event, 8, -5));
    TEST_ASSERT(received.type == EVENT_MOUSE_MOVED);
    TEST_ASSERT(received.data.mouse.x == 108 && received.data.mouse.y == 75);
    TEST_ASSERT(received.data.mouse.dx == 8 && received.data.mouse.dy == -5);

    set_pointer_position(1919, 1079);
    track_pointer_delta(17, 20);
    dispatch_mouse_move(event, 17, 20);
    TEST_ASSERT(received.data.mouse.x == 1919 && received.data.mouse.y == 1079);
    TEST_ASSERT(received.data.mouse.dx == 17 && received.data.mouse.dy == 20);

    set_pointer_position(0, 0);
    track_pointer_delta(-12, -9);
    dispatch_mouse_move(event, -12, -9);
    TEST_ASSERT(received.data.mouse.x == 0 && received.data.mouse.y == 0);
    TEST_ASSERT(received.data.mouse.dx == -12 && received.data.mouse.dy == -9);

    track_pointer_delta(0, 9);
    dispatch_mouse_move(event, 0, 9);
    TEST_ASSERT(received.data.mouse.dx == 0 && received.data.mouse.dy == 9);

    // Test narrowing at the dispatch boundary without overflowing the tracker's
    // integer position with deliberately extreme synthetic input values.
    dispatch_mouse_move(event, INT_MAX, INT_MIN);
    TEST_ASSERT(received.data.mouse.dx == INT16_MAX && received.data.mouse.dy == INT16_MIN);
    dispatch_mouse_move(event, INT16_MIN, INT16_MAX);
    TEST_ASSERT(received.data.mouse.dx == INT16_MIN && received.data.mouse.dy == INT16_MAX);

    consume = true;
    track_pointer_delta(3, -2);
    TEST_ASSERT(dispatch_mouse_move(event, 3, -2));
    TEST_ASSERT(received.data.mouse.dx == 3 && received.data.mouse.dy == -2);
    consume = false;
}

static void test_drag_and_button_events(struct input_event *event) {
    initialize_pointer();
    dispatch_mouse_move(event, 29, -11);
    set_modifier_mask(MASK_BUTTON1);
    dispatch_mouse_press(event, MOUSE_BUTTON1);
    TEST_ASSERT(received.type == EVENT_MOUSE_PRESSED);

    track_pointer_delta(6, -4);
    dispatch_mouse_move(event, 6, -4);
    TEST_ASSERT(received.type == EVENT_MOUSE_DRAGGED);
    TEST_ASSERT((received.mask & MASK_BUTTON1) != 0);
    TEST_ASSERT(received.data.mouse.dx == 6 && received.data.mouse.dy == -4);

    unset_modifier_mask(MASK_BUTTON1);
    dispatch_mouse_release(event, MOUSE_BUTTON1);
    TEST_ASSERT(received.type == EVENT_MOUSE_RELEASED && clicks == 0);

    // Wheel data shares the event union. Button events must clear deltas even
    // after another event type has overwritten that storage.
    dispatch_mouse_wheel(event, 1, WHEEL_VERTICAL_DIRECTION);
    set_modifier_mask(MASK_BUTTON1);
    dispatch_mouse_press(event, MOUSE_BUTTON1);
    unset_modifier_mask(MASK_BUTTON1);
    dispatch_mouse_release(event, MOUSE_BUTTON1);
    TEST_ASSERT(received.type == EVENT_MOUSE_CLICKED && clicks == 1);
}

int main(void) {
    struct input_event event = { 0 };
    event.input_event_sec = 1700000000;
    event.input_event_usec = 123456;
    hook_set_dispatch_proc(capture_event, NULL);
    test_relative_motion(&event);
    test_drag_and_button_events(&event);
    unsetenv("WAYLAND_DISPLAY");
    puts("Relative mouse motion tests passed");
    return EXIT_SUCCESS;
}
