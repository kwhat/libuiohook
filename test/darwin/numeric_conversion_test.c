/* libUIOHook: Cross-platform keyboard and mouse hooking from userland.
 * Copyright (C) 2006-2023 Alexander Barker.  All Rights Reserved.
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
#include "darwin/input_helper.h"
#include "numeric_helper.h"
#include "test_assert.h"

int main(void) {
    TEST_ASSERT(conv_double_to_int64(0x1p63) == INT64_MAX);
    TEST_ASSERT(conv_double_to_int64(-0x1p63) == INT64_MIN);
    TEST_ASSERT(conv_double_to_int64(0x1.fffffffffffffp62) == INT64_MAX - 1023);
    TEST_ASSERT(conv_double_to_int64(-0x1.fffffffffffffp62) == INT64_MIN + 1024);
    TEST_ASSERT(conv_double_to_int64(0x1p64) == INT64_MAX);
    TEST_ASSERT(conv_double_to_int64(-0x1p64) == INT64_MIN);
    TEST_ASSERT(clamp_int16(conv_double_to_int64(NAN)) == 0);
    TEST_ASSERT(clamp_int16(conv_double_to_int64(INFINITY)) == INT16_MAX);
    TEST_ASSERT(clamp_int16(conv_double_to_int64(-INFINITY)) == INT16_MIN);
    TEST_ASSERT(clamp_int16(conv_double_to_int64(32767.9)) == INT16_MAX);
    TEST_ASSERT(clamp_int16(conv_double_to_int64(-32768.9)) == INT16_MIN);
    TEST_ASSERT(clamp_int16(conv_double_to_int64(-12.75)) == -12);
    TEST_ASSERT(clamp_uint16(conv_double_to_int64(NAN)) == 0);
    TEST_ASSERT(clamp_uint16(conv_double_to_int64(INFINITY)) == UINT16_MAX);
    TEST_ASSERT(clamp_uint16(conv_double_to_int64(-INFINITY)) == 0);
    TEST_ASSERT(clamp_uint16(conv_double_to_int64(-1.0)) == 0);
    TEST_ASSERT(clamp_uint16(conv_double_to_int64(65535.9)) == UINT16_MAX);
    TEST_ASSERT(clamp_uint16(conv_double_to_int64(12.75)) == 12);
    return 0;
}
