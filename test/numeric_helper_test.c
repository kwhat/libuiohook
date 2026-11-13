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
#include <math.h>
#include "numeric_helper.h"
#include "test_assert.h"

int main(void) {
    TEST_ASSERT(clamp_int8(INT64_MIN) == INT8_MIN);
    TEST_ASSERT(clamp_int8(INT64_MAX) == INT8_MAX);
    TEST_ASSERT(clamp_int8(INT8_MIN) == INT8_MIN);
    TEST_ASSERT(clamp_int8(INT8_MAX) == INT8_MAX);
    TEST_ASSERT(clamp_int8(42) == 42);
    TEST_ASSERT(clamp_int8(-1) == -1);
    TEST_ASSERT(clamp_int8((int64_t) INT8_MIN - 1) == INT8_MIN);
    TEST_ASSERT(clamp_int8((int64_t) INT8_MAX + 1) == INT8_MAX);
    TEST_ASSERT(clamp_uint8(INT64_MIN) == 0);
    TEST_ASSERT(clamp_uint8(INT64_MAX) == UINT8_MAX);
    TEST_ASSERT(clamp_uint8(0) == 0);
    TEST_ASSERT(clamp_uint8(UINT8_MAX) == UINT8_MAX);
    TEST_ASSERT(clamp_uint8(42) == 42);
    TEST_ASSERT(clamp_uint8(-1) == 0);
    TEST_ASSERT(clamp_uint8((int64_t) 0 - 1) == 0);
    TEST_ASSERT(clamp_uint8((int64_t) UINT8_MAX + 1) == UINT8_MAX);
    TEST_ASSERT(clamp_int16(INT64_MIN) == INT16_MIN);
    TEST_ASSERT(clamp_int16(INT64_MAX) == INT16_MAX);
    TEST_ASSERT(clamp_int16(INT16_MIN) == INT16_MIN);
    TEST_ASSERT(clamp_int16(INT16_MAX) == INT16_MAX);
    TEST_ASSERT(clamp_int16(42) == 42);
    TEST_ASSERT(clamp_int16(-1) == -1);
    TEST_ASSERT(clamp_int16((int64_t) INT16_MIN - 1) == INT16_MIN);
    TEST_ASSERT(clamp_int16((int64_t) INT16_MAX + 1) == INT16_MAX);
    TEST_ASSERT(clamp_uint16(INT64_MIN) == 0);
    TEST_ASSERT(clamp_uint16(INT64_MAX) == UINT16_MAX);
    TEST_ASSERT(clamp_uint16(0) == 0);
    TEST_ASSERT(clamp_uint16(UINT16_MAX) == UINT16_MAX);
    TEST_ASSERT(clamp_uint16(42) == 42);
    TEST_ASSERT(clamp_uint16(-1) == 0);
    TEST_ASSERT(clamp_uint16((int64_t) 0 - 1) == 0);
    TEST_ASSERT(clamp_uint16((int64_t) UINT16_MAX + 1) == UINT16_MAX);
    TEST_ASSERT(clamp_int32(INT64_MIN) == INT32_MIN);
    TEST_ASSERT(clamp_int32(INT64_MAX) == INT32_MAX);
    TEST_ASSERT(clamp_int32(INT32_MIN) == INT32_MIN);
    TEST_ASSERT(clamp_int32(INT32_MAX) == INT32_MAX);
    TEST_ASSERT(clamp_int32(42) == 42);
    TEST_ASSERT(clamp_int32(-1) == -1);
    TEST_ASSERT(clamp_int32((int64_t) INT32_MIN - 1) == INT32_MIN);
    TEST_ASSERT(clamp_int32((int64_t) INT32_MAX + 1) == INT32_MAX);
    TEST_ASSERT(clamp_uint32(INT64_MIN) == 0);
    TEST_ASSERT(clamp_uint32(INT64_MAX) == UINT32_MAX);
    TEST_ASSERT(clamp_uint32(0) == 0);
    TEST_ASSERT(clamp_uint32(UINT32_MAX) == UINT32_MAX);
    TEST_ASSERT(clamp_uint32(42) == 42);
    TEST_ASSERT(clamp_uint32(-1) == 0);
    TEST_ASSERT(clamp_uint32((int64_t) 0 - 1) == 0);
    TEST_ASSERT(clamp_uint32((int64_t) UINT32_MAX + 1) == UINT32_MAX);
    TEST_ASSERT(clamp_int16_from_double(NAN) == 0);
    TEST_ASSERT(clamp_int16_from_double(INFINITY) == INT16_MAX);
    TEST_ASSERT(clamp_int16_from_double(-INFINITY) == INT16_MIN);
    TEST_ASSERT(clamp_int16_from_double(32767.9) == INT16_MAX);
    TEST_ASSERT(clamp_int16_from_double(-32768.9) == INT16_MIN);
    TEST_ASSERT(clamp_int16_from_double(-12.75) == -12);
    TEST_ASSERT(clamp_uint16_from_double(NAN) == 0);
    TEST_ASSERT(clamp_uint16_from_double(INFINITY) == UINT16_MAX);
    TEST_ASSERT(clamp_uint16_from_double(-INFINITY) == 0);
    TEST_ASSERT(clamp_uint16_from_double(-1.0) == 0);
    TEST_ASSERT(clamp_uint16_from_double(65535.9) == UINT16_MAX);
    TEST_ASSERT(clamp_uint16_from_double(12.75) == 12);
    return 0;
}
