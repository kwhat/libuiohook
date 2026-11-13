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

/* This decoder continues the keyboard-table approach derived from
 * Marc-André Moreau's original code. See unicode_helper.c for the retained
 * attribution, permission notice, and archived original tutorial.
 */

#include <stdlib.h>
#include <string.h>
#include "unicode_table.h"

// Keyboard DLLs use little-endian Windows kbd.h layouts. Decode their fields
// explicitly: BUILD_WOW6432 uses 8-byte table pointers even in an x86 process.
// https://github.com/microsoft/win32metadata/blob/main/generation/WinSDK/RecompiledIdlHeaders/um/kbd.h
#define WCH_NONE 0xF000
#define WCH_DEAD 0xF001
#define WCH_LGTR 0xF002
#define CAPLOK 0x01
#define SGCAPS 0x02
#define CAPLOKALTGR 0x04
#define KLLF_ALTGR 0x01
#define SHFT_INVALID 0x0F
#define VK_SHIFT_CODE 0x10
#define VK_CONTROL_CODE 0x11
#define VK_MENU_CODE 0x12
#define VK_CAPITAL_CODE 0x14

// Limits bound malformed or unterminated tables; ordinary layouts are much smaller.
#define MAX_CHARACTER_TABLES 256
#define MAX_CHARACTER_ROWS 1024
#define MAX_DEAD_KEYS 65536
#define MAX_LIGATURES 4096

struct character_table {
    uint8_t columns, stride;
    size_t count;
    uint8_t *rows;
    struct character_table *next;
};

struct unicode_table {
    uint8_t modifier_bits[256], modifier_numbers[256];
    uint16_t max_modifier_bits;
    uint32_t locale_flags;
    struct character_table *characters;
    uint8_t *dead_keys, *ligatures;
    size_t dead_count, ligature_count;
    uint8_t ligature_length, ligature_stride;
};

static uint16_t get_u16(const uint8_t *data) {
    return (uint16_t) ((uint16_t) data[0] | (uint16_t) data[1] << 8);
}

static uint32_t get_u32(const uint8_t *data) {
    return (uint32_t) get_u16(data) | (uint32_t) get_u16(data + 2) << 16;
}

static size_t align_pointer(size_t size, unsigned int pointer_size) {
    return (size + pointer_size - 1) & ~(size_t) (pointer_size - 1);
}

static bool read_at(unicode_memory_reader reader, void *context, uint64_t address,
        size_t offset, void *buffer, size_t size) {
    return address != 0 && offset <= UINT64_MAX - address
            && reader(context, address + offset, buffer, size);
}

static bool read_pointer(unicode_memory_reader reader, void *context, uint64_t address,
        size_t offset, unsigned int pointer_size, uint64_t *pointer) {
    uint8_t bytes[8];
    if (!read_at(reader, context, address, offset, bytes, pointer_size)) {
        return false;
    }
    *pointer = get_u32(bytes);
    if (pointer_size == 8) {
        *pointer |= (uint64_t) get_u32(bytes + 4) << 32;
    }
    return true;
}

void unicode_table_destroy(struct unicode_table **table) {
    if (*table != NULL) {
        struct character_table *item = (*table)->characters;
        while (item != NULL) {
            struct character_table *next = item->next;
            free(item->rows);
            free(item);
            item = next;
        }
        free((*table)->dead_keys);
        free((*table)->ligatures);
        free(*table);
        *table = NULL;
    }
}

static bool load_modifiers(struct unicode_table *table, unicode_memory_reader reader,
        void *context, uint64_t modifiers, unsigned int pointer_size) {
    uint64_t bits;
    uint8_t bytes[2];
    if (!read_pointer(reader, context, modifiers, 0, pointer_size, &bits)
            || !read_at(reader, context, modifiers, pointer_size, bytes, 2)) {
        return false;
    }
    table->max_modifier_bits = get_u16(bytes);
    if (table->max_modifier_bits > 255
            || !read_at(reader, context, modifiers, pointer_size + 2,
                table->modifier_numbers, table->max_modifier_bits + 1)) {
        return false;
    }
    for (size_t i = 0; i < 256; i++) {
        if (!read_at(reader, context, bits, i * 2, bytes, 2)) {
            return false;
        }
        if (bytes[0] == 0) {
            return true;
        }
        table->modifier_bits[bytes[0]] = bytes[1];
    }
    return false;
}

static bool load_characters(struct unicode_table *table, unicode_memory_reader reader,
        void *context, uint64_t characters, unsigned int pointer_size) {
    size_t entry_size = align_pointer(pointer_size + 2, pointer_size);
    struct character_table **tail = &table->characters;
    for (size_t i = 0; i < MAX_CHARACTER_TABLES; i++) {
        uint64_t rows;
        uint8_t dimensions[2];
        if (!read_pointer(reader, context, characters, i * entry_size, pointer_size, &rows)
                || !read_at(reader, context, characters, i * entry_size + pointer_size, dimensions, 2)) {
            return false;
        }
        if (rows == 0) {
            return table->characters != NULL;
        }
        unsigned int columns = dimensions[0], stride = dimensions[1];
        if (columns == 0 || stride < 2 + columns * 2) {
            return false;
        }
        struct character_table *item = calloc(1, sizeof(*item));
        if (item == NULL) {
            return false;
        }
        *tail = item;
        tail = &item->next;
        item->columns = (uint8_t) columns;
        item->stride = (uint8_t) stride;
        bool terminated = false;
        for (size_t j = 0; j < MAX_CHARACTER_ROWS; j++) {
            uint8_t row[256];
            if (!read_at(reader, context, rows, j * stride, row, stride)) {
                return false;
            }
            if (row[0] == 0) {
                terminated = true;
                break;
            }
            uint8_t *copy = realloc(item->rows, (item->count + 1) * stride);
            if (copy == NULL) {
                return false;
            }
            item->rows = copy;
            memcpy(copy + item->count * stride, row, stride);
            item->count++;
        }
        if (!terminated) {
            return false;
        }
    }
    return false;
}

static bool load_sequence(uint8_t **data, size_t *count, unicode_memory_reader reader,
        void *context, uint64_t address, size_t stride, size_t terminator_size, size_t limit) {
    if (address == 0) {
        return true;
    }
    for (size_t i = 0; i < limit; i++) {
        uint8_t entry[256];
        if (!read_at(reader, context, address, i * stride, entry, stride)) {
            return false;
        }
        bool terminated = true;
        for (size_t j = 0; j < terminator_size; j++) {
            terminated &= entry[j] == 0;
        }
        if (terminated) {
            return true;
        }
        uint8_t *copy = realloc(*data, (i + 1) * stride);
        if (copy == NULL) {
            return false;
        }
        *data = copy;
        memcpy(copy + i * stride, entry, stride);
        *count = i + 1;
    }
    return false;
}

bool unicode_table_load(struct unicode_table **result, unicode_memory_reader reader,
        void *context, uint64_t descriptor, unsigned int pointer_size) {
    if (result == NULL || *result != NULL || reader == NULL
            || (pointer_size != 4 && pointer_size != 8)) {
        return false;
    }
    struct unicode_table *table = calloc(1, sizeof(*table));
    if (table == NULL) {
        return false;
    }
    uint64_t modifiers, characters, dead_keys, ligatures;
    // Seven leading pointers, one byte, two aligned pointers, then locale flags.
    size_t flags_offset = align_pointer(7 * pointer_size + 1, pointer_size) + 2 * pointer_size;
    size_t ligature_pointer_offset = align_pointer(flags_offset + 6, pointer_size);
    uint8_t flags[4], dimensions[2];
    bool valid = read_pointer(reader, context, descriptor, 0, pointer_size, &modifiers)
            && read_pointer(reader, context, descriptor, pointer_size, pointer_size, &characters)
            && read_pointer(reader, context, descriptor, 2 * pointer_size, pointer_size, &dead_keys)
            && read_at(reader, context, descriptor, flags_offset, flags, 4)
            && load_modifiers(table, reader, context, modifiers, pointer_size)
            && load_characters(table, reader, context, characters, pointer_size)
            && load_sequence(&table->dead_keys, &table->dead_count, reader, context, dead_keys, 8, 4, MAX_DEAD_KEYS);
    if (valid) {
        table->locale_flags = get_u32(flags);
        // Ligature fields were added in keyboard-table version 1. Do not read
        // past a version-0 descriptor just because the current SDK has them.
        if ((table->locale_flags >> 16) != 0) {
            valid = read_at(reader, context, descriptor, flags_offset + 4, dimensions, 2)
                    && read_pointer(reader, context, descriptor, ligature_pointer_offset,
                        pointer_size, &ligatures);
            if (valid) {
                table->ligature_length = dimensions[0];
                table->ligature_stride = dimensions[1];
            }
        }
        if (valid && table->ligature_length != 0) {
            valid = ligatures != 0 && table->ligature_stride >= 4 + 2 * table->ligature_length
                    && load_sequence(&table->ligatures, &table->ligature_count, reader,
                        context, ligatures, table->ligature_stride, 1, MAX_LIGATURES);
        }
    }
    if (!valid) {
        unicode_table_destroy(&table);
        return false;
    }
    *result = table;
    return true;
}

static bool compose(const struct unicode_table *table, uint16_t accent, uint16_t base,
        uint16_t *composed, bool *dead) {
    for (size_t i = 0; i < table->dead_count; i++) {
        const uint8_t *entry = table->dead_keys + i * 8;
        if (get_u16(entry) == base && get_u16(entry + 2) == accent) {
            *composed = get_u16(entry + 4);
            *dead = (get_u16(entry + 6) & 1) != 0;
            return true;
        }
    }
    return false;
}

static size_t lookup(const struct unicode_table *table, uint16_t keycode,
        const uint8_t key_state[256], uint16_t output[256], bool *is_dead) {
    unsigned int bits = 0;
    for (size_t i = 0; i < 256; i++) {
        if ((key_state[i] & 0x80) != 0) {
            bits |= table->modifier_bits[i];
        }
    }
    bool caps = (key_state[VK_CAPITAL_CODE] & 1) != 0;
    bool altgr = (table->locale_flags & KLLF_ALTGR) != 0
            && (key_state[VK_CONTROL_CODE] & 0x80) != 0
            && (key_state[VK_MENU_CODE] & 0x80) != 0;
    unsigned int caps_attribute = altgr ? CAPLOKALTGR : CAPLOK;
    for (const struct character_table *item = table->characters; item != NULL; item = item->next) {
        for (size_t row_index = 0; row_index < item->count; row_index++) {
            const uint8_t *row = item->rows + row_index * item->stride;
            if (row[0] != keycode || row[0] == 0xFF) {
                continue;
            }
            unsigned int key_bits = bits;
            if (caps && (row[1] & caps_attribute) != 0) {
                key_bits ^= table->modifier_bits[VK_SHIFT_CODE];
            }
            if (key_bits > table->max_modifier_bits) {
                break;
            }
            unsigned int column = table->modifier_numbers[key_bits];
            if (column == SHFT_INVALID || column >= item->columns) {
                break;
            }
            size_t selected = row_index;
            if ((row[1] & SGCAPS) != 0 && !caps) {
                selected++;
                if (selected >= item->count) {
                    return 0;
                }
            }
            row = item->rows + selected * item->stride;
            uint16_t value = get_u16(row + 2 + column * 2);
            if (value == WCH_NONE) {
                return 0;
            }
            if (value == WCH_DEAD) {
                if (++selected >= item->count) {
                    return 0;
                }
                row = item->rows + selected * item->stride;
                if (row[0] != 0xFF) {
                    return 0;
                }
                value = get_u16(row + 2 + column * 2);
                if (value >= WCH_NONE && value <= WCH_LGTR) {
                    return 0;
                }
                *is_dead = true;
            } else if (value == WCH_LGTR) {
                for (size_t i = 0; i < table->ligature_count; i++) {
                    const uint8_t *entry = table->ligatures + i * table->ligature_stride;
                    if (entry[0] == keycode && get_u16(entry + 2) == column) {
                        size_t count = 0;
                        while (count < table->ligature_length) {
                            uint16_t unit = get_u16(entry + 4 + count * 2);
                            if (unit == WCH_NONE) {
                                break;
                            }
                            output[count++] = unit;
                        }
                        return count;
                    }
                }
                return 0;
            }
            output[0] = value;
            return 1;
        }
    }
    return 0;
}

size_t unicode_table_translate(const struct unicode_table *table, struct unicode_state *state,
        uint16_t keycode, const uint8_t key_state[256], uint16_t *buffer, size_t capacity) {
    if (table == NULL || state == NULL || key_state == NULL || buffer == NULL
            || capacity == 0 || keycode > 255) {
        return 0;
    }
    uint16_t characters[256], output[256];
    bool is_dead = false;
    size_t length = lookup(table, keycode, key_state, characters, &is_dead);
    if (length == 0) {
        return 0;
    }
    uint16_t pending = state->dead_char;
    size_t count = 0;
    if (is_dead && pending == 0) {
        state->dead_char = characters[0];
        return 0;
    }
    for (size_t i = 0; i < length; i++) {
        uint16_t composed;
        bool dead;
        if (pending != 0) {
            if (compose(table, pending, characters[i], &composed, &dead)) {
                pending = dead ? composed : 0;
                if (!dead) {
                    output[count++] = composed;
                }
                continue;
            }
            output[count++] = pending;
        }
        pending = is_dead ? characters[i] : 0;
        if (!is_dead) {
            output[count++] = characters[i];
        }
    }
    if (count > capacity) {
        return 0;
    }
    memcpy(buffer, output, count * sizeof(*buffer));
    state->dead_char = pending;
    return count;
}
