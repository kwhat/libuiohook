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

#include <math.h>

#include "numeric_helper.h"
#include "test_assert.h"

int main(void) {
    TEST_ASSERT(clamp_int16(NAN) == 0);
    TEST_ASSERT(clamp_int16(INFINITY) == INT16_MAX);
    TEST_ASSERT(clamp_int16(-INFINITY) == INT16_MIN);
    TEST_ASSERT(clamp_int16(32767.9) == INT16_MAX);
    TEST_ASSERT(clamp_int16(-32768.9) == INT16_MIN);
    TEST_ASSERT(clamp_int16(-12.75) == -12);
    TEST_ASSERT(clamp_uint16(NAN) == 0);
    TEST_ASSERT(clamp_uint16(INFINITY) == UINT16_MAX);
    TEST_ASSERT(clamp_uint16(-INFINITY) == 0);
    TEST_ASSERT(clamp_uint16(-1.0) == 0);
    TEST_ASSERT(clamp_uint16(65535.9) == UINT16_MAX);
    TEST_ASSERT(clamp_uint16(12.75) == 12);
    TEST_ASSERT(clamp_int16(32767.0) == INT16_MAX);
    TEST_ASSERT(clamp_int16(-32768.0) == INT16_MIN);
    TEST_ASSERT(clamp_int16(32768.0) == INT16_MAX);
    TEST_ASSERT(clamp_int16(-32769.0) == INT16_MIN);
    TEST_ASSERT(clamp_int16(0.0) == 0);
    TEST_ASSERT(clamp_int16(12.75) == 12);
    TEST_ASSERT(clamp_int16(0x1p64) == INT16_MAX);
    TEST_ASSERT(clamp_int16(-0x1p64) == INT16_MIN);
    TEST_ASSERT(clamp_uint16(0.0) == 0);
    TEST_ASSERT(clamp_uint16(65535.0) == UINT16_MAX);
    TEST_ASSERT(clamp_uint16(65536.0) == UINT16_MAX);
    TEST_ASSERT(clamp_uint16(-0.75) == 0);
    TEST_ASSERT(clamp_uint16(0x1p64) == UINT16_MAX);
    TEST_ASSERT(clamp_uint16(-0x1p64) == 0);
    return 0;
}
