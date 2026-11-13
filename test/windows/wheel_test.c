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

#include <windows.h>
#include <uiohook.h>

#include "dispatch_event.h"
#include "input_helper.h"
#include "test_assert.h"

static UINT amount = 3, sends;
static BOOL query_success = TRUE;
static UINT send_result = 1;
static INPUT sent;
static uiohook_event captured;
static UINT last_query;

static UINT WINAPI mock_send_input(UINT count, LPINPUT input, int size) {
    TEST_ASSERT(count == 1 && size == sizeof(INPUT));
    sent = *input;
    sends++;
    return send_result;
}

static BOOL WINAPI mock_system_parameters(UINT action, UINT param, PVOID value, UINT flags) {
    (void) param;
    (void) flags;
    last_query = action;
    *(UINT *) value = amount;
    return query_success;
}

UINT (WINAPI *__imp_SendInput)(UINT, LPINPUT, int) = mock_send_input;
#ifdef UNICODE
BOOL (WINAPI *__imp_SystemParametersInfoW)(UINT, UINT, PVOID, UINT) = mock_system_parameters;
#else
BOOL (WINAPI *__imp_SystemParametersInfoA)(UINT, UINT, PVOID, UINT) = mock_system_parameters;
#endif

SIZE_T vkcode_to_unicode(DWORD code, PWCHAR buffer, SIZE_T size) {
    (void) code; (void) buffer; (void) size;
    TEST_ASSERT(false);
    return 0;
}

UIOHOOK_API long int hook_get_multi_click_time(void) { return 500; }

static void capture(uiohook_event *const event, void *data) {
    (void) data;
    captured = *event;
}

int main(void) {
    hook_set_dispatch_proc(capture, NULL);
    MSLLHOOKSTRUCT native = { 0 };
    native.mouseData = (DWORD) (120U << 16);
    TEST_ASSERT(!dispatch_mouse_wheel(1, &native, WHEEL_VERTICAL_DIRECTION));
    TEST_ASSERT(captured.data.wheel.rotation == 120 && captured.data.wheel.amount == 3);
    TEST_ASSERT(last_query == SPI_GETWHEELSCROLLLINES);
    TEST_ASSERT(hook_post_event(&captured) == UIOHOOK_SUCCESS);
    TEST_ASSERT(sent.mi.dwFlags == MOUSEEVENTF_WHEEL && (LONG) sent.mi.mouseData == 120);

    amount = WHEEL_PAGESCROLL;
    native.mouseData = (DWORD) ((UINT) (uint16_t) -60 << 16);
    dispatch_mouse_wheel(2, &native, WHEEL_HORIZONTAL_DIRECTION);
    TEST_ASSERT(last_query == SPI_GETWHEELSCROLLCHARS);
    TEST_ASSERT(captured.data.wheel.type == WHEEL_BLOCK_SCROLL && captured.data.wheel.amount == 1);
    TEST_ASSERT(captured.data.wheel.rotation == -60);
    TEST_ASSERT(hook_post_event(&captured) == UIOHOOK_SUCCESS);
    TEST_ASSERT(sent.mi.dwFlags == MOUSEEVENTF_HWHEEL && (LONG) sent.mi.mouseData == -60);

    amount = 0;
    dispatch_mouse_wheel(3, &native, WHEEL_VERTICAL_DIRECTION);
    TEST_ASSERT(captured.data.wheel.amount == 0 && captured.data.wheel.rotation == -60);
    amount = 1000;
    dispatch_mouse_wheel(4, &native, WHEEL_VERTICAL_DIRECTION);
    TEST_ASSERT(captured.data.wheel.amount == UINT8_MAX && captured.data.wheel.rotation == -60);
    query_success = FALSE;
    TEST_ASSERT(!dispatch_mouse_wheel(5, &native, WHEEL_VERTICAL_DIRECTION));
    TEST_ASSERT(captured.data.wheel.amount == 3);

    captured.data.wheel.rotation = 300;
    captured.data.wheel.delta = 100;
    TEST_ASSERT(hook_post_event(&captured) == UIOHOOK_SUCCESS && (LONG) sent.mi.mouseData == 360);
    captured.data.wheel.delta = 0;
    UINT previous = sends;
    TEST_ASSERT(hook_post_event(&captured) == UIOHOOK_FAILURE && sends == previous);
    captured.data.wheel.delta = 120;
    captured.data.wheel.rotation = 0;
    TEST_ASSERT(hook_post_event(&captured) == UIOHOOK_SUCCESS && sends == previous);
    // Fractions are independent by axis and are committed only on success.
    captured.data.wheel.rotation = 1;
    captured.data.wheel.delta = 240;
    TEST_ASSERT(hook_post_event(&captured) == UIOHOOK_SUCCESS && sends == previous);
    captured.data.wheel.direction = WHEEL_HORIZONTAL_DIRECTION;
    TEST_ASSERT(hook_post_event(&captured) == UIOHOOK_SUCCESS && sends == previous);
    TEST_ASSERT(hook_post_event(&captured) == UIOHOOK_SUCCESS && (LONG) sent.mi.mouseData == 1);
    captured.data.wheel.direction = WHEEL_VERTICAL_DIRECTION;
    send_result = 0;
    TEST_ASSERT(hook_post_event(&captured) == UIOHOOK_FAILURE);
    send_result = 1;
    TEST_ASSERT(hook_post_event(&captured) == UIOHOOK_SUCCESS && (LONG) sent.mi.mouseData == 1);
    captured.data.wheel.rotation = 120;
    captured.data.wheel.delta = 120;
    send_result = 0;
    TEST_ASSERT(hook_post_event(&captured) == UIOHOOK_FAILURE);
    return EXIT_SUCCESS;
}
