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

#include <uiohook.h>

#include "darwin/keycode_helper.h"
#include "test_assert.h"

static void test_known_mappings(void) {
    // Hardware keycode fixtures are independent of the old scan-code reverse
    // table and require no Carbon headers or active keyboard layout.
    const struct {
        uint16_t native;
        uint16_t virtual_code;
    } fixtures[] = {
        { 0x00, VC_A },
        { 0x24, VC_ENTER },
        { 0x30, VC_TAB },
        { 0x31, VC_SPACE },
        { 0x33, VC_BACKSPACE },
        { 0x35, VC_ESCAPE },
        { 0x36, VC_META_R },
        { 0x37, VC_META_L },
        { 0x38, VC_SHIFT_L },
        { 0x3A, VC_ALT_L },
        { 0x3B, VC_CONTROL_L },
        { 0x3C, VC_SHIFT_R },
        { 0x3D, VC_ALT_R },
        { 0x3E, VC_CONTROL_R },
        { 0x4C, VC_KP_ENTER },
        { 0x52, VC_KP_0 },
        { 0x69, VC_F13 },
        { 0x7A, VC_F1 },
        { 0x7B, VC_LEFT },
        { 0x7C, VC_RIGHT },
        { 0x7D, VC_DOWN },
        { 0x7E, VC_UP }
    };

    for (unsigned int i = 0; i < sizeof(fixtures) / sizeof(fixtures[0]); i++) {
        TEST_ASSERT(keycode_to_uiocode(fixtures[i].native) == fixtures[i].virtual_code);
        TEST_ASSERT(uiocode_to_keycode(fixtures[i].virtual_code) == fixtures[i].native);
    }
}

static void test_unknown_and_aliased_keys(void) {
    TEST_ASSERT(uiocode_to_keycode(VC_A) == 0); // A valid key, not a failure sentinel.
    TEST_ASSERT(uiocode_to_keycode(VC_UNDEFINED) == DARWIN_KEYCODE_UNDEFINED);
    TEST_ASSERT(uiocode_to_keycode(UINT16_MAX) == DARWIN_KEYCODE_UNDEFINED);
    TEST_ASSERT(keycode_to_uiocode(DARWIN_KEYCODE_UNDEFINED) == VC_UNDEFINED);
    TEST_ASSERT(keycode_to_uiocode(256) == VC_UNDEFINED);
    TEST_ASSERT(keycode_to_uiocode(UINT16_MAX) == VC_UNDEFINED);

    for (uint16_t native = 0; native < 256; native++) {
        uint16_t code = keycode_to_uiocode(native);
        if (code != VC_UNDEFINED) {
            uint16_t restored = uiocode_to_keycode(code);
            TEST_ASSERT(restored != DARWIN_KEYCODE_UNDEFINED);
            TEST_ASSERT(keycode_to_uiocode(restored) == code);
        }
    }
}

static void test_modifier_and_drag_state(void) {
    unset_modifier_mask(UINT16_MAX);
    TEST_ASSERT(get_modifiers() == 0 && !is_mouse_dragged());
    set_mouse_dragged(true);
    TEST_ASSERT(is_mouse_dragged() && get_modifiers() == 0);
    set_modifier_mask(MASK_SHIFT_L | MASK_BUTTON1 | MASK_META_R);
    TEST_ASSERT(get_modifiers() == (MASK_SHIFT_L | MASK_BUTTON1 | MASK_META_R));
    unset_modifier_mask(MASK_BUTTON1);
    TEST_ASSERT(get_modifiers() == (MASK_SHIFT_L | MASK_META_R));
    set_mouse_dragged(false);
    TEST_ASSERT(!is_mouse_dragged() && get_modifiers() == (MASK_SHIFT_L | MASK_META_R));
    unset_modifier_mask(UINT16_MAX);
    TEST_ASSERT(get_modifiers() == 0);
}

int main(void) {
    test_known_mappings();
    test_unknown_and_aliased_keys();
    test_modifier_and_drag_state();
    puts("Darwin keycode helper tests passed");
    return EXIT_SUCCESS;
}
