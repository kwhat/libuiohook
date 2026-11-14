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
#include "unicode_fixture.h"

static void put16(uint8_t *p, uint16_t value) { p[0] = (uint8_t) value; p[1] = (uint8_t) (value >> 8); }
static void put32(uint8_t *p, uint32_t value) { put16(p, (uint16_t) value); put16(p + 2, (uint16_t) (value >> 16)); }

void unicode_fixture_pointer(struct unicode_fixture *f, size_t offset, uint64_t value) {
    put32(f->bytes + offset, (uint32_t) value);
    if (f->pointer_size == 8) put32(f->bytes + offset + 4, (uint32_t) (value >> 32));
}

bool unicode_fixture_read(void *context, uint64_t address, void *buffer, size_t size) {
    struct unicode_fixture *f = context;
    if (address < f->base || address - f->base > f->size
            || size > f->size - (size_t) (address - f->base)) return false;
    memcpy(buffer, f->bytes + (size_t) (address - f->base), size);
    return true;
}

void unicode_fixture_init(struct unicode_fixture *f, unsigned int width, uint64_t base) {
    memset(f, 0, sizeof(*f));
    f->base = base;
    f->size = sizeof(f->bytes);
    f->pointer_size = width;
    // Independent constants from the ABI: x86 offsets 40/44/48, x64 and
    // BUILD_WOW6432 offsets 80/84/88. Descriptor starts at image offset 0x200.
    unicode_fixture_pointer(f, 0x200, base + 0x300);
    unicode_fixture_pointer(f, 0x200 + width, base + 0x400);
    unicode_fixture_pointer(f, 0x200 + 2 * width, base + 0xA00);
    size_t flags = width == 4 ? 0x228 : 0x250;
    put32(f->bytes + flags, 0x00010001); // Version 1, KLLF_ALTGR
    f->bytes[flags + 4] = 4;
    f->bytes[flags + 5] = 12; // BYTE + pad + WORD + four UTF-16 units
    unicode_fixture_pointer(f, width == 4 ? 0x230 : 0x258, base + 0xB00);

    unicode_fixture_pointer(f, 0x300, base + 0x340);
    put16(f->bytes + 0x300 + width, 7);
    const uint8_t modifications[] = {0, 1, 4, 5, 15, 15, 2, 3};
    memcpy(f->bytes + 0x302 + width, modifications, sizeof(modifications));
    const uint8_t bits[] = {0x10, 1, 0x11, 2, 0x12, 4, 0, 0};
    memcpy(f->bytes + 0x340, bits, sizeof(bits));

    unicode_fixture_pointer(f, 0x400, base + 0x500);
    f->bytes[0x400 + width] = 6;
    f->bytes[0x401 + width] = 16; // Deliberately padded row: 2 + 6*2 < 16.
    size_t second = width == 4 ? 0x408 : 0x410;
    unicode_fixture_pointer(f, second, base + 0x900);
    f->bytes[second + width] = 1;
    f->bytes[second + width + 1] = 4;
    const uint16_t rows[][8] = {
        {'A', 0x85, 'a', 'A', 0xE4, 0xC4, 1, 1},
        {'B', 1, 'b', 'B', 'b', 'B', 0xF000, 0xF000},
        {0xDE, 0, 0xF001, 0xF001, 0xF001, 0xF001, 0xF001, 0xF001},
        {0xFF, 0, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4, 0xB4},
        {0xC0, 0, 0xF001, 0xF001, 0xF001, 0xF001, 0xF001, 0xF001},
        {0xFF, 0, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60},
        {'E', 1, 'e', 'E', 'e', 'E', 5, 5},
        {'X', 1, 'x', 'X', 'x', 'X', 24, 24},
        {' ', 0, ' ', ' ', ' ', ' ', ' ', ' '},
        {'L', 0, 0xF002, 0xF002, 0xF002, 0xF002, 0xF002, 0xF002},
        {'Q', 2, 0xC4, 0xE4, 'Q', 'q', 'Q', 'q'},
        {0xFF, 0, 'q', 'Q', 'q', 'Q', 'q', 'Q'}
    };
    for (size_t i = 0; i < sizeof(rows)/sizeof(rows[0]); i++) {
        uint8_t *row = f->bytes + 0x500 + i * 16;
        row[0] = (uint8_t) rows[i][0]; row[1] = (uint8_t) rows[i][1];
        for (size_t j = 0; j < 6; j++) put16(row + 2 + j * 2, rows[i][j + 2]);
        row[14] = 0xA5; row[15] = 0x5A;
    }
    f->bytes[0x900] = 'Z'; f->bytes[0x901] = 1; put16(f->bytes + 0x902, 'z');
    const uint16_t dead[][4] = {
        {'e', 0xB4, 0xE9, 0}, {'E', 0xB4, 0xC9, 0}, {' ', 0xB4, 0xB4, 0},
        {0xB4, 0xB4, 0xB4, 0}, {0x60, 0xB4, '^', 1}, {'a', '^', 0xE2, 0}
    };
    for (size_t i = 0; i < sizeof(dead)/sizeof(dead[0]); i++)
        for (size_t j = 0; j < 4; j++) put16(f->bytes + 0xA00 + i * 8 + j * 2, dead[i][j]);
    f->bytes[0xB00] = 'L';
    put16(f->bytes + 0xB04, 0xD83D); put16(f->bytes + 0xB06, 0xDE00);
    put16(f->bytes + 0xB08, 'x'); put16(f->bytes + 0xB0A, 0xF000);
}
