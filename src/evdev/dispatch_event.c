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

#include <limits.h>
#include <stdlib.h>
#include <wchar.h>

#include "dispatch_event.h"
#include "input_helper.h"
#include "logger.h"

typedef struct _mouse_click {
    uint16_t count;
    uint64_t time;
    uint16_t button;
} mouse_click;
static mouse_click click = {
    .count = 0,
    .time = 0,
    .button = MOUSE_NOBUTTON
};

// Virtual event pointer.
static uiohook_event uio_event;

// Event dispatch callback.
static dispatcher_t dispatch = NULL;
static void *dispatch_data = NULL;


UIOHOOK_API void hook_set_dispatch_proc(dispatcher_t dispatch_proc, void *user_data) {
    logger(LOG_LEVEL_DEBUG, "%s [%u]: Setting new dispatch callback to %#p.\n",
            __FUNCTION__, __LINE__, dispatch_proc);

    dispatch = dispatch_proc;
    dispatch_data = user_data;
}


// Send out an event if a dispatcher was set.
static void dispatch_event(uiohook_event *const uio_event) {
    if (dispatch != NULL) {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Dispatching event type %u.\n",
                __FUNCTION__, __LINE__, uio_event->type);

        dispatch(uio_event, dispatch_data);
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: No dispatch callback set!\n",
                __FUNCTION__, __LINE__);
    }
}

void dispatch_hook_enabled() {
    // Initialize native input helper functions.
    load_input_helper();

    #ifdef USE_EPOCH_TIME
    // Get the local system time in UTC.
    struct timeval system_time;
    gettimeofday(&system_time, NULL);

    uint64_t timestamp = get_unix_timestamp(system_time.tv_sec, system_time.tv_usec);
    #else
    uint64_t timestamp = get_seq_timestamp();
    #endif

    // Populate the hook start event.
    uio_event.time = timestamp;
    uio_event.reserved = 0x00;

    uio_event.type = EVENT_HOOK_ENABLED;
    uio_event.mask = 0x00;

    // Fire the hook start event.
    dispatch_event(&uio_event);
}

void dispatch_hook_disabled() {
    #ifdef USE_EPOCH_TIME
    struct timeval system_time;
    gettimeofday(&system_time, NULL);

    uint64_t timestamp = get_unix_timestamp(system_time.tv_sec, system_time.tv_usec);
    #else
    uint64_t timestamp = get_seq_timestamp();
    #endif

    // Populate the hook stop event.
    uio_event.time = timestamp;
    uio_event.reserved = 0x00;

    uio_event.type = EVENT_HOOK_DISABLED;
    uio_event.mask = 0x00;

    // Fire the hook stop event.
    dispatch_event(&uio_event);

    // Uninitialize native input helper functions.
    unload_input_helper();
}

bool dispatch_key_press(struct input_event *const ev) {
    #ifdef USE_EPOCH_TIME
    uint64_t timestamp = get_unix_timestamp(ev->input_event_sec, ev->input_event_usec);
    #else
    uint64_t timestamp = get_seq_timestamp();
    #endif

    xkb_keycode_t keycode = event_to_keycode(ev->code);
    xkb_keysym_t keysym = event_to_keysym(keycode, ev->value);
    uint16_t uiocode = keysym_to_uiocode(keysym);

    // Populate key pressed event.
    uio_event.time = timestamp;
    uio_event.reserved = 0x00;

    uio_event.type = EVENT_KEY_PRESSED;
    uio_event.mask = get_modifiers();

    uio_event.data.keyboard.keycode = uiocode;
    uio_event.data.keyboard.rawcode = keycode;
    uio_event.data.keyboard.keychar = CHAR_UNDEFINED;

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Key %#X pressed. (%#X)\n",
            __FUNCTION__, __LINE__,
            uio_event.data.keyboard.keycode, uio_event.data.keyboard.rawcode);

    // Fire key pressed event.
    dispatch_event(&uio_event);

    // If the pressed event was not consumed and we got a char in the buffer.
    if (uio_event.reserved ^ 0x01) {
        wchar_t surrogate[4] = {};
        size_t count = keycode_to_utf8(keycode, surrogate, sizeof(surrogate));
        for (unsigned int i = 0; i < count; i++) {
            // Populate key typed event.
            uio_event.time = timestamp;
            uio_event.reserved = 0x00;

            uio_event.type = EVENT_KEY_TYPED;
            uio_event.mask = get_modifiers();

            uio_event.data.keyboard.keycode = VC_UNDEFINED;
            uio_event.data.keyboard.rawcode = keycode;
            uio_event.data.keyboard.keychar = surrogate[i];

            logger(LOG_LEVEL_DEBUG, "%s [%u]: Key %#X typed. (%lc)\n",
                    __FUNCTION__, __LINE__,
                    uio_event.data.keyboard.keycode, uio_event.data.keyboard.keychar);

            // Fire key typed event.
            dispatch_event(&uio_event);
        }
    }

    return uio_event.reserved & 0x01;
}

bool dispatch_key_release(struct input_event *const ev) {
    #ifdef USE_EPOCH_TIME
    uint64_t timestamp = get_unix_timestamp(ev->input_event_sec, ev->input_event_usec);
    #else
    uint64_t timestamp = get_seq_timestamp();
    #endif

    xkb_keycode_t keycode = event_to_keycode(ev->code);
    xkb_keysym_t keysym = event_to_keysym(keycode, ev->value);
    uint16_t uiocode = keysym_to_uiocode(keysym);

    // Populate key released event.
    uio_event.time = timestamp;
    uio_event.reserved = 0x00;

    uio_event.type = EVENT_KEY_RELEASED;
    uio_event.mask = get_modifiers();

    uio_event.data.keyboard.keycode = uiocode;
    uio_event.data.keyboard.rawcode = keycode;
    uio_event.data.keyboard.keychar = CHAR_UNDEFINED;

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Key %#X released. (%#X)\n",
            __FUNCTION__, __LINE__,
            uio_event.data.keyboard.keycode, uio_event.data.keyboard.rawcode);

    // Fire key released event.
    dispatch_event(&uio_event);

    return uio_event.reserved & 0x01;
}

// FIXME Why doesn't this live in the click structure? click.is_drag?
// Set to true by dispatch_mouse_move between a button press and release so we know
// whether to synthesize an EVENT_MOUSE_CLICKED on release.
static bool mouse_dragged = false;

bool dispatch_mouse_press(struct input_event *const ev, uint16_t button) {
    #ifdef USE_EPOCH_TIME
    uint64_t timestamp = get_unix_timestamp(ev->input_event_sec, ev->input_event_usec);
    #else
    uint64_t timestamp = get_seq_timestamp();
    #endif

    // Track the number of clicks, the button must match the previous button.
    if (button == click.button && timestamp - click.time <= hook_get_multi_click_time()) {
        if (click.count < UINT16_MAX) {
            click.count++;
        } else {
            logger(LOG_LEVEL_WARN, "%s [%u]: Click count overflow detected!\n",
                    __FUNCTION__, __LINE__);
        }
    } else {
        // Reset the click count.
        click.count = 1;

        // Set the last clicked button.
        click.button = button;
    }

    // Save this events time to calculate multi-clicks.
    click.time = timestamp;

    // Reset drag tracking; any motion before release cancels the clicked event.
    mouse_dragged = false;

    // Populate mouse pressed event.
    uio_event.time = timestamp;
    uio_event.reserved = 0x00;

    uio_event.type = EVENT_MOUSE_PRESSED;
    uio_event.mask = get_modifiers();

    uio_event.data.mouse.button = button;
    uio_event.data.mouse.clicks = click.count;
    get_pointer_position(&uio_event.data.mouse.x, &uio_event.data.mouse.y);

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Button %u pressed %u time(s). (%i, %i)\n",
            __FUNCTION__, __LINE__,
            uio_event.data.mouse.button, uio_event.data.mouse.clicks,
            uio_event.data.mouse.x, uio_event.data.mouse.y);

    // Fire mouse pressed event.
    dispatch_event(&uio_event);

    return uio_event.reserved & 0x01;
}

static void dispatch_mouse_clicked(uint64_t timestamp, uint16_t button) {
    // Populate mouse clicked event.
    uio_event.time = timestamp;
    uio_event.reserved = 0x00;

    uio_event.type = EVENT_MOUSE_CLICKED;
    uio_event.mask = get_modifiers();

    uio_event.data.mouse.button = button;
    uio_event.data.mouse.clicks = click.count;
    get_pointer_position(&uio_event.data.mouse.x, &uio_event.data.mouse.y);

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Button %u clicked %u time(s). (%i, %i)\n",
            __FUNCTION__, __LINE__,
            uio_event.data.mouse.button, uio_event.data.mouse.clicks,
            uio_event.data.mouse.x, uio_event.data.mouse.y);

    // Fire mouse clicked event.
    dispatch_event(&uio_event);
}

bool dispatch_mouse_release(struct input_event *const ev, uint16_t button) {
    #ifdef USE_EPOCH_TIME
    uint64_t timestamp = get_unix_timestamp(ev->input_event_sec, ev->input_event_usec);
    #else
    uint64_t timestamp = get_seq_timestamp();
    #endif

    // Populate mouse released event.
    uio_event.time = timestamp;
    uio_event.reserved = 0x00;

    uio_event.type = EVENT_MOUSE_RELEASED;
    uio_event.mask = get_modifiers();

    uio_event.data.mouse.button = button;
    uio_event.data.mouse.clicks = click.count;
    get_pointer_position(&uio_event.data.mouse.x, &uio_event.data.mouse.y);

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Button %u released %u time(s). (%i, %i)\n",
            __FUNCTION__, __LINE__,
            uio_event.data.mouse.button, uio_event.data.mouse.clicks,
            uio_event.data.mouse.x, uio_event.data.mouse.y);

    // Fire mouse released event.
    dispatch_event(&uio_event);
    bool consumed = uio_event.reserved & 0x01;

    // If the release was not consumed and the pointer didn't move between press and release,
    // fire a clicked event.
    if (!consumed && !mouse_dragged) {
        dispatch_mouse_clicked(timestamp, button);
    }

    // Reset the click count if the multi-click interval has elapsed.
    if (button == click.button && timestamp - click.time > hook_get_multi_click_time()) {
        click.count = 0;
    }

    return consumed;
}

bool dispatch_mouse_move(struct input_event *const ev) {
    #ifdef USE_EPOCH_TIME
    uint64_t timestamp = get_unix_timestamp(ev->input_event_sec, ev->input_event_usec);
    #else
    uint64_t timestamp = get_seq_timestamp();
    #endif

    // Reset the click count if the multi-click interval has elapsed.
    if (click.count != 0 && timestamp - click.time > hook_get_multi_click_time()) {
        click.count = 0;
    }

    // Populate mouse move event.
    uio_event.time = timestamp;
    uio_event.reserved = 0x00;

    uio_event.mask = get_modifiers();

    // Check the upper half of virtual modifiers for non-zero values and set the mouse
    // dragged flag.  The last 3 bits are reserved for lock masks.
    bool is_dragged = (bool) (uio_event.mask & 0x1F00);
    if (is_dragged) {
        uio_event.type = EVENT_MOUSE_DRAGGED;

        // Motion with a button held cancels the clicked event on release.
        mouse_dragged = true;
    } else {
        uio_event.type = EVENT_MOUSE_MOVED;
    }

    uio_event.data.mouse.button = MOUSE_NOBUTTON;
    uio_event.data.mouse.clicks = click.count;

    // evdev only reports relative deltas; query the display server for the
    // resulting absolute pointer position.
    get_pointer_position(&uio_event.data.mouse.x, &uio_event.data.mouse.y);

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Mouse %s to %i, %i. (%#X)\n",
            __FUNCTION__, __LINE__,
            is_dragged ? "dragged" : "moved",
            uio_event.data.mouse.x, uio_event.data.mouse.y,
            uio_event.mask);

    // Fire mouse move event.
    dispatch_event(&uio_event);

    return uio_event.reserved & 0x01;
}

bool dispatch_mouse_wheel(struct input_event *const ev, int16_t rotation, uint8_t direction) {
    #ifdef USE_EPOCH_TIME
    uint64_t timestamp = get_unix_timestamp(ev->input_event_sec, ev->input_event_usec);
    #else
    uint64_t timestamp = get_seq_timestamp();
    #endif

    // Reset the click count and previous button.
    click.count = 0;
    click.button = MOUSE_NOBUTTON;

    // Populate mouse wheel event.
    uio_event.time = timestamp;
    uio_event.reserved = 0x00;

    uio_event.type = EVENT_MOUSE_WHEEL;
    uio_event.mask = get_modifiers();

    // evdev has no position for wheel events; query the display server.
    get_pointer_position(&uio_event.data.wheel.x, &uio_event.data.wheel.y);

    /* Linux has no API for the scroll type or lines-per-notch, so we report unit scroll with a
     * fixed delta of 120 (matching the Windows WHEEL_DELTA convention).  Following that convention
     * rotation is a signed multiple of delta, i.e. one wheel detent equals one delta. */
    uio_event.data.wheel.type = WHEEL_UNIT_SCROLL;
    uio_event.data.wheel.delta = 120;

    // Scale in a wider type and clamp to the int16_t rotation field so a large scroll pins
    // to the range limit instead of silently wrapping.
    int32_t scaled_rotation = (int32_t) rotation * uio_event.data.wheel.delta;
    if (scaled_rotation > INT16_MAX) {
        scaled_rotation = INT16_MAX;
        logger(LOG_LEVEL_WARN, "%s [%u]: Wheel rotation overflow detected!\n",
                __FUNCTION__, __LINE__);
    } else if (scaled_rotation < INT16_MIN) {
        scaled_rotation = INT16_MIN;
        logger(LOG_LEVEL_WARN, "%s [%u]: Wheel rotation underflow detected!\n",
                __FUNCTION__, __LINE__);
    }

    uio_event.data.wheel.rotation = (int16_t) scaled_rotation;
    uio_event.data.wheel.direction = direction;

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Mouse wheel %i / %u of type %u in the %u direction at %u, %u.\n",
            __FUNCTION__, __LINE__,
            uio_event.data.wheel.rotation, uio_event.data.wheel.delta,
            uio_event.data.wheel.type, uio_event.data.wheel.direction,
            uio_event.data.wheel.x, uio_event.data.wheel.y);

    // Fire mouse wheel event.
    dispatch_event(&uio_event);

    return uio_event.reserved & 0x01;
}
