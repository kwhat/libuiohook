/* libUIOHook: Cross-platform keyboard and mouse hooking from userland.
 * Copyright (C) 2006-2022 Alexander Barker.  All Rights Reserved.
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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <uiohook.h>

#include "logger.h"

static logger_t callback = NULL;
static void *callback_data = NULL;

void logger(unsigned int level, const char *format, ...) {
    if (callback != NULL) {
        va_list args;

        va_start(args, format);
        callback(level, callback_data, format, args);
        va_end(args);
    }
}

UIOHOOK_API void hook_set_logger_proc(logger_t logger_proc, void *user_data) {
    callback = logger_proc;
    callback_data = user_data;
}
