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

#include <string.h>
#include "windows/unicode_table.h"
#include "unicode_fixture.h"
#include "test_assert.h"

static void test_layout(unsigned int width, uint64_t base) {
    struct unicode_fixture f;
    unicode_fixture_init(&f, width, base);
    struct unicode_table *table = NULL;
    TEST_ASSERT(unicode_table_load(&table, unicode_fixture_read, &f, base + 0x200, width));
    // Imported tables survive overwriting/unloading the source image.
    memset(f.bytes, 0xCC, sizeof(f.bytes));
    struct unicode_state state = {0};
    uint8_t keys[256] = {0};
    uint16_t output[5] = {0xAAAA, 0xBBBB, 0xCCCC, 0xDDDD, 0xEEEE};
    TEST_ASSERT(unicode_table_translate(table, &state, 'A', keys, output, 1) == 1 && output[0] == 'a');
    TEST_ASSERT(output[1] == 0xBBBB);
    keys[0x10] = 0x80;
    TEST_ASSERT(unicode_table_translate(table, &state, 'A', keys, output, 1) == 1 && output[0] == 'A');
    keys[0x14] = 1;
    TEST_ASSERT(unicode_table_translate(table, &state, 'A', keys, output, 1) == 1 && output[0] == 'a');
    keys[0x10] = 0;
    TEST_ASSERT(unicode_table_translate(table, &state, 'A', keys, output, 1) == 1 && output[0] == 'A');
    keys[0x14] = 0;
    keys[0x11] = keys[0x12] = 0x80;
    TEST_ASSERT(unicode_table_translate(table, &state, 'A', keys, output, 1) == 1 && output[0] == 0xE4);
    keys[0x14] = 1;
    TEST_ASSERT(unicode_table_translate(table, &state, 'A', keys, output, 1) == 1 && output[0] == 0xC4);
    memset(keys, 0, sizeof(keys)); keys[0x11] = 0x80;
    TEST_ASSERT(unicode_table_translate(table, &state, 'A', keys, output, 1) == 1 && output[0] == 1);
    TEST_ASSERT(unicode_table_translate(table, &state, 'B', keys, output, 1) == 0);
    memset(keys, 0, sizeof(keys)); keys[0x12] = 0x80;
    TEST_ASSERT(unicode_table_translate(table, &state, 'A', keys, output, 1) == 0);
    memset(keys, 0, sizeof(keys)); keys[0x14] = 1;
    TEST_ASSERT(unicode_table_translate(table, &state, 'Z', keys, output, 1) == 0); // Caps index beyond one column.
    TEST_ASSERT(unicode_table_translate(table, &state, 'Q', keys, output, 1) == 1 && output[0] == 0xC4);
    keys[0x14] = 0;
    TEST_ASSERT(unicode_table_translate(table, &state, 'Q', keys, output, 1) == 1 && output[0] == 'q');
    TEST_ASSERT(unicode_table_translate(table, &state, 'Z', keys, output, 1) == 1 && output[0] == 'z');

    output[0] = 0xAAAA;
    TEST_ASSERT(unicode_table_translate(table, &state, 0xDE, keys, output, 2) == 0);
    TEST_ASSERT(state.dead_char == 0xB4 && output[0] == 0xAAAA);
    TEST_ASSERT(unicode_table_translate(table, &state, 0x10, keys, output, 2) == 0 && state.dead_char == 0xB4);
    TEST_ASSERT(unicode_table_translate(table, &state, 'E', keys, output, 1) == 1 && output[0] == 0xE9);
    TEST_ASSERT(state.dead_char == 0);
    unicode_table_translate(table, &state, 0xDE, keys, output, 2);
    output[0] = 0xAAAA;
    TEST_ASSERT(unicode_table_translate(table, &state, 'X', keys, output, 1) == 0);
    TEST_ASSERT(output[0] == 0xAAAA && state.dead_char == 0xB4);
    TEST_ASSERT(unicode_table_translate(table, &state, 'X', keys, output, 2) == 2);
    TEST_ASSERT(output[0] == 0xB4 && output[1] == 'x' && state.dead_char == 0);
    unicode_table_translate(table, &state, 0xDE, keys, output, 2);
    TEST_ASSERT(unicode_table_translate(table, &state, 0xDE, keys, output, 2) == 1 && output[0] == 0xB4);
    TEST_ASSERT(state.dead_char == 0);
    unicode_table_translate(table, &state, 0xDE, keys, output, 2);
    TEST_ASSERT(unicode_table_translate(table, &state, 0xC0, keys, output, 2) == 0 && state.dead_char == '^');
    TEST_ASSERT(unicode_table_translate(table, &state, 'A', keys, output, 1) == 1 && output[0] == 0xE2);
    unicode_table_translate(table, &state, 0xDE, keys, output, 2);
    TEST_ASSERT(unicode_table_translate(table, &state, ' ', keys, output, 1) == 1 && output[0] == 0xB4);

    output[0] = 0xAAAA;
    TEST_ASSERT(unicode_table_translate(table, &state, 'L', keys, output, 2) == 0 && output[0] == 0xAAAA);
    TEST_ASSERT(unicode_table_translate(table, &state, 'L', keys, output, 3) == 3);
    TEST_ASSERT(output[0] == 0xD83D && output[1] == 0xDE00 && output[2] == 'x');
    TEST_ASSERT(unicode_table_translate(table, &state, 'A', keys, NULL, 1) == 0);
    TEST_ASSERT(unicode_table_translate(table, &state, 'A', keys, output, 0) == 0);
    TEST_ASSERT(unicode_table_translate(table, &state, 256, keys, output, 1) == 0);
    unicode_table_destroy(&table);
    TEST_ASSERT(table == NULL);
    unicode_table_destroy(&table);
}

static void test_invalid_layouts(void) {
    struct unicode_fixture f;
    struct unicode_table *table = NULL;
    uint8_t padding_keys[256] = {0};
    uint16_t padding_output = 0xAAAA;
    struct unicode_state padding_state = {0};
    unicode_fixture_init(&f, 8, UINT64_C(0x20000000));
    f.bytes[0x30A] = 6; // Modifier points at padding, not a seventh character column.
    TEST_ASSERT(unicode_table_load(&table, unicode_fixture_read, &f, f.base + 0x200, 8));
    TEST_ASSERT(unicode_table_translate(table, &padding_state, 'A', padding_keys, &padding_output, 1) == 0);
    TEST_ASSERT(padding_output == 0xAAAA);
    unicode_table_destroy(&table);
    unicode_fixture_init(&f, 8, UINT64_C(0x20000000));
    TEST_ASSERT(!unicode_table_load(&table, unicode_fixture_read, &f, f.base + 0x200, 4));
    TEST_ASSERT(!unicode_table_load(&table, unicode_fixture_read, &f, f.base + 0x200, 6));
    f.bytes[0x409] = 3; // cbSize smaller than the declared columns.
    TEST_ASSERT(!unicode_table_load(&table, unicode_fixture_read, &f, f.base + 0x200, 8));
    unicode_fixture_init(&f, 4, UINT64_C(0x10000000));
    TEST_ASSERT(!unicode_table_load(&table, unicode_fixture_read, &f, f.base + 0x200, 8));
    unicode_fixture_pointer(&f, 0x200, UINT64_MAX - 1);
    TEST_ASSERT(!unicode_table_load(&table, unicode_fixture_read, &f, f.base + 0x200, 4));
    unicode_fixture_init(&f, 8, UINT64_C(0x100000000));
    f.size = 0x350; // Truncated image, before the character tables.
    TEST_ASSERT(!unicode_table_load(&table, unicode_fixture_read, &f, f.base + 0x200, 8));
    unicode_fixture_init(&f, 8, UINT64_C(0x100000000));
    unicode_fixture_pointer(&f, 0x210, 0); // No dead-key table is legal.
    TEST_ASSERT(unicode_table_load(&table, unicode_fixture_read, &f, f.base + 0x200, 8));
    uint8_t keys[256] = {0}; uint16_t output[2]; struct unicode_state state = {0};
    unicode_table_translate(table, &state, 0xDE, keys, output, 2);
    TEST_ASSERT(unicode_table_translate(table, &state, 'E', keys, output, 2) == 2);
    TEST_ASSERT(output[0] == 0xB4 && output[1] == 'e');
    unicode_table_destroy(&table);
    unicode_fixture_init(&f, 8, UINT64_C(0x100000000));
    // Missing dead-character continuation must not read past the copied rows.
    f.bytes[0x500 + 3 * 16] = 'V';
    TEST_ASSERT(unicode_table_load(&table, unicode_fixture_read, &f, f.base + 0x200, 8));
    TEST_ASSERT(unicode_table_translate(table, &state, 0xDE, keys, output, 2) == 0);
    unicode_table_destroy(&table);

    unicode_fixture_init(&f, 8, UINT64_C(0x100000000));
    f.bytes[0x308] = 0xFF; f.bytes[0x309] = 0xFF; // Modifier map cannot exceed 256 entries.
    TEST_ASSERT(!unicode_table_load(&table, unicode_fixture_read, &f, f.base + 0x200, 8));
    unicode_fixture_init(&f, 8, UINT64_C(0x100000000));
    f.bytes[0x250 + 2] = 0; // Version 0: optional ligature fields must not be read.
    unicode_fixture_pointer(&f, 0x258, UINT64_MAX);
    TEST_ASSERT(unicode_table_load(&table, unicode_fixture_read, &f, f.base + 0x200, 8));
    TEST_ASSERT(unicode_table_translate(table, &state, 'L', keys, output, 2) == 0);
    unicode_table_destroy(&table);
    unicode_fixture_init(&f, 8, UINT64_C(0x100000000));
    // Unterminated character rows fail safely and release partially copied data.
    memset(f.bytes + 0x500, 0xFF, sizeof(f.bytes) - 0x500);
    TEST_ASSERT(!unicode_table_load(&table, unicode_fixture_read, &f, f.base + 0x200, 8));
    TEST_ASSERT(table == NULL);
}

int main(void) {
    test_layout(4, UINT64_C(0x10000000));
    test_layout(8, UINT64_C(0x100000000));
    test_layout(8, UINT64_C(0x20000000)); // WOW64: low addresses, 8-byte fields.
    test_invalid_layouts();
    return EXIT_SUCCESS;
}
