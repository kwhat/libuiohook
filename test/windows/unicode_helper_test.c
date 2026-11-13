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

#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <uiohook.h>
#include "windows/unicode_helper.h"
#include "unicode_fixture.h"
#include "test_assert.h"

static struct unicode_fixture images[3];
static HKL layouts[3], active, focused;
static int layout_count = 2, loads, frees, opened, closed;
static int loaded_index;
static bool query_failure, invalid_image;
static uint16_t modifiers;
static SHORT caps;

uint16_t get_modifiers(void) { return modifiers; }

static int WINAPI mock_GetKeyboardLayoutList(int capacity, HKL *list) {
    if (capacity == 0) return layout_count;
    int count = capacity < layout_count ? capacity : layout_count;
    memcpy(list, layouts, count * sizeof(*list));
    return count;
}
static HKL WINAPI mock_GetKeyboardLayout(DWORD thread) { return thread == 42 ? focused : active; }
static HKL WINAPI mock_ActivateKeyboardLayout(HKL layout, UINT flags) {
    (void) flags;
    HKL previous = active;
    active = layout;
    return previous;
}
static BOOL WINAPI mock_GetKeyboardLayoutNameA(LPSTR name) {
    snprintf(name, KL_NAMELENGTH, "%08x", (unsigned int) (uintptr_t) active);
    return TRUE;
}
static LSTATUS WINAPI mock_RegOpenKeyExA(HKEY root, LPCSTR path, DWORD options, REGSAM access, PHKEY key) {
    (void) options; (void) access;
    TEST_ASSERT(root == HKEY_LOCAL_MACHINE);
    TEST_ASSERT(strstr(path, "SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts\\") == path);
    *key = (HKEY) (uintptr_t) 1;
    opened++;
    return ERROR_SUCCESS;
}
static LSTATUS WINAPI mock_RegQueryValueExA(HKEY key, LPCSTR name, LPDWORD reserved,
        LPDWORD type, LPBYTE value, LPDWORD size) {
    (void) key; (void) reserved;
    TEST_ASSERT(strcmp(name, "Layout File") == 0 && *size >= 16);
    if (query_failure) return ERROR_FILE_NOT_FOUND;
    strcpy((char *) value, "fixture.dll");
    *type = REG_SZ; *size = 12;
    return ERROR_SUCCESS;
}
static LSTATUS WINAPI mock_RegCloseKey(HKEY key) { (void) key; closed++; return ERROR_SUCCESS; }
static UINT WINAPI mock_GetSystemDirectoryA(LPSTR path, UINT capacity) {
    TEST_ASSERT(capacity == MAX_PATH);
    strcpy(path, "C:\\Windows\\System32");
    return (UINT) strlen(path);
}
static HMODULE WINAPI mock_LoadLibraryA(LPCSTR path) {
    TEST_ASSERT(strcmp(path, "C:\\Windows\\System32\\fixture.dll") == 0);
    loaded_index = (int) (uintptr_t) active - 1;
    TEST_ASSERT(loaded_index >= 0 && loaded_index < 3);
    loads++;
    return (HMODULE) images[loaded_index].bytes;
}
static void * __cdecl descriptor(void) { return images[loaded_index].bytes + 0x200; }
static FARPROC WINAPI mock_GetProcAddress(HMODULE module, LPCSTR name) {
    TEST_ASSERT(module == (HMODULE) images[loaded_index].bytes && strcmp(name, "KbdLayerDescriptor") == 0);
    return (FARPROC) descriptor;
}
static BOOL WINAPI mock_FreeLibrary(HMODULE module) { (void) module; frees++; return TRUE; }
static BOOL WINAPI mock_ReadProcessMemory(HANDLE process, LPCVOID address, LPVOID buffer, SIZE_T size, SIZE_T *copied) {
    (void) process;
    if (invalid_image) return FALSE;
    for (int i = 0; i < 3; i++) {
        if (unicode_fixture_read(&images[i], (uintptr_t) address, buffer, size)) {
            *copied = size;
            return TRUE;
        }
    }
    return FALSE;
}
static HWND WINAPI mock_GetForegroundWindow(void) { return (HWND) (uintptr_t) 1; }
static DWORD WINAPI mock_GetWindowThreadProcessId(HWND window, LPDWORD process) {
    (void) window; (void) process;
    return 42;
}
static BOOL WINAPI mock_GetKeyboardState(PBYTE keys) { memset(keys, 0, 256); return TRUE; }
static SHORT WINAPI mock_GetKeyState(int key) { return key == VK_CAPITAL ? caps : 0; }

// SDK import pointers provide the link-time mock boundary (Windows x64).
#define IMPORT(result, name, args) result (WINAPI *__imp_##name) args = mock_##name
IMPORT(int, GetKeyboardLayoutList, (int, HKL *));
IMPORT(HKL, GetKeyboardLayout, (DWORD));
IMPORT(HKL, ActivateKeyboardLayout, (HKL, UINT));
IMPORT(BOOL, GetKeyboardLayoutNameA, (LPSTR));
IMPORT(LSTATUS, RegOpenKeyExA, (HKEY, LPCSTR, DWORD, REGSAM, PHKEY));
IMPORT(LSTATUS, RegQueryValueExA, (HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD));
IMPORT(LSTATUS, RegCloseKey, (HKEY));
IMPORT(UINT, GetSystemDirectoryA, (LPSTR, UINT));
IMPORT(HMODULE, LoadLibraryA, (LPCSTR));
IMPORT(FARPROC, GetProcAddress, (HMODULE, LPCSTR));
IMPORT(BOOL, FreeLibrary, (HMODULE));
IMPORT(BOOL, ReadProcessMemory, (HANDLE, LPCVOID, LPVOID, SIZE_T, SIZE_T *));
IMPORT(HWND, GetForegroundWindow, (void));
IMPORT(DWORD, GetWindowThreadProcessId, (HWND, LPDWORD));
IMPORT(BOOL, GetKeyboardState, (PBYTE));
IMPORT(SHORT, GetKeyState, (int));

static void initialize_images(void) {
    for (int i = 0; i < 3; i++) {
        unicode_fixture_init(&images[i], 8, (uintptr_t) images[i].bytes);
        IMAGE_DOS_HEADER dos = {0}; IMAGE_NT_HEADERS nt = {0};
        dos.e_magic = IMAGE_DOS_SIGNATURE; dos.e_lfanew = 0x80;
        nt.Signature = IMAGE_NT_SIGNATURE; nt.OptionalHeader.SizeOfImage = sizeof(images[i].bytes);
        memcpy(images[i].bytes, &dos, sizeof(dos));
        memcpy(images[i].bytes + 0x80, &nt, sizeof(nt));
        images[i].bytes[0x502] = (BYTE) ('a' + i);
    }
}

int main(void) {
    initialize_images();
    layouts[0] = active = focused = (HKL) (uintptr_t) 1;
    layouts[1] = (HKL) (uintptr_t) 2;
    load_unicode_helper();
    TEST_ASSERT(active == (HKL) (uintptr_t) 1 && loads == 2 && frees == 2);
    WCHAR output[4] = {0xAAAA, 0xBBBB, 0xCCCC, 0xDDDD};
    TEST_ASSERT(vkcode_to_unicode('A', output, 1) == 1 && output[0] == 'a' && output[1] == 0xBBBB);
    modifiers = MASK_SHIFT_L;
    TEST_ASSERT(vkcode_to_unicode('A', output, 1) == 1 && output[0] == 'A');
    modifiers = MASK_CTRL_L | MASK_ALT_R;
    TEST_ASSERT(vkcode_to_unicode('A', output, 1) == 1 && output[0] == 0xE4);
    modifiers = 0;
    vkcode_to_unicode(0xDE, output, 4);
    focused = (HKL) (uintptr_t) 2;
    TEST_ASSERT(vkcode_to_unicode('E', output, 4) == 1 && output[0] == 'e'); // Layout change resets dead key.
    focused = (HKL) (uintptr_t) 1;
    // Data is owned by the decoder even after the DLL has been released.
    memset(images[0].bytes + 0x500, 0xCC, 16);
    TEST_ASSERT(vkcode_to_unicode('A', output, 1) == 1 && output[0] == 'a');

    // Remove the cache head, preserve the second node, and add a new layout.
    layouts[0] = (HKL) (uintptr_t) 2; layouts[1] = focused = (HKL) (uintptr_t) 3;
    TEST_ASSERT(vkcode_to_unicode('A', output, 1) == 1 && output[0] == 'c');
    focused = (HKL) (uintptr_t) 2;
    TEST_ASSERT(vkcode_to_unicode('A', output, 1) == 1 && output[0] == 'b');
    TEST_ASSERT(loads == 3 && frees == 3);
    TEST_ASSERT(vkcode_to_unicode('A', NULL, 1) == 0 && vkcode_to_unicode('A', output, 0) == 0);
    unload_unicode_helper(); unload_unicode_helper();

    // Registry failures still close handles; unreadable images still unload DLLs.
    query_failure = true;
    load_unicode_helper();
    TEST_ASSERT(opened == closed);
    TEST_ASSERT(vkcode_to_unicode('A', output, 1) == 0);
    query_failure = false; invalid_image = true;
    load_unicode_helper();
    TEST_ASSERT(vkcode_to_unicode('A', output, 1) == 0 && loads == frees);
    invalid_image = false;
    initialize_images();
    load_unicode_helper();
    TEST_ASSERT(vkcode_to_unicode('A', output, 1) == 1 && output[0] == 'b');
    unload_unicode_helper();
    TEST_ASSERT(loads == frees && opened == closed);
    return EXIT_SUCCESS;
}
