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

#include <stdint.h>
#include <uiohook.h>

#include "input_helper.h"
#include "test_assert.h"

#if !defined(_WIN32) && !defined(__APPLE__)
#include <linux/input-event-codes.h>
#include "evdev/mock_desktop.h"
#endif

static void test_key_mappings(void) {
    // Explicit expected mappings catch errors that a round trip can hide.
    const struct {
        uint16_t native;
        uint16_t virtual_code;
    } fixtures[] = {
#if defined(_WIN32)
        { VK_ESCAPE, VC_ESCAPE },
        { VK_RETURN, VC_ENTER },
        { VK_SPACE, VC_SPACE },
        { 0x41, VC_A }
#elif defined(__APPLE__)
        { kVK_Escape, VC_ESCAPE },
        { kVK_Return, VC_ENTER },
        { kVK_Space, VC_SPACE },
        { kVK_ANSI_A, VC_A }
#else
        { XKB_KEY_Escape, VC_ESCAPE },
        { XKB_KEY_Return, VC_ENTER },
        { XKB_KEY_space, VC_SPACE },
        { XKB_KEY_a, VC_A }
#endif
    };

    for (unsigned int i = 0; i < sizeof(fixtures) / sizeof(fixtures[0]); i++) {
#if defined(_WIN32)
        TEST_ASSERT(vkcode_to_uiocode(fixtures[i].native, 0) == fixtures[i].virtual_code);
        TEST_ASSERT(uiocode_to_vkcode(fixtures[i].virtual_code) == fixtures[i].native);
#elif defined(__APPLE__)
        TEST_ASSERT(keycode_to_uiocode(fixtures[i].native) == fixtures[i].virtual_code);
        TEST_ASSERT(uiocode_to_keycode(fixtures[i].virtual_code) == fixtures[i].native);
#else
        TEST_ASSERT(keysym_to_uiocode(fixtures[i].native) == fixtures[i].virtual_code);
#endif
    }

#if defined(_WIN32)
    TEST_ASSERT(vkcode_to_uiocode(0xFFFF, 0) == VC_UNDEFINED);
    TEST_ASSERT(vkcode_to_uiocode(VK_RETURN, LLKHF_EXTENDED) == VC_KP_ENTER);
#elif defined(__APPLE__)
    TEST_ASSERT(keycode_to_uiocode(0xFFFF) == VC_UNDEFINED);
#else
    TEST_ASSERT(keysym_to_uiocode(XKB_KEY_NoSymbol) == VC_UNDEFINED);
    TEST_ASSERT(event_to_keycode(KEY_A) == 38);
    TEST_ASSERT(keycode_to_event(38) == KEY_A);
    TEST_ASSERT(keycode_to_event(7) == 0);
    for (uint16_t code = 0; code <= KEY_MAX; code++) {
        TEST_ASSERT(keycode_to_event(event_to_keycode(code)) == code);
    }
#endif
}

static void test_modifier_masks(void) {
    unset_modifier_mask(UINT16_MAX);
    TEST_ASSERT(get_modifiers() == 0);
    set_modifier_mask(MASK_SHIFT_L);
    set_modifier_mask(MASK_BUTTON1);
    set_modifier_mask(MASK_BUTTON3);
    TEST_ASSERT(get_modifiers() == (MASK_SHIFT_L | MASK_BUTTON1 | MASK_BUTTON3));
    unset_modifier_mask(MASK_BUTTON1);
    TEST_ASSERT(get_modifiers() == (MASK_SHIFT_L | MASK_BUTTON3));
    unset_modifier_mask(UINT16_MAX);
    TEST_ASSERT(get_modifiers() == 0);
}

#if !defined(_WIN32) && !defined(__APPLE__)
static void test_fixture_keymap_and_pointer(void) {
    mock_desktop_reset();
    mock_desktop.wayland_screens = true;
    setenv("WAYLAND_DISPLAY", "mock-wayland", 1);
    load_input_helper();
    TEST_ASSERT(uiocode_to_keycode(VC_A) == 38);
    TEST_ASSERT(event_to_keysym(38, KEY_STATE_PRESS) == XKB_KEY_a);
    TEST_ASSERT(event_to_keysym(38, KEY_STATE_RELEASE) == XKB_KEY_a);
    TEST_ASSERT(uiocode_to_keycode(VC_UNDEFINED) == 0);

    int16_t x, y;
    get_pointer_position(&x, &y);
    TEST_ASSERT(x == 100 && y == 80);
    track_pointer_delta(10, -20);
    get_pointer_position(&x, &y);
    TEST_ASSERT(x == 110 && y == 60);
    track_pointer_delta(5000, -5000);
    get_pointer_position(&x, &y);
    TEST_ASSERT(x == 1919 && y == 0);
    unload_input_helper();
    TEST_ASSERT(event_to_keysym(38, KEY_STATE_PRESS) == XKB_KEY_NoSymbol);
    unsetenv("WAYLAND_DISPLAY");
}
#endif

void input_helper_tests(void) {
    test_key_mappings();
    test_modifier_masks();
#if !defined(_WIN32) && !defined(__APPLE__)
    test_fixture_keymap_and_pointer();
#endif
}
