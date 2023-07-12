/* libUIOHook: Cross-platform keyboard and mouse hooking from userland.
 * Copyright (C) 2006-2022 Alexander Barker.  All Rights Reserved.
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

#include <uiohook.h>

#include "dispatch_event.h"
#include "input_helper.h"
#include "logger.h"

// Virtual event pointer.
static uiohook_event uio_event;

// Click count globals.
static unsigned short click_count = 0;
static uint64_t click_time = 0;
static unsigned short int click_button = MOUSE_NOBUTTON;
static POINT last_click;

// Event dispatch callback.
static dispatcher_t dispatch = NULL;
static void *dispatch_data = NULL;


UIOHOOK_API void hook_set_dispatch_proc(dispatcher_t dispatch_proc, void *user_data) {
    logger(LOG_LEVEL_DEBUG, "%s [%u]: Setting new dispatch callback to %#p.\n",
            __FUNCTION__, __LINE__, dispatch_proc);

    dispatch = dispatch_proc;
    dispatch_data = user_data;
}

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

// Send out an event if a dispatcher was set.
static void dispatch_event(uiohook_event *const event) {
    if (dispatch != NULL) {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Dispatching event type %u.\n",
                __FUNCTION__, __LINE__, event->type);

        dispatch(event, dispatch_data);
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: No dispatch callback set!\n",
                __FUNCTION__, __LINE__);
    }
}

bool dispatch_hook_enable() {
    bool consumed = false;
    // Initialize native input helper functions.
    load_input_helper();

    // Get the local system time in UNIX epoch form.
    #ifdef USE_EPOCH_TIME
    uint64_t timestamp = get_unix_timestamp();
    #else
    uint64_t timestamp = GetMessageTime();
    #endif

    // Populate the hook start event.
    uio_event.time = timestamp;
    uio_event.reserved = 0x00;

    uio_event.type = EVENT_HOOK_ENABLED;
    uio_event.mask = 0x00;

    // Fire the hook start event.
    dispatch_event(&uio_event);
    consumed = uio_event.reserved & 0x01;

    return consumed;
}

bool dispatch_hook_disable() {
    bool consumed = false;
    // Get the local system time in UNIX epoch form.
    #ifdef USE_EPOCH_TIME
    uint64_t timestamp = get_unix_timestamp();
    #else
    uint64_t timestamp = GetMessageTime();
    #endif

    // Populate the hook stop event.
    uio_event.time = timestamp;
    uio_event.reserved = 0x00;

    uio_event.type = EVENT_HOOK_DISABLED;
    uio_event.mask = 0x00;

    // Fire the hook stop event.
    dispatch_event(&uio_event);
    consumed = uio_event.reserved & 0x01;

    // Deinitialize native input helper functions.
    unload_input_helper();

    return consumed;
}

bool dispatch_key_press(KBDLLHOOKSTRUCT *kbhook) {
    bool consumed = false;
    #ifdef USE_EPOCH_TIME
    uint64_t timestamp = get_unix_timestamp();
    #else
    uint64_t timestamp = kbhook->time;
    #endif

    // Check and setup modifiers.
    if      (kbhook->vkCode == VK_LSHIFT)   { set_modifier_mask(MASK_SHIFT_L);     }
    else if (kbhook->vkCode == VK_RSHIFT)   { set_modifier_mask(MASK_SHIFT_R);     }
    else if (kbhook->vkCode == VK_LCONTROL) { set_modifier_mask(MASK_CTRL_L);      }
    else if (kbhook->vkCode == VK_RCONTROL) { set_modifier_mask(MASK_CTRL_R);      }
    else if (kbhook->vkCode == VK_LMENU)    { set_modifier_mask(MASK_ALT_L);       }
    else if (kbhook->vkCode == VK_RMENU)    { set_modifier_mask(MASK_ALT_R);       }
    else if (kbhook->vkCode == VK_LWIN)     { set_modifier_mask(MASK_META_L);      }
    else if (kbhook->vkCode == VK_RWIN)     { set_modifier_mask(MASK_META_R);      }
    else if (kbhook->vkCode == VK_NUMLOCK)  { set_modifier_mask(MASK_NUM_LOCK);    }
    else if (kbhook->vkCode == VK_CAPITAL)  { set_modifier_mask(MASK_CAPS_LOCK);   }
    else if (kbhook->vkCode == VK_SCROLL)   { set_modifier_mask(MASK_SCROLL_LOCK); }

    // Populate key pressed event.
    uio_event.time = timestamp;
    uio_event.reserved = kbhook->flags & (LLKHF_INJECTED | LLKHF_LOWER_IL_INJECTED) ? 0x02 : 0x00;

    uio_event.type = EVENT_KEY_PRESSED;
    uio_event.mask = get_modifiers();

    uio_event.data.keyboard.keycode = keycode_to_scancode(kbhook->vkCode, kbhook->flags);
    uio_event.data.keyboard.rawcode = (uint16_t) kbhook->vkCode;
    uio_event.data.keyboard.keychar = CHAR_UNDEFINED;

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Key %#X pressed. (%#X)\n",
            __FUNCTION__, __LINE__,
            uio_event.data.keyboard.keycode, uio_event.data.keyboard.rawcode);

    // Populate key pressed event.
    dispatch_event(&uio_event);
    consumed = uio_event.reserved & 0x01;

    // If the pressed event was not consumed...
    if (!consumed) {
        // Buffer for unicode typed chars. No more than 2 needed.
        WCHAR buffer[2]; // = { WCH_NONE };

        // If the pressed event was not consumed and a unicode char exists...
        SIZE_T count = keycode_to_unicode(kbhook->vkCode, buffer, sizeof(buffer));
        for (unsigned int i = 0; i < count; i++) {
            // Populate key typed event.
            uio_event.time = timestamp;
            uio_event.reserved = kbhook->flags & (LLKHF_INJECTED | LLKHF_LOWER_IL_INJECTED) ? 0x02 : 0x00;

            uio_event.type = EVENT_KEY_TYPED;
            uio_event.mask = get_modifiers();

            uio_event.data.keyboard.keycode = VC_UNDEFINED;
            uio_event.data.keyboard.rawcode = (uint16_t) kbhook->vkCode;
            uio_event.data.keyboard.keychar = buffer[i];

            logger(LOG_LEVEL_DEBUG, "%s [%u]: Key %#X typed. (%lc)\n",
                    __FUNCTION__, __LINE__,
                    uio_event.data.keyboard.keycode, (wint_t) uio_event.data.keyboard.keychar);

            // Fire key typed event.
            dispatch_event(&uio_event);
            consumed = uio_event.reserved & 0x01;
        }
    }

    return consumed;
}

bool dispatch_key_release(KBDLLHOOKSTRUCT *kbhook) {
    bool consumed = false;
    #ifdef USE_EPOCH_TIME
    uint64_t timestamp = get_unix_timestamp();
    #else
    uint64_t timestamp = kbhook->time;
    #endif

    // Check and setup modifiers.
    if      (kbhook->vkCode == VK_LSHIFT)   { unset_modifier_mask(MASK_SHIFT_L);     }
    else if (kbhook->vkCode == VK_RSHIFT)   { unset_modifier_mask(MASK_SHIFT_R);     }
    else if (kbhook->vkCode == VK_LCONTROL) { unset_modifier_mask(MASK_CTRL_L);      }
    else if (kbhook->vkCode == VK_RCONTROL) { unset_modifier_mask(MASK_CTRL_R);      }
    else if (kbhook->vkCode == VK_LMENU)    { unset_modifier_mask(MASK_ALT_L);       }
    else if (kbhook->vkCode == VK_RMENU)    { unset_modifier_mask(MASK_ALT_R);       }
    else if (kbhook->vkCode == VK_LWIN)     { unset_modifier_mask(MASK_META_L);      }
    else if (kbhook->vkCode == VK_RWIN)     { unset_modifier_mask(MASK_META_R);      }
    else if (kbhook->vkCode == VK_NUMLOCK)  { unset_modifier_mask(MASK_NUM_LOCK);    }
    else if (kbhook->vkCode == VK_CAPITAL)  { unset_modifier_mask(MASK_CAPS_LOCK);   }
    else if (kbhook->vkCode == VK_SCROLL)   { unset_modifier_mask(MASK_SCROLL_LOCK); }

    // Populate key pressed event.
    uio_event.time = timestamp;
    uio_event.reserved = kbhook->flags & (LLKHF_INJECTED | LLKHF_LOWER_IL_INJECTED) ? 0x02 : 0x00;

    uio_event.type = EVENT_KEY_RELEASED;
    uio_event.mask = get_modifiers();

    uio_event.data.keyboard.keycode = keycode_to_scancode(kbhook->vkCode, kbhook->flags);
    uio_event.data.keyboard.rawcode = (uint16_t) kbhook->vkCode;
    uio_event.data.keyboard.keychar = CHAR_UNDEFINED;

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Key %#X released. (%#X)\n",
            __FUNCTION__, __LINE__,
            uio_event.data.keyboard.keycode, uio_event.data.keyboard.rawcode);

    // Fire key released event.
    dispatch_event(&uio_event);
    consumed = uio_event.reserved & 0x01;

    return consumed;
}

bool dispatch_button_press(MSLLHOOKSTRUCT *mshook, uint16_t button) {
    bool consumed = false;
    #ifdef USE_EPOCH_TIME
    uint64_t timestamp = get_unix_timestamp();
    #else
    uint64_t timestamp = mshook->time;
    #endif

    // Track the number of clicks, the button must match the previous button.
    if (button == click_button && (long int) (timestamp - click_time) <= hook_get_multi_click_time()) {
        if (click_count < USHRT_MAX) {
            click_count++;
        } else {
            logger(LOG_LEVEL_WARN, "%s [%u]: Click count overflow detected!\n",
                    __FUNCTION__, __LINE__);
        }
    } else {
        // Reset the click count.
        click_count = 1;

        // Set the previous button.
        click_button = button;
    }

    // Save this events time to calculate the click_count.
    click_time = timestamp;

    // Store the last click point.
    last_click.x = mshook->pt.x;
    last_click.y = mshook->pt.y;

    // Populate mouse pressed event.
    uio_event.time = timestamp;
    uio_event.reserved = mshook->flags & (LLMHF_INJECTED | LLMHF_LOWER_IL_INJECTED) ? 0x02 : 0x00;

    uio_event.type = EVENT_MOUSE_PRESSED;
    uio_event.mask = get_modifiers();

    uio_event.data.mouse.button = button;
    uio_event.data.mouse.clicks = click_count;

    uio_event.data.mouse.x = (int16_t) mshook->pt.x;
    uio_event.data.mouse.y = (int16_t) mshook->pt.y;

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Button %u  pressed %u time(s). (%u, %u)\n",
            __FUNCTION__, __LINE__,
            uio_event.data.mouse.button, uio_event.data.mouse.clicks,
            uio_event.data.mouse.x, uio_event.data.mouse.y);

    // Fire mouse pressed event.
    dispatch_event(&uio_event);
    consumed = uio_event.reserved & 0x01;

    return consumed;
}

bool dispatch_button_release(MSLLHOOKSTRUCT *mshook, uint16_t button) {
    bool consumed = false;
    #ifdef USE_EPOCH_TIME
    uint64_t timestamp = get_unix_timestamp();
    #else
    uint64_t timestamp = mshook->time;
    #endif

    // Populate mouse released event.
    uio_event.time = timestamp;
    uio_event.reserved = mshook->flags & (LLMHF_INJECTED | LLMHF_LOWER_IL_INJECTED) ? 0x02 : 0x00;

    uio_event.type = EVENT_MOUSE_RELEASED;
    uio_event.mask = get_modifiers();

    uio_event.data.mouse.button = button;
    uio_event.data.mouse.clicks = click_count;

    uio_event.data.mouse.x = (int16_t) mshook->pt.x;
    uio_event.data.mouse.y = (int16_t) mshook->pt.y;

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Button %u released %u time(s). (%u, %u)\n",
            __FUNCTION__, __LINE__,
            uio_event.data.mouse.button, uio_event.data.mouse.clicks,
            uio_event.data.mouse.x, uio_event.data.mouse.y);

    // Fire mouse released event.
    dispatch_event(&uio_event);
    consumed = uio_event.reserved & 0x01;

    // If the pressed event was not consumed...
    if (!consumed && last_click.x == mshook->pt.x && last_click.y == mshook->pt.y) {
        // Populate mouse clicked event.
        uio_event.time = timestamp;
        uio_event.reserved = mshook->flags & (LLMHF_INJECTED | LLMHF_LOWER_IL_INJECTED) ? 0x02 : 0x00;

        uio_event.type = EVENT_MOUSE_CLICKED;
        uio_event.mask = get_modifiers();

        uio_event.data.mouse.button = button;
        uio_event.data.mouse.clicks = click_count;
        uio_event.data.mouse.x = (int16_t) mshook->pt.x;
        uio_event.data.mouse.y = (int16_t) mshook->pt.y;

        logger(LOG_LEVEL_DEBUG, "%s [%u]: Button %u clicked %u time(s). (%u, %u)\n",
                __FUNCTION__, __LINE__,
                uio_event.data.mouse.button, uio_event.data.mouse.clicks,
                uio_event.data.mouse.x, uio_event.data.mouse.y);

        // Fire mouse clicked event.
        dispatch_event(&uio_event);
        consumed = uio_event.reserved & 0x01;
    }

    // Reset the number of clicks.
    if (button == click_button && (long int) (timestamp - click_time) > hook_get_multi_click_time()) {
        // Reset the click count.
        click_count = 0;
    }

    return consumed;
}


bool dispatch_mouse_move(MSLLHOOKSTRUCT *mshook) {
    bool consumed = false;
    #ifdef USE_EPOCH_TIME
    uint64_t timestamp = get_unix_timestamp();
    #else
    uint64_t timestamp = mshook->time;
    #endif

    // We received a mouse move event with the mouse actually moving.
    // This verifies that the mouse was moved after being depressed.
    if (last_click.x != mshook->pt.x || last_click.y != mshook->pt.y) {
        // Reset the click count.
        if (click_count != 0 && (long) (timestamp - click_time) > hook_get_multi_click_time()) {
            click_count = 0;
        }

        // Populate mouse move event.
        uio_event.time = timestamp;
        uio_event.reserved = mshook->flags & (LLMHF_INJECTED | LLMHF_LOWER_IL_INJECTED) ? 0x02 : 0x00;

        uio_event.mask = get_modifiers();

        // Check the modifier mask range for MASK_BUTTON1 - 5.
        bool mouse_dragged = uio_event.mask & (MASK_BUTTON1 | MASK_BUTTON2 | MASK_BUTTON3 | MASK_BUTTON4 | MASK_BUTTON5);
        if (mouse_dragged) {
            // Create Mouse Dragged event.
            uio_event.type = EVENT_MOUSE_DRAGGED;
        } else {
            // Create a Mouse Moved event.
            uio_event.type = EVENT_MOUSE_MOVED;
        }

        uio_event.data.mouse.button = MOUSE_NOBUTTON;
        uio_event.data.mouse.clicks = click_count;
        uio_event.data.mouse.x = (int16_t) mshook->pt.x;
        uio_event.data.mouse.y = (int16_t) mshook->pt.y;

        logger(LOG_LEVEL_DEBUG, "%s [%u]: Mouse %s to %u, %u.\n",
                __FUNCTION__, __LINE__,
                mouse_dragged ? "dragged" : "moved",
                uio_event.data.mouse.x, uio_event.data.mouse.y);

        // Fire mouse move event.
        dispatch_event(&uio_event);
        consumed = uio_event.reserved & 0x01;
    }

    return consumed;
}

bool dispatch_mouse_wheel(MSLLHOOKSTRUCT *mshook, uint8_t direction) {
    bool consumed = false;
    #ifdef USE_EPOCH_TIME
    uint64_t timestamp = get_unix_timestamp();
    #else
    uint64_t timestamp = mshook->time;
    #endif

    // Track the number of clicks.
    // Reset the click count and previous button.
    click_count = 0;
    click_button = MOUSE_NOBUTTON;

    // Populate mouse wheel event.
    uio_event.time = timestamp;
    uio_event.reserved = mshook->flags & (LLMHF_INJECTED | LLMHF_LOWER_IL_INJECTED) ? 0x02 : 0x00;

    uio_event.type = EVENT_MOUSE_WHEEL;
    uio_event.mask = get_modifiers();

    uio_event.data.wheel.x = (int16_t) mshook->pt.x;
    uio_event.data.wheel.y = (int16_t) mshook->pt.y;

    /* Delta GET_WHEEL_DELTA_WPARAM(mshook->mouseData)
     * A positive value indicates that the wheel was rotated
     * forward, away from the user; a negative value indicates that
     * the wheel was rotated backward, toward the user. One wheel
     * click is defined as WHEEL_DELTA, which is 120. */
    uio_event.data.wheel.rotation = (int16_t) GET_WHEEL_DELTA_WPARAM(mshook->mouseData);
    uio_event.data.wheel.delta = WHEEL_DELTA;

    UINT uiAction = SPI_GETWHEELSCROLLLINES;
    if (direction == WHEEL_HORIZONTAL_DIRECTION) {
        uiAction = SPI_GETWHEELSCROLLCHARS;
    }

    UINT wheel_amount = 3;
    if (SystemParametersInfo(uiAction, 0, &wheel_amount, 0)) {
        if (wheel_amount == WHEEL_PAGESCROLL) {
            /* If this number is WHEEL_PAGESCROLL, a wheel roll should be interpreted as clicking once in the page
             * down or page up regions of the scroll bar. */

            uio_event.data.wheel.type = WHEEL_BLOCK_SCROLL;
            uio_event.data.wheel.rotation *= 1;
        } else {
            /* If this number is 0, no scrolling should occur.
             * If the number of lines to scroll is greater than the number of lines viewable, the scroll operation
             * should also be interpreted as a page down or page up operation. */

            uio_event.data.wheel.type = WHEEL_UNIT_SCROLL;
            uio_event.data.wheel.rotation *= wheel_amount;
        }

        // Set the direction based on what event was received.
        uio_event.data.wheel.direction = direction;

        logger(LOG_LEVEL_DEBUG, "%s [%u]: Mouse wheel %i / %u of type %u in the %u direction at %u, %u.\n",
                __FUNCTION__, __LINE__,
                uio_event.data.wheel.rotation, uio_event.data.wheel.delta,
                uio_event.data.wheel.type, uio_event.data.wheel.direction,
                uio_event.data.wheel.x, uio_event.data.wheel.y);

        // Fire mouse wheel event.
        dispatch_event(&uio_event);
        consumed = uio_event.reserved & 0x01;
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: SystemParametersInfo() failed, event will be consumed.\n",
                __FUNCTION__, __LINE__);

        consumed = true;
    }

    return consumed;
}
