/* libUIOHook: Cross-platform keyboard and mouse hooking from userland.
 * Copyright (C) 2006-2023 Alexander Barker.  All Rights Reserved.
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

#include <inttypes.h>
#include <uiohook.h>
#include <windows.h>

#include "dispatch_event.h"
#include "input_helper.h"
#include "logger.h"

// Thread and hook handles.
static DWORD hook_thread_id = 0;
static HHOOK keyboard_event_hhook = NULL, mouse_event_hhook = NULL;

// The handle to the DLL module pulled in DllMain on DLL_PROCESS_ATTACH.
extern HINSTANCE hInst;

#ifdef USE_EPOCH_TIME
static uint64_t get_unix_timestamp() {
    FILETIME system_time;

    // Get the local system time in UTC.
    GetSystemTimeAsFileTime(&system_time);

    // Convert the local system time to a Unix epoch in MS.
    // milliseconds = 100-nanoseconds / 10000
    uint64_t timestamp = (((uint64_t) system_time.dwHighDateTime << 32) | system_time.dwLowDateTime) / 10000;

    // Convert Windows epoch to Unix epoch. (1970 - 1601 in milliseconds)
    timestamp -= 11644473600000;

    return timestamp;
}
#endif

void unregister_running_hooks() {
    // Destroy the native hooks.
    if (keyboard_event_hhook != NULL) {
        UnhookWindowsHookEx(keyboard_event_hhook);
        keyboard_event_hhook = NULL;
    }

    if (mouse_event_hhook != NULL) {
        UnhookWindowsHookEx(mouse_event_hhook);
        mouse_event_hhook = NULL;
    }
}

LRESULT CALLBACK keyboard_hook_event_proc(int nCode, WPARAM wParam, LPARAM lParam) {
    bool consumed = false;

    KBDLLHOOKSTRUCT *kbhook = (KBDLLHOOKSTRUCT *) lParam;

    #ifdef USE_EPOCH_TIME
    uint64_t timestamp = get_unix_timestamp();
    #else
    uint64_t timestamp = kbhook->time;
    #endif

    switch (wParam) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            consumed = dispatch_key_press(timestamp, kbhook);
            break;

        case WM_KEYUP:
        case WM_SYSKEYUP:
            consumed = dispatch_key_release(timestamp, kbhook);
            break;

        default:
            // In theory this *should* never execute.
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Unhandled Windows keyboard event: %#X.\n",
                    __FUNCTION__, __LINE__, (unsigned int) wParam);
            break;
    }

    LRESULT hook_result = -1;
    if (nCode < 0 || !consumed) {
        hook_result = CallNextHookEx(keyboard_event_hhook, nCode, wParam, lParam);
    } else {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Consuming the current event. (%li)\n",
                __FUNCTION__, __LINE__, (long) hook_result);
    }

    return hook_result;
}

LRESULT CALLBACK mouse_hook_event_proc(int nCode, WPARAM wParam, LPARAM lParam) {
    bool consumed = false;

    MSLLHOOKSTRUCT *mshook = (MSLLHOOKSTRUCT *) lParam;

    #ifdef USE_EPOCH_TIME
    uint64_t timestamp = get_unix_timestamp();
    #else
    uint64_t timestamp = mshook->time;
    #endif

    switch (wParam) {
        case WM_LBUTTONDOWN:
            set_modifier_mask(MASK_BUTTON1);
            consumed = dispatch_button_press(timestamp, mshook, MOUSE_BUTTON1);
            break;

        case WM_RBUTTONDOWN:
            set_modifier_mask(MASK_BUTTON2);
            consumed = dispatch_button_press(timestamp, mshook, MOUSE_BUTTON2);
            break;

        case WM_MBUTTONDOWN:
            set_modifier_mask(MASK_BUTTON3);
            consumed = dispatch_button_press(timestamp, mshook, MOUSE_BUTTON3);
            break;

        case WM_XBUTTONDOWN:
        case WM_NCXBUTTONDOWN:
            if (HIWORD(mshook->mouseData) == XBUTTON1) {
                set_modifier_mask(MASK_BUTTON4);
                consumed = dispatch_button_press(timestamp, mshook, MOUSE_BUTTON4);
            } else if (HIWORD(mshook->mouseData) == XBUTTON2) {
                set_modifier_mask(MASK_BUTTON5);
                consumed = dispatch_button_press(timestamp, mshook, MOUSE_BUTTON5);
            } else {
                // Extra mouse buttons.
                uint16_t button = HIWORD(mshook->mouseData);

                // Add support for mouse 4 & 5.
                if (button == 4) {
                    set_modifier_mask(MOUSE_BUTTON4);
                } else if (button == 5) {
                    set_modifier_mask(MOUSE_BUTTON5);
                }

                consumed = dispatch_button_press(timestamp, mshook, button);
            }
            break;


        case WM_LBUTTONUP:
            unset_modifier_mask(MASK_BUTTON1);
            consumed = dispatch_button_release(timestamp, mshook, MOUSE_BUTTON1);
            break;

        case WM_RBUTTONUP:
            unset_modifier_mask(MASK_BUTTON2);
            consumed = dispatch_button_release(timestamp, mshook, MOUSE_BUTTON2);
            break;

        case WM_MBUTTONUP:
            unset_modifier_mask(MASK_BUTTON3);
            consumed = dispatch_button_release(timestamp, mshook, MOUSE_BUTTON3);
            break;

        case WM_XBUTTONUP:
        case WM_NCXBUTTONUP:
            if (HIWORD(mshook->mouseData) == XBUTTON1) {
                unset_modifier_mask(MASK_BUTTON4);
                consumed = dispatch_button_release(timestamp, mshook, MOUSE_BUTTON4);
            } else if (HIWORD(mshook->mouseData) == XBUTTON2) {
                unset_modifier_mask(MASK_BUTTON5);
                consumed = dispatch_button_release(timestamp, mshook, MOUSE_BUTTON5);
            } else {
                // Extra mouse buttons.
                uint16_t button = HIWORD(mshook->mouseData);

                // Add support for mouse 4 & 5.
                if (button == 4) {
                    unset_modifier_mask(MOUSE_BUTTON4);
                } else if (button == 5) {
                    unset_modifier_mask(MOUSE_BUTTON5);
                }

                consumed = dispatch_button_release(timestamp, mshook, MOUSE_BUTTON5);
            }
            break;

        case WM_MOUSEMOVE:
            consumed = dispatch_mouse_move(timestamp, mshook);
            break;

        case WM_MOUSEWHEEL:
            consumed = dispatch_mouse_wheel(timestamp, mshook, WHEEL_VERTICAL_DIRECTION);
            break;

        // For horizontal scroll wheel support Windows >= Vista
        case WM_MOUSEHWHEEL:
            consumed = dispatch_mouse_wheel(timestamp, mshook, WHEEL_HORIZONTAL_DIRECTION);
            break;

        default:
            // In theory this *should* never execute.
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Unhandled Windows mouse event: %#X.\n",
                    __FUNCTION__, __LINE__, (unsigned int) wParam);
            break;
    }

    LRESULT hook_result = -1;
    if (nCode < 0 || !consumed) {
        hook_result = CallNextHookEx(mouse_event_hhook, nCode, wParam, lParam);
    } else {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Consuming the current event. (%li)\n",
                __FUNCTION__, __LINE__, (long) hook_result);
    }

    return hook_result;
}

UIOHOOK_API int hook_run() {
    int status = UIOHOOK_FAILURE;

    // Set the thread id we want to signal later.
    hook_thread_id = GetCurrentThreadId();

    // Spot check the hInst in case the library was statically linked and DllMain
    // did not receive a pointer on load.
    if (hInst == NULL) {
        logger(LOG_LEVEL_WARN, "%s [%u]: hInst was not set by DllMain().\n",
                __FUNCTION__, __LINE__);

        hInst = GetModuleHandle(NULL);
        if (hInst == NULL) {
            logger(LOG_LEVEL_ERROR, "%s [%u]: Could not determine hInst for SetWindowsHookEx()! (%#lX)\n",
                    __FUNCTION__, __LINE__, (unsigned long) GetLastError());

            return UIOHOOK_ERROR_GET_MODULE_HANDLE;
        }
    }

    // Create the native hooks.
    keyboard_event_hhook = SetWindowsHookEx(WH_KEYBOARD_LL, keyboard_hook_event_proc, hInst, 0);
    mouse_event_hhook = SetWindowsHookEx(WH_MOUSE_LL, mouse_hook_event_proc, hInst, 0);

    // If we did not encounter a problem, start processing events.
    if (keyboard_event_hhook != NULL && mouse_event_hhook != NULL) {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: SetWindowsHookEx() successful.\n",
                __FUNCTION__, __LINE__);

        // Set the exit status.
        status = UIOHOOK_SUCCESS;

        // Get the local system time in UNIX epoch form.
        #ifdef USE_EPOCH_TIME
        uint64_t timestamp = get_unix_timestamp();
        #else
        uint64_t timestamp = GetMessageTime();
        #endif

        // Initialize native input helper.
        int input_helper_status = load_input_helper();
        if (input_helper_status != UIOHOOK_SUCCESS) {
            unload_input_helper();
            return input_helper_status;
        }

        // Windows does not have a hook start event or callback so we need to manually fake it.
        dispatch_hook_enable(timestamp);

        // Block until the thread receives an WM_QUIT request.
        MSG message;
        while (GetMessage(&message, (HWND) NULL, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        // Get the local system time in UNIX epoch form.
        #ifdef USE_EPOCH_TIME
        timestamp = get_unix_timestamp();
        #else
        timestamp = GetMessageTime();
        #endif

        // We must explicitly call the cleanup handler because Windows does not
        // provide a thread cleanup method like POSIX pthread_cleanup_push/pop.
        dispatch_hook_disable(timestamp);

        // Uninitialize the native input helper.
        unload_input_helper();
    } else {
        logger(LOG_LEVEL_ERROR, "%s [%u]: SetWindowsHookEx() failed! (%#lX)\n",
                __FUNCTION__, __LINE__, (unsigned long) GetLastError());

        status = UIOHOOK_ERROR_SET_WINDOWS_HOOK_EX;
    }

    // Unregister any hooks that may still be installed.
    unregister_running_hooks();

    return status;
}

UIOHOOK_API int hook_stop() {
    int status = UIOHOOK_FAILURE;

    // Try to exit the thread naturally.
    if (PostThreadMessage(hook_thread_id, WM_QUIT, (WPARAM) NULL, (LPARAM) NULL)) {
        status = UIOHOOK_SUCCESS;
    }

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Status: %#X.\n",
            __FUNCTION__, __LINE__, status);

    return status;
}
