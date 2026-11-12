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

#include <stdarg.h>
#include <string.h>
#include <uiohook.h>

#include "logger.h"
#include "test_assert.h"

static unsigned int received_level;
static int calls;
static char message[128];

static void mock_logger(unsigned int level, void *data, const char *format, va_list args) {
    TEST_ASSERT(data == &calls);
    received_level = level;
    calls++;
    vsnprintf(message, sizeof(message), format, args);
}

int main(void) {
    logger(LOG_LEVEL_INFO, "No callback");
    TEST_ASSERT(calls == 0);
    hook_set_logger_proc(mock_logger, &calls);
    logger(LOG_LEVEL_WARN, "%s %d", "Fixture", 42);
    TEST_ASSERT(calls == 1);
    TEST_ASSERT(received_level == LOG_LEVEL_WARN);
    TEST_ASSERT(strcmp(message, "Fixture 42") == 0);
    hook_set_logger_proc(NULL, NULL);
    logger(LOG_LEVEL_ERROR, "Callback removed");
    TEST_ASSERT(calls == 1);
    return EXIT_SUCCESS;
}
