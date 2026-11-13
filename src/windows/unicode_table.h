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

#ifndef _included_unicode_table
#define _included_unicode_table

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct unicode_table;
// The reader validates each address/range before copying from the source image.
typedef bool (*unicode_memory_reader)(void *context, uint64_t address, void *buffer, size_t size);
struct unicode_state { uint16_t dead_char; };

extern bool unicode_table_load(struct unicode_table **table, unicode_memory_reader reader,
        void *context, uint64_t descriptor, unsigned int pointer_size);
extern void unicode_table_destroy(struct unicode_table **table);
// Capacity and return value are UTF-16 code units, never bytes. No terminator is written.
// Insufficient capacity leaves the buffer and dead-key state unchanged.
extern size_t unicode_table_translate(const struct unicode_table *table, struct unicode_state *state,
        uint16_t keycode, const uint8_t key_state[256], uint16_t *buffer, size_t capacity);

#endif
