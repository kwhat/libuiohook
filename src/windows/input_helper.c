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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <uiohook.h>
#include <windows.h>

#include "logger.h"
#include "input_helper.h"
#include "input_state.h"

static void initialize_modifiers() {
    // NOTE We are checking the high order bit, so it will be < 0 for a singed short.
    if (GetKeyState(VK_LSHIFT)   < 0) { set_modifier_mask(MASK_SHIFT_L);     }
    if (GetKeyState(VK_RSHIFT)   < 0) { set_modifier_mask(MASK_SHIFT_R);     }
    if (GetKeyState(VK_LCONTROL) < 0) { set_modifier_mask(MASK_CTRL_L);      }
    if (GetKeyState(VK_RCONTROL) < 0) { set_modifier_mask(MASK_CTRL_R);      }
    if (GetKeyState(VK_LMENU)    < 0) { set_modifier_mask(MASK_ALT_L);       }
    if (GetKeyState(VK_RMENU)    < 0) { set_modifier_mask(MASK_ALT_R);       }
    if (GetKeyState(VK_LWIN)     < 0) { set_modifier_mask(MASK_META_L);      }
    if (GetKeyState(VK_RWIN)     < 0) { set_modifier_mask(MASK_META_R);      }

    if (GetKeyState(VK_LBUTTON)  < 0) { set_modifier_mask(MASK_BUTTON1);     }
    if (GetKeyState(VK_RBUTTON)  < 0) { set_modifier_mask(MASK_BUTTON2);     }
    if (GetKeyState(VK_MBUTTON)  < 0) { set_modifier_mask(MASK_BUTTON3);     }
    if (GetKeyState(VK_XBUTTON1) < 0) { set_modifier_mask(MASK_BUTTON4);     }
    if (GetKeyState(VK_XBUTTON2) < 0) { set_modifier_mask(MASK_BUTTON5);     }

    if (GetKeyState(VK_NUMLOCK)  < 0) { set_modifier_mask(MASK_NUM_LOCK);    }
    if (GetKeyState(VK_CAPITAL)  < 0) { set_modifier_mask(MASK_CAPS_LOCK);   }
    if (GetKeyState(VK_SCROLL)   < 0) { set_modifier_mask(MASK_SCROLL_LOCK); }
}


/***********************************************************************
 * The following code is based on code provided by Marc-André Moreau
 * to work around a failure to support dead keys in the ToUnicode() API.
 * According to the author some parts were taken directly from
 * Microsoft's kbd.h header file that is shipped with the Windows Driver
 * Development Kit.
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
 *   http://www.techmantras.com/content/writing-keyloggers-full-length-tutorial
 *
 ***********************************************************************/

// Structure and pointers for the keyboard locale cache.
typedef struct _KeyboardLocale {
    HKL id;                                 // Locale ID
    HINSTANCE library;                      // Keyboard DLL instance.
    PVK_TO_BIT pVkToBit;                    // Pointers struct arrays.
    PVK_TO_WCHAR_TABLE pVkToWcharTable;
    PDEADKEY pDeadKey;
    struct _KeyboardLocale* next;
} KeyboardLocale;

static KeyboardLocale* locale_first = NULL;
static KeyboardLocale* locale_current = NULL;
static WCHAR deadChar = WCH_NONE;

// Amount of pointer padding to apply for Wow64 instances.
static unsigned short int ptr_padding = 0;

#if defined(_WIN32) && !defined(_WIN64)
// Small function to check and see if we are executing under Wow64.
static BOOL is_wow64() {
    BOOL status = FALSE;

    LPFN_ISWOW64PROCESS pIsWow64Process = (LPFN_ISWOW64PROCESS)
            GetProcAddress(GetModuleHandle("kernel32"), "IsWow64Process");

    if (pIsWow64Process != NULL) {
        HANDLE current_proc = GetCurrentProcess();

        if (!pIsWow64Process(current_proc, &status)) {
            status = FALSE;

            logger(LOG_LEVEL_DEBUG, "%s [%u]: pIsWow64Process(%#p, %#p) failed!\n",
                    __FUNCTION__, __LINE__, current_proc, &status);
        }
    }

    return status;
}
#endif

// Locate the DLL that contains the current keyboard layout.
static int get_keyboard_layout_file(char *layoutFile, DWORD bufferSize) {
    int status = UIOHOOK_FAILURE;
    HKEY hKey;
    DWORD varType = REG_SZ;

    char kbdName[KL_NAMELENGTH * 4];
    if (GetKeyboardLayoutName(kbdName)) {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Found keyboard layout \"%s\".\n",
                __FUNCTION__, __LINE__, kbdName);

        const char *regPrefix = "SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts\\";
        size_t regPathSize = strlen(regPrefix) + strlen(kbdName) + 1;
        char *regPath = malloc(regPathSize);
        if (regPath != NULL) {
            strcpy_s(regPath, regPathSize, regPrefix);
            strcat_s(regPath, regPathSize, kbdName);

            if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, (LPCTSTR) regPath, 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
                const char *regKey =  "Layout File";
                if (RegQueryValueEx(hKey, regKey, NULL, &varType, (LPBYTE) layoutFile, &bufferSize) == ERROR_SUCCESS) {
                    RegCloseKey(hKey);
                    status = UIOHOOK_SUCCESS;
                } else {
                    logger(LOG_LEVEL_WARN, "%s [%u]: RegOpenKeyEx failed to open key: \"%s\"!\n",
                            __FUNCTION__, __LINE__, regKey);
                }
            } else {
                logger(LOG_LEVEL_WARN, "%s [%u]: RegOpenKeyEx failed to open key: \"%s\"!\n",
                        __FUNCTION__, __LINE__, regPath);
            }

            free(regPath);
        } else {
            logger(LOG_LEVEL_WARN, "%s [%u]: malloc(%u) failed!\n",
                    __FUNCTION__, __LINE__, regPathSize);
        }
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: GetKeyboardLayoutName() failed!\n",
                __FUNCTION__, __LINE__);
    }

    return status;
}

// Returns the number of locales that were loaded.
static int refresh_locale_list() {
    int count = 0;

    // Get the number of layouts the user has activated.
    int hkl_size = GetKeyboardLayoutList(0, NULL);
    if (hkl_size > 0) {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: GetKeyboardLayoutList(0, NULL) found %i layouts.\n",
                __FUNCTION__, __LINE__, hkl_size);

        // Get the thread id that currently has focus for our default.
        DWORD focus_pid = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
        HKL hlk_focus = GetKeyboardLayout(focus_pid);
        HKL hlk_default = GetKeyboardLayout(0);
        HKL *hkl_list = malloc(sizeof(HKL) * hkl_size);

        int new_size = GetKeyboardLayoutList(hkl_size, hkl_list);
        if (new_size > 0) {
            if (new_size != hkl_size) {
                logger(LOG_LEVEL_WARN, "%s [%u]: Locale size mismatch!  Expected %i, received %i!\n",
                        __FUNCTION__, __LINE__, hkl_size, new_size);
            } else {
                logger(LOG_LEVEL_DEBUG, "%s [%u]: Received %i locales.\n",
                        __FUNCTION__, __LINE__, new_size);
            }

            KeyboardLocale* locale_previous = NULL;
            KeyboardLocale* locale_item = locale_first;

            // Go though the linked list and remove KeyboardLocale's that are
            // no longer loaded.
            while (locale_item != NULL) {
                // Check to see if the old HKL is in the new list.
                bool is_loaded = false;
                for (int i = 0; i < new_size && !is_loaded; i++) {
                    if (locale_item->id == hkl_list[i]) {
                        // Flag and jump out of the loop.
                        hkl_list[i] = NULL;
                        is_loaded = true;
                    }
                }


                if (is_loaded) {
                    logger(LOG_LEVEL_DEBUG, "%s [%u]: Found locale ID %#p in the cache.\n",
                            __FUNCTION__, __LINE__, locale_item->id);

                    // Set the previous local to the current locale.
                    locale_previous = locale_item;

                    // Check and see if the locale is our current active locale.
                    if (locale_item->id == hlk_focus) {
                        locale_current = locale_item;
                    }

                    count++;
                } else {
                    logger(LOG_LEVEL_DEBUG, "%s [%u]: Removing locale ID %#p from the cache.\n",
                            __FUNCTION__, __LINE__, locale_item->id);

                    // If the old id is not in the new list, remove it.
                    locale_previous->next = locale_item->next;

                    // Make sure the locale_current points NULL or something valid.
                    if (locale_item == locale_current) {
                        locale_current = NULL;
                    }

                    // Free the memory used by locale_item;
                    free(locale_item);

                    // Set the item to the pervious item to guarantee a next.
                    locale_item = locale_previous;
                }

                // Iterate to the next linked list item.
                locale_item = locale_item->next;
            }


            // Insert anything new into the linked list.
            for (int i = 0; i < new_size; i++) {
                // Check to see if the item was already in the list.
                if (hkl_list[i] != NULL) {
                    // Set the active keyboard layout for this thread to the HKL.
                    ActivateKeyboardLayout(hkl_list[i], 0x00);

                    // Try to pull the current keyboard layout DLL from the registry.
                    char layoutFile[MAX_PATH];
                    if (get_keyboard_layout_file(layoutFile, MAX_PATH) == UIOHOOK_SUCCESS) {
                        // You can't trust the %SYSPATH%, look it up manually.
                        char systemDirectory[MAX_PATH];
                        if (GetSystemDirectory(systemDirectory, MAX_PATH) != 0) {
                            char kbdLayoutFilePath[MAX_PATH];
                            snprintf(kbdLayoutFilePath, MAX_PATH, "%s\\%s", systemDirectory, layoutFile);

                            logger(LOG_LEVEL_DEBUG, "%s [%u]: Loading layout for %#p: %s.\n",
                                    __FUNCTION__, __LINE__, hkl_list[i], layoutFile);

                            // Create the new locale item.
                            locale_item = malloc(sizeof(KeyboardLocale));
                            locale_item->id = hkl_list[i];
                            locale_item->library = LoadLibrary(kbdLayoutFilePath);

                            #if __GNUC__
                            #pragma GCC diagnostic push
                            #pragma GCC diagnostic ignored "-Wcast-function-type"
                            #endif
                            // Get the function pointer from the library to get the keyboard layer descriptor.
                            KbdLayerDescriptor pKbdLayerDescriptor = (KbdLayerDescriptor) GetProcAddress(locale_item->library, "KbdLayerDescriptor");
                            #if __GNUC__
                            #pragma GCC diagnostic pop
                            #endif

                            if (pKbdLayerDescriptor != NULL) {
                                PKBDTABLES pKbd = pKbdLayerDescriptor();

                                // Store the memory address of the following 3 structures.
                                BYTE *base = (BYTE *) pKbd;

                                // First element of each structure, no offset adjustment needed.
                                locale_item->pVkToBit = pKbd->pCharModifiers->pVkToBit;

                                // Second element of pKbd, +4 byte offset on wow64.
                                locale_item->pVkToWcharTable = *((PVK_TO_WCHAR_TABLE *) (base + offsetof(KBDTABLES, pVkToWcharTable) + ptr_padding));

                                // Third element of pKbd, +8 byte offset on wow64.
                                locale_item->pDeadKey = *((PDEADKEY *) (base + offsetof(KBDTABLES, pDeadKey) + (ptr_padding * 2)));

                                // This will always be added to the end of the list.
                                locale_item->next = NULL;

                                // Insert the item into the linked list.
                                if (locale_previous == NULL) {
                                    // If nothing came before, the list is empty.
                                    locale_first = locale_item;
                                } else {
                                    // Append the new locale to the end of the list.
                                    locale_previous->next = locale_item;
                                }

                                // Check and see if the locale is our current active locale.
                                if (locale_item->id == hlk_focus) {
                                    locale_current = locale_item;
                                }

                                // Set the pervious locale item to the new one.
                                locale_previous = locale_item;

                                count++;
                            } else {
                                logger(LOG_LEVEL_ERROR, "%s [%u]: GetProcAddress() failed for KbdLayerDescriptor!\n",
                                        __FUNCTION__, __LINE__);

                                FreeLibrary(locale_item->library);
                                free(locale_item);
                                locale_item = NULL;
                            }
                        } else {
                            logger(LOG_LEVEL_ERROR, "%s [%u]: GetSystemDirectory() failed!\n",
                                    __FUNCTION__, __LINE__);
                        }
                    } else {
                        logger(LOG_LEVEL_ERROR, "%s [%u]: Could not find keyboard map for locale %#p!\n",
                                __FUNCTION__, __LINE__, hkl_list[i]);
                    }
                }
            }
        } else {
            logger(LOG_LEVEL_ERROR, "%s [%u]: GetKeyboardLayoutList() failed!\n",
                    __FUNCTION__, __LINE__);

            // TODO Try and recover by using the current layout.
            // Hint: Use locale_id instead of hkl_list[i] in the loop above.
        }

        free(hkl_list);
        ActivateKeyboardLayout(hlk_default, 0x00);
    }

    return count;
}

// Returns the number of chars written to the buffer.
SIZE_T vkcode_to_unicode(DWORD vkcode, PWCHAR buffer, SIZE_T size) {
    // Get the thread id that currently has focus and ask for its current locale.
    DWORD focus_pid = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
    HKL locale_id = GetKeyboardLayout(focus_pid);
    if (locale_id == NULL) {
        // Default to the current thread's layout if the focused window fails.
        locale_id = GetKeyboardLayout(0);
    }

    // If the current Locale is not the new locale, search the linked list.
    if (locale_current == NULL || locale_current->id != locale_id) {
        locale_current = NULL;
        KeyboardLocale* locale_item = locale_first;

        // Search the linked list...
        while (locale_item != NULL && locale_item->id != locale_id) {
            locale_item = locale_item->next;
        }

        // You may already be a winner!
        if (locale_item != NULL && locale_item->id == locale_id) {
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Activating keyboard layout %#p.\n",
                    __FUNCTION__, __LINE__, locale_item->id);

            // Switch the current locale.
            locale_current = locale_item;
            locale_item = NULL;

            // If they layout changes the dead key state needs to be reset.
            // This is consistent with the way Windows handles locale changes.
            deadChar = WCH_NONE;
        } else {
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Refreshing locale cache.\n",
                    __FUNCTION__, __LINE__);

            refresh_locale_list();
        }
    }

    // Initialize to empty.
    SIZE_T charCount = 0;
    // buffer[i] = WCH_NONE;

    // Check and make sure the Unicode helper was loaded.
    if (locale_current != NULL) {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Using keyboard layout %#p.\n",
                __FUNCTION__, __LINE__, locale_current->id);

        int mod = 0;

        int capsLock = (GetKeyState(VK_CAPITAL) & 0x01);

        PVK_TO_BIT pVkToBit = locale_current->pVkToBit;
        PVK_TO_WCHAR_TABLE pVkToWcharTable = locale_current->pVkToWcharTable;
        PDEADKEY pDeadKey = locale_current->pDeadKey;

        /* Loop over the modifier keys for this locale and determine what is
         * currently depressed.  Because this is only a structure of two
         * bytes, we don't need to worry about the structure padding of __ptr64
         * offsets on Wow64.
         */
        bool is_shift = false, is_ctrl = false, is_alt = false;
        for (int i = 0; pVkToBit[i].Vk != 0; i++) {
            short state = GetAsyncKeyState(pVkToBit[i].Vk);

            // Check to see if the most significant bit is active.
            if (state & ~SHRT_MAX) {
                if (pVkToBit[i].Vk == VK_SHIFT) {
                    is_shift = true;
                } else if (pVkToBit[i].Vk == VK_CONTROL) {
                    is_ctrl = true;
                } else if (pVkToBit[i].Vk == VK_MENU) {
                    is_alt = true;
                }
            }
        }

        // Check the Shift modifier.
        if (is_shift) {
            mod = 1;
        }

        // Check for the AltGr modifier.
        if (is_ctrl && is_alt) {
            mod += 3;
        }

        // Default 32 bit structure size should be 6 bytes (4 for the pointer and 2
        // additional byte fields) that are padded out to 8 bytes by the compiler.
        unsigned short sizeVkToWcharTable = sizeof(VK_TO_WCHAR_TABLE);
        #if defined(_WIN32) && !defined(_WIN64)
        if (is_wow64()) {
            // If we are running under Wow64 the size of the first pointer will be
            // 8 bringing the total size to 10 bytes padded out to 16.
            sizeVkToWcharTable = (sizeVkToWcharTable + ptr_padding + 7) & -8;
        }
        #endif

        BYTE *ptrCurrentVkToWcharTable = (BYTE *) pVkToWcharTable;

        int cbSize, n;
        do {
            // cbSize is used to calculate n, and n is used for the size of pVkToWchars[j].wch[n]
            cbSize = *(ptrCurrentVkToWcharTable + offsetof(VK_TO_WCHAR_TABLE, cbSize) + ptr_padding);
            n = (cbSize - 2) / 2;

            // Same as VK_TO_WCHARS pVkToWchars[] = pVkToWcharTable[i].pVkToWchars
            PVK_TO_WCHARS pVkToWchars = (PVK_TO_WCHARS) ((PVK_TO_WCHAR_TABLE) ptrCurrentVkToWcharTable)->pVkToWchars;

            if (pVkToWchars != NULL && mod < n) {
                // pVkToWchars[j].VirtualKey
                BYTE *pCurrentVkToWchars = (BYTE *) pVkToWchars;

                do {
                    if (((PVK_TO_WCHARS) pCurrentVkToWchars)->VirtualKey == vkcode) {
                        if ((((PVK_TO_WCHARS) pCurrentVkToWchars)->Attributes == CAPLOK) && capsLock) {
                            if (is_shift && mod > 0) {
                                mod -= 1;
                            } else {
                                mod += 1;
                            }
                        }
                        
                        // Set the initial unicode char.
                        WCHAR unicode = ((PVK_TO_WCHARS) pCurrentVkToWchars)->wch[mod];
        
                        // Increment the pCurrentVkToWchars by the size of wch[n].
                        pCurrentVkToWchars += sizeof(VK_TO_WCHARS) + (sizeof(WCHAR) * n);

                        
                        if (unicode == WCH_DEAD) {
                            // The current unicode char is a dead key...
                            if (deadChar == WCH_NONE) {
                                // No previous dead key was set so cache the next 
                                // wchar so we know what to do next time its pressed.
                                deadChar = ((PVK_TO_WCHARS) pCurrentVkToWchars)->wch[mod];
                            } else {
                                if (size >= 2) {
                                    // Received a second dead key.
                                    memset(buffer, deadChar, 2);
                                    //buffer[0] = deadChar;
                                    //buffer[1] = deadChar;
                                    
                                    deadChar = WCH_NONE;
                                    charCount = 2;
                                }
                            }
                        } else if (unicode != WCH_NONE) {
                            // We are not WCH_NONE or WCH_DEAD
                            if (size >= 1) {
                                buffer[0] = unicode;
                                charCount = 1;
                            }
                        }

                        break;
                    } else {
                        // Add sizeof WCHAR because we are really an array of WCHAR[n] not WCHAR[]
                        pCurrentVkToWchars += sizeof(VK_TO_WCHARS) + (sizeof(WCHAR) * n);
                    }
                } while ( ((PVK_TO_WCHARS) pCurrentVkToWchars)->VirtualKey != 0 );
            }

            // This is effectively the same as: ptrCurrentVkToWcharTable = pVkToWcharTable[++i];
            ptrCurrentVkToWcharTable += sizeVkToWcharTable;
        } while (cbSize != 0);


        // If the current local has a dead key set.
        if (deadChar != WCH_NONE) {
            // Loop over the pDeadKey lookup table for the locale.
            for (int i = 0; pDeadKey[i].dwBoth != 0; i++) {
                WCHAR baseChar = (WCHAR) pDeadKey[i].dwBoth;
                WCHAR diacritic = (WCHAR) (pDeadKey[i].dwBoth >> 16);

                // If we locate an extended dead char, set it.
                if (size >= 1 && baseChar == buffer[0] && diacritic == deadChar) {
                    deadChar = WCH_NONE;
                    
                    if (charCount <= size) {
                        memset(buffer, (WCHAR) pDeadKey[i].wchComposed, charCount);
                        //buffer[i] = (WCHAR) pDeadKey[i].wchComposed;
                    }
                }
            }
        }
    }

    return charCount;
}

// Returns the number of locales that were loaded.
int load_input_helper() {
    initialize_modifiers();

    #if defined(_WIN32) && !defined(_WIN64)
    if (is_wow64()) {
        ptr_padding = sizeof(void *);
    }
    #endif

    int count = refresh_locale_list();
    logger(LOG_LEVEL_DEBUG, "%s [%u]: loaded %i locale(s).\n",
            __FUNCTION__, __LINE__, count);

    return UIOHOOK_SUCCESS;
}

// Returns the number of locales that were removed.
void unload_input_helper() {
    int count = 0;

    // Cleanup and free memory from the old list.
    KeyboardLocale* locale_item = locale_first;
    while (locale_item != NULL) {
        // Remove the first item from the linked list.
        FreeLibrary(locale_item->library);
        locale_first = locale_item->next;
        free(locale_item);
        locale_item = locale_first;

        count++;
    }

    logger(LOG_LEVEL_DEBUG, "%s [%u]: unloaded %i locale(s).\n",
            __FUNCTION__, __LINE__, count);

    // Reset the current local.
    locale_current = NULL;

    // Reset the modifier mask.
    unset_modifier_mask(UINT16_MAX);
}
