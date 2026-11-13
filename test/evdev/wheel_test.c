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
#include <libevdev/libevdev-uinput.h>
#include <linux/input.h>
#include <uiohook.h>

#include "dispatch_event.h"
#include "input_helper.h"
#include "mock_desktop.h"
#include "test_assert.h"

static int device_token, movement, writes, status;
static unsigned int last_code;
static uiohook_event captured;

int libevdev_uinput_create_from_device(const struct libevdev *device, int fd,
        struct libevdev_uinput **uinput) {
    (void) device;
    TEST_ASSERT(fd == LIBEVDEV_UINPUT_OPEN_MANAGED);
    *uinput = (struct libevdev_uinput *) &device_token;
    return 0;
}

void libevdev_uinput_destroy(struct libevdev_uinput *device) {
    TEST_ASSERT(device == (struct libevdev_uinput *) &device_token);
}

int libevdev_uinput_write_event(const struct libevdev_uinput *device,
        unsigned int type, unsigned int code, int value) {
    TEST_ASSERT(device == (struct libevdev_uinput *) &device_token);
    if (status != 0) {
        return status;
    }
    if (type == EV_REL) {
        last_code = code;
        movement += value;
        writes++;
    } else {
        TEST_ASSERT(type == EV_SYN && code == SYN_REPORT && value == 0);
    }
    return 0;
}

static void capture(uiohook_event *const event, void *data) {
    (void) data;
    captured = *event;
}

int main(void) {
    mock_desktop_reset();
    hook_set_dispatch_proc(capture, NULL);
    struct input_event native = { 0 };
    dispatch_mouse_wheel(&native, 3, WHEEL_VERTICAL_DIRECTION);
    TEST_ASSERT(captured.data.wheel.amount == 1);
    TEST_ASSERT(captured.data.wheel.rotation == 360 && captured.data.wheel.delta == 120);
    TEST_ASSERT(hook_post_event(&captured) == UIOHOOK_SUCCESS);
    TEST_ASSERT(movement == 3 && last_code == REL_WHEEL);
    dispatch_mouse_wheel(&native, -2, WHEEL_HORIZONTAL_DIRECTION);
    TEST_ASSERT(hook_post_event(&captured) == UIOHOOK_SUCCESS);
    TEST_ASSERT(movement == 1 && last_code == REL_HWHEEL);

    captured.data.wheel.amount = 0; // Never a divisor.
    captured.data.wheel.rotation = 60;
    int previous = writes;
    TEST_ASSERT(hook_post_event(&captured) == UIOHOOK_SUCCESS && writes == previous);
    captured.data.wheel.direction = WHEEL_VERTICAL_DIRECTION;
    TEST_ASSERT(hook_post_event(&captured) == UIOHOOK_SUCCESS && writes == previous);
    captured.data.wheel.direction = WHEEL_HORIZONTAL_DIRECTION;
    TEST_ASSERT(hook_post_event(&captured) == UIOHOOK_SUCCESS && movement == 2);
    captured.data.wheel.direction = WHEEL_VERTICAL_DIRECTION;
    TEST_ASSERT(hook_post_event(&captured) == UIOHOOK_SUCCESS && movement == 3);

    captured.data.wheel.rotation = -120;
    status = -EIO;
    TEST_ASSERT(hook_post_event(&captured) == UIOHOOK_FAILURE);
    status = 0;
    TEST_ASSERT(hook_post_event(&captured) == UIOHOOK_SUCCESS && movement == 2);
    captured.data.wheel.delta = 0;
    TEST_ASSERT(hook_post_event(&captured) == UIOHOOK_FAILURE);
    captured.data.wheel.delta = 120;
    captured.data.wheel.direction = 0;
    TEST_ASSERT(hook_post_event(&captured) == UIOHOOK_FAILURE);
    return EXIT_SUCCESS;
}
