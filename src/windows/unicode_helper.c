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

/**********************************************************************
 *
 * The original code was substantially modified to provide the following:
 *   1) More dynamic code structure.
 *   2) Support for compilers that do not implement _ptr64 (GCC / LLVM).
 *   3) Support for Wow64 at runtime via 32-bit binary.
 *   4) Support for contextual language switching.
 *
 * I have contacted Marc-André Moreau who has granted permission for
 * his original source code to be used under the Public Domain.  Although
 * the libUIOHook library as a whole is currently covered under the LGPLv3,
 * please feel free to use and learn from the source code contained in the
 * following functions under the terms of the Public Domain.
 *
 * For further reading and the original code, please visit:
 *   http://legacy.docdroppers.org/wiki/index.php?title=Writing_Keyloggers
 *   https://web.archive.org/web/20140210000534/http://techmantras.com/content/writing-keyloggers-full-length-tutorial
 *
 ***********************************************************************/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uiohook.h>

#include "input_helper.h"
#include "logger.h"
#include "unicode_helper.h"
#include "unicode_table.h"

struct keyboard_locale {
    HKL id;
    struct unicode_table *table;
    struct keyboard_locale *next;
};

struct layout_image { uint64_t base; size_t size; };
static struct keyboard_locale *locale_first, *locale_current;
static HKL current_id;
static struct unicode_state translation_state;

// Only read ranges belonging to the loaded image. Pointer width is decoded
// before narrowing to uintptr_t, so WOW64 pointers cannot be silently truncated.
static bool read_layout_image(void *context, uint64_t address, void *buffer, size_t size) {
    const struct layout_image *image = context;
    if (address < image->base || address > UINTPTR_MAX) {
        return false;
    }
    uint64_t offset = address - image->base;
    if (offset > image->size || size > image->size - (size_t) offset) {
        return false;
    }
    SIZE_T copied = 0;
    return ReadProcessMemory(GetCurrentProcess(), (const void *) (uintptr_t) address,
            buffer, size, &copied) && copied == size;
}

static bool get_layout_file(char filename[MAX_PATH]) {
    char name[KL_NAMELENGTH];
    if (!GetKeyboardLayoutNameA(name)) {
        return false;
    }
    char path[128];
    int length = snprintf(path, sizeof(path),
            "SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts\\%s", name);
    if (length < 0 || (size_t) length >= sizeof(path)) {
        return false;
    }
    HKEY key;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, path, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return false;
    }
    DWORD type = 0, size = MAX_PATH;
    LONG result = RegQueryValueExA(key, "Layout File", NULL, &type, (BYTE *) filename, &size);
    RegCloseKey(key);
    return result == ERROR_SUCCESS && type == REG_SZ && size > 0 && size <= MAX_PATH
            && memchr(filename, 0, size) != NULL;
}

static struct unicode_table *load_layout(void) {
    char filename[MAX_PATH], directory[MAX_PATH], path[MAX_PATH];
    if (!get_layout_file(filename)) {
        return NULL;
    }
    UINT length = GetSystemDirectoryA(directory, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return NULL;
    }
    int path_length = snprintf(path, sizeof(path), "%s\\%s", directory, filename);
    if (path_length < 0 || (size_t) path_length >= sizeof(path)) {
        return NULL;
    }
    HMODULE library = LoadLibraryA(path);
    if (library == NULL) {
        return NULL;
    }
    struct unicode_table *table = NULL;
    // KbdLayerDescriptor has no arguments and returns a native process pointer.
    typedef void * (__cdecl *keyboard_descriptor)(void);
    keyboard_descriptor descriptor = (keyboard_descriptor) GetProcAddress(library, "KbdLayerDescriptor");
    if (descriptor != NULL) {
        IMAGE_DOS_HEADER dos;
        IMAGE_NT_HEADERS nt;
        SIZE_T copied = 0;
        uintptr_t base = (uintptr_t) library;
        if (ReadProcessMemory(GetCurrentProcess(), library, &dos, sizeof(dos), &copied)
                && copied == sizeof(dos) && dos.e_magic == IMAGE_DOS_SIGNATURE
                && dos.e_lfanew > 0 && (uintptr_t) dos.e_lfanew <= UINTPTR_MAX - base
                && ReadProcessMemory(GetCurrentProcess(), (void *) (base + dos.e_lfanew),
                    &nt, sizeof(nt), &copied)
                && copied == sizeof(nt) && nt.Signature == IMAGE_NT_SIGNATURE) {
            struct layout_image image = { .base = base, .size = nt.OptionalHeader.SizeOfImage };
            uint64_t root = (uintptr_t) descriptor();
            unsigned int pointer_size = sizeof(void *);
            // Native x64 uses 8-byte pointers. An x86 DLL may use either
            // native x86 or BUILD_WOW6432 tables; validate the actual bytes.
            if (!unicode_table_load(&table, read_layout_image, &image, root, pointer_size)
                    && pointer_size == 4) {
                unicode_table_load(&table, read_layout_image, &image, root, 8);
            }
        }
    }
    // The decoder owns all copied data; no DLL pointers survive import.
    FreeLibrary(library);
    return table;
}

static struct keyboard_locale *find_locale(HKL id) {
    for (struct keyboard_locale *item = locale_first; item != NULL; item = item->next) {
        if (item->id == id) {
            return item;
        }
    }
    return NULL;
}

static void refresh_locale_list(void) {
    int capacity = GetKeyboardLayoutList(0, NULL);
    if (capacity <= 0 || (size_t) capacity > SIZE_MAX / sizeof(HKL)) {
        return;
    }
    HKL *layouts = malloc((size_t) capacity * sizeof(*layouts));
    if (layouts == NULL) {
        return;
    }
    int count = GetKeyboardLayoutList(capacity, layouts);
    if (count <= 0 || count > capacity) {
        free(layouts);
        return;
    }
    struct keyboard_locale **link = &locale_first;
    while (*link != NULL) {
        struct keyboard_locale *item = *link;
        bool present = false;
        for (int i = 0; i < count; i++) {
            present |= layouts[i] == item->id;
        }
        if (present) {
            link = &item->next;
        } else {
            *link = item->next;
            if (locale_current == item) {
                locale_current = NULL;
                translation_state.dead_char = 0;
            }
            unicode_table_destroy(&item->table);
            free(item);
        }
    }

    HKL original = GetKeyboardLayout(0);
    for (int i = 0; i < count; i++) {
        if (layouts[i] == NULL || find_locale(layouts[i]) != NULL) {
            continue;
        }
        if (ActivateKeyboardLayout(layouts[i], 0) == NULL) {
            continue;
        }
        struct unicode_table *table = load_layout();
        if (table == NULL) {
            logger(LOG_LEVEL_WARN, "%s [%u]: Unable to decode keyboard layout %#p.\n",
                    __FUNCTION__, __LINE__, layouts[i]);
            continue;
        }
        struct keyboard_locale *item = calloc(1, sizeof(*item));
        if (item == NULL) {
            unicode_table_destroy(&table);
            continue;
        }
        item->id = layouts[i];
        item->table = table;
        *link = item;
        link = &item->next;
    }
    if (original != NULL) {
        ActivateKeyboardLayout(original, 0);
    }
    free(layouts);
}

void unload_unicode_helper(void) {
    while (locale_first != NULL) {
        struct keyboard_locale *item = locale_first;
        locale_first = item->next;
        unicode_table_destroy(&item->table);
        free(item);
    }
    locale_current = NULL;
    current_id = NULL;
    translation_state.dead_char = 0;
}

void load_unicode_helper(void) {
    unload_unicode_helper();
    refresh_locale_list();
}

SIZE_T vkcode_to_unicode(DWORD keycode, PWCHAR buffer, SIZE_T capacity) {
    if (keycode > 255 || buffer == NULL || capacity == 0) {
        return 0;
    }
    DWORD thread = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
    HKL id = GetKeyboardLayout(thread);
    if (id == NULL) {
        id = GetKeyboardLayout(0);
    }
    if (id != current_id) {
        current_id = id;
        translation_state.dead_char = 0;
        locale_current = find_locale(id);
    }
    if (locale_current == NULL) {
        refresh_locale_list();
        locale_current = find_locale(id);
    }
    if (locale_current == NULL) {
        return 0;
    }

    BYTE keys[256] = { 0 };
    GetKeyboardState(keys);
    // The hook runs before async key state updates. Use its tracked modifiers
    // for Shift/Ctrl/Alt, retaining the snapshot for layout-specific modifiers.
    uint16_t mask = get_modifiers();
    keys[VK_LSHIFT] = (mask & (MASK_SHIFT_L)) != 0 ? 0x80 : 0;
    keys[VK_RSHIFT] = (mask & (MASK_SHIFT_R)) != 0 ? 0x80 : 0;
    keys[VK_LCONTROL] = (mask & (MASK_CTRL_L)) != 0 ? 0x80 : 0;
    keys[VK_RCONTROL] = (mask & (MASK_CTRL_R)) != 0 ? 0x80 : 0;
    keys[VK_LMENU] = (mask & (MASK_ALT_L)) != 0 ? 0x80 : 0;
    keys[VK_RMENU] = (mask & (MASK_ALT_R)) != 0 ? 0x80 : 0;
    keys[VK_SHIFT] = keys[VK_LSHIFT] | keys[VK_RSHIFT];
    keys[VK_CONTROL] = keys[VK_LCONTROL] | keys[VK_RCONTROL];
    keys[VK_MENU] = keys[VK_LMENU] | keys[VK_RMENU];
    keys[VK_CAPITAL] = (BYTE) (GetKeyState(VK_CAPITAL) & 1);
    keys[keycode] |= 0x80;

    // Windows WCHAR is a 16-bit UTF-16 unit; do not alias it through uint16_t*.
    uint16_t translated[256];
    size_t available = capacity < 256 ? capacity : 256;
    size_t count = unicode_table_translate(locale_current->table, &translation_state,
            (uint16_t) keycode, keys, translated, available);
    for (size_t i = 0; i < count; i++) {
        buffer[i] = (WCHAR) translated[i];
    }
    return count;
}
