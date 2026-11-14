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
#include <stdarg.h>
#include <string.h>
#include <uiohook.h>

#include "numeric_helper.h"
#include "test_assert.h"

static int warnings;
static char message[160];

static void capture_warning(unsigned int level, void *data, const char *format, va_list args) {
    TEST_ASSERT(level == LOG_LEVEL_WARN);
    TEST_ASSERT(data == &warnings);
    warnings++;
    vsnprintf(message, sizeof(message), format, args);
}

int main(void) {
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
    hook_set_logger_proc(capture_warning, &warnings);
    TEST_ASSERT(clamp_int16(INT16_MIN) == INT16_MIN);
    TEST_ASSERT(clamp_int16(INT16_MAX) == INT16_MAX);
    TEST_ASSERT(clamp_uint16(0) == 0);
    TEST_ASSERT(clamp_uint16(UINT16_MAX) == UINT16_MAX);
    TEST_ASSERT(warnings == 0);
    TEST_ASSERT(clamp_int16((int64_t) INT16_MIN - 1) == INT16_MIN);
    TEST_ASSERT(warnings == 1 && strstr(message, "underflow") != NULL);
    TEST_ASSERT(clamp_int16((int64_t) INT16_MAX + 1) == INT16_MAX);
    TEST_ASSERT(warnings == 2 && strstr(message, "overflow") != NULL);
    TEST_ASSERT(clamp_uint16(-1) == 0);
    TEST_ASSERT(warnings == 3 && strstr(message, "underflow") != NULL);
    TEST_ASSERT(clamp_uint16((int64_t) UINT16_MAX + 1) == UINT16_MAX);
    TEST_ASSERT(warnings == 4 && strstr(message, "overflow") != NULL);
    hook_set_logger_proc(NULL, NULL);
    return 0;
}
