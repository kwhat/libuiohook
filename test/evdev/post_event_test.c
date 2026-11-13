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

#include <libevdev/libevdev-uinput.h>
#include <linux/input.h>
#include <stdlib.h>
#include <uiohook.h>

#include "input_helper.h"
#include "evdev/mock_desktop.h"
#include "test_assert.h"

static int device_token;
static unsigned int create_calls, write_calls;
static struct input_event writes[16];

int libevdev_uinput_create_from_device(const struct libevdev *device, int fd,
        struct libevdev_uinput **uinput) {
    (void) device;
    TEST_ASSERT(fd == LIBEVDEV_UINPUT_OPEN_MANAGED);
    create_calls++;
    *uinput = (struct libevdev_uinput *) &device_token;
    return 0;
}

void libevdev_uinput_destroy(struct libevdev_uinput *device) {
    TEST_ASSERT(device == (struct libevdev_uinput *) &device_token);
}

int libevdev_uinput_write_event(const struct libevdev_uinput *device,
        unsigned int type, unsigned int code, int value) {
    TEST_ASSERT(device == (struct libevdev_uinput *) &device_token);
    TEST_ASSERT(write_calls < sizeof(writes) / sizeof(writes[0]));
    writes[write_calls++] = (struct input_event) {
        .type = type, .code = code, .value = value
    };
    return 0;
}

static void post_key(uint16_t keycode, unsigned int native, event_type type) {
    uiohook_event event = { .type = type };
    event.data.keyboard.keycode = keycode;
    unsigned int first = write_calls;
    TEST_ASSERT(hook_post_event(&event) == UIOHOOK_SUCCESS);
    TEST_ASSERT(write_calls == first + 2);
    TEST_ASSERT(writes[first].type == EV_KEY && writes[first].code == native);
    TEST_ASSERT(writes[first].value == (type == EVENT_KEY_PRESSED ? 1 : 0));
    TEST_ASSERT(writes[first + 1].type == EV_SYN);
    TEST_ASSERT(writes[first + 1].code == SYN_REPORT && writes[first + 1].value == 0);
}

int main(void) {
    mock_desktop_reset();
    setenv("WAYLAND_DISPLAY", "mock-wayland", 1);
    unload_input_helper();

    // No hook or explicit helper initialization precedes these crafted events.
    post_key(VC_UP, KEY_UP, EVENT_KEY_PRESSED);
    post_key(VC_UP, KEY_UP, EVENT_KEY_RELEASED);
    post_key(VC_DOWN, KEY_DOWN, EVENT_KEY_PRESSED);
    post_key(VC_DOWN, KEY_DOWN, EVENT_KEY_RELEASED);

    // Starting a hook after posting must preserve existing helper state.
    track_pointer_delta(10, -20);
    load_input_helper();
    int16_t x, y;
    get_pointer_position(&x, &y);
    TEST_ASSERT(x == 110 && y == 60);

    // A stopped hook frees its keymap; subsequent posting must initialize again.
    unload_input_helper();
    post_key(VC_A, KEY_A, EVENT_KEY_PRESSED);
    TEST_ASSERT(create_calls == 1);
    unload_input_helper();
    unsetenv("WAYLAND_DISPLAY");
    return EXIT_SUCCESS;
}
