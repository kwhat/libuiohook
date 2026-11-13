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

#include "test_assert.h"

int main(void) {
    // These expressions deliberately leave macro uses unwrapped, as callers may.
    uint32_t mask = 0;
    TEST_ASSERT((mask & MASK_SHIFT) == 0);
    TEST_ASSERT((mask & MASK_CTRL) == 0);
    TEST_ASSERT((mask & MASK_META) == 0);
    TEST_ASSERT((mask & MASK_ALT) == 0);

    mask = MASK_SHIFT_L | MASK_SHIFT_R | MASK_BUTTON1;
    mask &= ~MASK_SHIFT;
    TEST_ASSERT(mask == MASK_BUTTON1);
    TEST_ASSERT((MASK_BUTTON1 + 1U) == 257U);
    TEST_ASSERT((MASK_EMULATED >> 30) == 1U);
    TEST_ASSERT((MASK_CONSUMED >> 31) == 1U);
    TEST_ASSERT((MASK_CONSUMED | MASK_EMULATED) == UINT32_C(0xC0000000));

    TEST_ASSERT((VC_KP_UP & 0x00FF) == VC_UP);
    TEST_ASSERT((VC_KP_DOWN & 0x00FF) == VC_DOWN);
    TEST_ASSERT((VC_KP_BEGIN & 0x00FF) == (VC_BEGIN & 0x00FF));
    TEST_ASSERT((VC_KP_DELETE & 0x00FF) == VC_DELETE);
    TEST_ASSERT((VC_KP_UP == VC_UP) == 0);
    return EXIT_SUCCESS;
}
