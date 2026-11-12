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

#include <string.h>
#include <uiohook.h>

#include "test_assert.h"
#include "evdev/mock_desktop.h"

static void test_repeat(void) {
    mock_desktop_reset();
    TEST_ASSERT(hook_get_auto_repeat_rate() == -1);
    TEST_ASSERT(hook_get_auto_repeat_delay() == -1);

    mock_desktop.x11_available = true;
    TEST_ASSERT(hook_get_auto_repeat_rate() == 33);
    TEST_ASSERT(hook_get_auto_repeat_delay() == 600);
    mock_desktop.repeat_success = false;
    TEST_ASSERT(hook_get_auto_repeat_rate() == -1);
    TEST_ASSERT(hook_get_auto_repeat_delay() == -1);

    setenv("WAYLAND_DISPLAY", "mock-wayland", 1);
    mock_desktop.x11_calls = 0;
    TEST_ASSERT(hook_get_auto_repeat_rate() == 40);
    TEST_ASSERT(hook_get_auto_repeat_delay() == 400);
    TEST_ASSERT(mock_desktop.x11_calls == 0);

    mock_desktop.repeat_success = true;
    mock_desktop.wayland_rate = 0;
    mock_desktop.wayland_delay = -1;
    TEST_ASSERT(hook_get_auto_repeat_rate() == 33);
    TEST_ASSERT(hook_get_auto_repeat_delay() == 600);
    mock_desktop.wayland_status = UIOHOOK_FAILURE;
    TEST_ASSERT(hook_get_auto_repeat_rate() == 33);
    TEST_ASSERT(hook_get_auto_repeat_delay() == 600);
    unsetenv("WAYLAND_DISPLAY");
}

static void test_pointer(void) {
    mock_desktop_reset();
    TEST_ASSERT(hook_get_pointer_acceleration_multiplier() == -1);
    TEST_ASSERT(hook_get_pointer_acceleration_threshold() == -1);
    TEST_ASSERT(hook_get_pointer_sensitivity() == -1);

    mock_desktop.x11_available = true;
    TEST_ASSERT(hook_get_pointer_acceleration_multiplier() == 2);
    TEST_ASSERT(hook_get_pointer_acceleration_threshold() == 4);
    TEST_ASSERT(hook_get_pointer_sensitivity() == 3);
    mock_desktop.numerator = -1;
    mock_desktop.denominator = -1;
    mock_desktop.threshold = -1;
    TEST_ASSERT(hook_get_pointer_acceleration_multiplier() == -1);
    TEST_ASSERT(hook_get_pointer_acceleration_threshold() == -1);
    TEST_ASSERT(hook_get_pointer_sensitivity() == -1);
}

static void test_click_time(void) {
    mock_desktop_reset();
    TEST_ASSERT(hook_get_multi_click_time() == 200);
    mock_desktop.x11_available = true;
    mock_desktop.click_time = "350";
    TEST_ASSERT(hook_get_multi_click_time() == 350);
    mock_desktop.click_time = "invalid";
    mock_desktop.fallback_click_time = "450";
    TEST_ASSERT(hook_get_multi_click_time() == 450);
    mock_desktop.fallback_click_time = "invalid";
    TEST_ASSERT(hook_get_multi_click_time() == 200);
}

static void test_screens(void) {
    mock_desktop_reset();
    unsigned char count = 99;
    TEST_ASSERT(hook_create_screen_info(&count) == NULL);
    TEST_ASSERT(count == 0);

    mock_desktop.x11_available = true;
    screen_data *screens = hook_create_screen_info(&count);
    TEST_ASSERT(screens != NULL && count == 1);
    TEST_ASSERT(screens[0].width == 1920 && screens[0].height == 1080);
    free(screens);

    mock_desktop.xinerama_available = true;
    screens = hook_create_screen_info(&count);
    TEST_ASSERT(screens != NULL && count == 2);
    TEST_ASSERT(screens[0].x == -1280 && screens[0].width == 1280);
    TEST_ASSERT(screens[1].x == 0 && screens[1].width == 1920);
    free(screens);

    setenv("WAYLAND_DISPLAY", "mock-wayland", 1);
    mock_desktop.wayland_screens = true;
    screens = hook_create_screen_info(&count);
    TEST_ASSERT(screens != NULL && count == 1);
    TEST_ASSERT(screens[0].width == 1920);
    free(screens);
    mock_desktop.wayland_screens = false;
    screens = hook_create_screen_info(&count);
    TEST_ASSERT(screens != NULL && count == 2);
    free(screens);
    unsetenv("WAYLAND_DISPLAY");
}

int main(int argc, char **argv) {
    TEST_ASSERT(argc == 2);
    if (strcmp(argv[1], "repeat") == 0) {
        test_repeat();
    } else if (strcmp(argv[1], "pointer") == 0) {
        test_pointer();
    } else if (strcmp(argv[1], "click_time") == 0) {
        test_click_time();
    } else if (strcmp(argv[1], "screens") == 0) {
        test_screens();
    } else {
        TEST_ASSERT(0 && "Unknown test case");
    }
    return EXIT_SUCCESS;
}
