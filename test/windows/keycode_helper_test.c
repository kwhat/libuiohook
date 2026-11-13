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

#include "windows/keycode_helper.h"
#include "test_assert.h"

static void test_key_mappings(void) {
    const struct {
        uint16_t native;
        uint16_t virtual_code;
    } fixtures[] = {
        { VK_ESCAPE, VC_ESCAPE },
        { VK_RETURN, VC_ENTER },
        { VK_SPACE, VC_SPACE },
        { 0x41, VC_A },
        { VK_F1, VC_F1 },
        { VK_F13, VC_F13 },
        { VK_NUMPAD0, VC_KP_0 },
        { VK_LSHIFT, VC_SHIFT_L },
        { VK_RSHIFT, VC_SHIFT_R },
        { VK_LCONTROL, VC_CONTROL_L },
        { VK_RCONTROL, VC_CONTROL_R },
        { VK_LMENU, VC_ALT_L },
        { VK_RMENU, VC_ALT_R },
        { VK_LWIN, VC_META_L },
        { VK_RWIN, VC_META_R }
    };

    for (unsigned int i = 0; i < sizeof(fixtures) / sizeof(fixtures[0]); i++) {
        TEST_ASSERT(vkcode_to_uiocode(fixtures[i].native, 0) == fixtures[i].virtual_code);
        TEST_ASSERT(uiocode_to_vkcode(fixtures[i].virtual_code) == fixtures[i].native);
    }

    TEST_ASSERT(vkcode_to_uiocode(0xFFFF, 0) == VC_UNDEFINED);
    TEST_ASSERT(vkcode_to_uiocode(VK_RETURN, LLKHF_EXTENDED) == VC_KP_ENTER);
    TEST_ASSERT(uiocode_to_vkcode(VC_KP_ENTER) == VK_RETURN);
    TEST_ASSERT(vkcode_to_uiocode(VK_LEFT, LLKHF_EXTENDED) == VC_LEFT);
    TEST_ASSERT(vkcode_to_uiocode(VK_LEFT, 0) == VC_KP_LEFT);
    TEST_ASSERT(uiocode_to_vkcode(VC_LEFT) == VK_LEFT);
    TEST_ASSERT(uiocode_to_vkcode(VC_KP_LEFT) == VK_LEFT);
    TEST_ASSERT(uiocode_to_vkcode(VC_UNDEFINED) == 0);
    TEST_ASSERT(uiocode_to_vkcode(0xFFFF) == 0);
    TEST_ASSERT(vkcode_to_uiocode(VK_SHIFT, 0) == VC_SHIFT_L);
    TEST_ASSERT(vkcode_to_uiocode(VK_CONTROL, 0) == VC_CONTROL_L);
    TEST_ASSERT(vkcode_to_uiocode(VK_MENU, 0) == VC_ALT_L);
    const DWORD flags[] = { 0, LLKHF_EXTENDED };
    for (unsigned int i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
        for (DWORD native = 0; native < 256; native++) {
            uint16_t code = vkcode_to_uiocode(native, flags[i]);
            if (code != VC_UNDEFINED) {
                DWORD restored = uiocode_to_vkcode(code);
                TEST_ASSERT(restored != 0);
                TEST_ASSERT(vkcode_to_uiocode(restored, flags[i]) == code);
            }
        }
    }
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

int main(void) {
    test_key_mappings();
    test_modifier_masks();
    puts("Windows keycode helper tests passed");
    return EXIT_SUCCESS;
}
