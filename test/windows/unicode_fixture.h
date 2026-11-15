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

#ifndef UIOHOOK_UNICODE_FIXTURE_H
#define UIOHOOK_UNICODE_FIXTURE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct unicode_fixture {
    uint8_t bytes[8192];
    uint64_t base;
    size_t size;
    unsigned int pointer_size;
};
extern void unicode_fixture_init(struct unicode_fixture *fixture, unsigned int width, uint64_t base);
extern bool unicode_fixture_read(void *context, uint64_t address, void *buffer, size_t size);
extern void unicode_fixture_pointer(struct unicode_fixture *fixture, size_t offset, uint64_t value);
#endif
