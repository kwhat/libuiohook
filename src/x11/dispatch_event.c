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
// FIXME Should be static void dispatch_event(
void dispatch_event(uiohook_event *const uio_event) {
    if (dispatch != NULL) {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Dispatching event type %u.\n",
                __FUNCTION__, __LINE__, uio_event->type);

        dispatch(uio_event, dispatch_data);
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: No dispatch callback set!\n",
                __FUNCTION__, __LINE__);
    }
}

void dispatch_hook_enabled(XAnyEvent * const x_event) {
    // Initialize native input helper functions.
    load_input_helper();

    // Populate the hook start event.
    uio_event.time = x_event->serial;
    uio_event.reserved = 0x00;

    uio_event.type = EVENT_HOOK_ENABLED;
    uio_event.mask = 0x00;

    // Fire the hook start event.
    dispatch_event(&uio_event);
}

void dispatch_hook_disabled(XAnyEvent * const x_event) {
    // Populate the hook stop event.
    uio_event.time = x_event->serial;
    uio_event.reserved = 0x00;

    uio_event.type = EVENT_HOOK_DISABLED;
    uio_event.mask = 0x00;

    // Fire the hook stop event.
    dispatch_event(&uio_event);

    // Uninitialize native input helper functions.
    unload_input_helper();
}

void dispatch_key_press(XKeyPressedEvent * const x_event) {
    KeySym keysym = 0x00;

    wchar_t surrogate[2] = {};
    size_t count = x_key_event_lookup(x_event, surrogate, sizeof(surrogate) - 1, &keysym);


    uint16_t uiocode = keysym_to_vcode(keysym);

    // FIXME This can happen inside of keysym_to_vcode()
    // TODO VC_ALT_GRAPH MASK?
    if      (uiocode == VC_SHIFT_L)   { set_modifier_mask(MASK_SHIFT_L); }
    else if (uiocode == VC_SHIFT_R)   { set_modifier_mask(MASK_SHIFT_R); }
    else if (uiocode == VC_CONTROL_L) { set_modifier_mask(MASK_CTRL_L);  }
    else if (uiocode == VC_CONTROL_R) { set_modifier_mask(MASK_CTRL_R);  }
    else if (uiocode == VC_ALT_L)     { set_modifier_mask(MASK_ALT_L);   }
    else if (uiocode == VC_ALT_R)     { set_modifier_mask(MASK_ALT_R);   }
    else if (uiocode == VC_META_L)    { set_modifier_mask(MASK_META_L);  }
    else if (uiocode == VC_META_R)    { set_modifier_mask(MASK_META_R);  }

    // FIXME We shouldn't be doing this on each key press, do something similar to above.
    //initialize_locks();




    // Populate key pressed event.
    uio_event.time = x_event->serial;
    uio_event.reserved = 0x00;

    uio_event.type = EVENT_KEY_PRESSED;
    uio_event.mask = get_modifiers();

    uio_event.data.keyboard.keycode = uiocode;
    uio_event.data.keyboard.rawcode = keysym;
    uio_event.data.keyboard.keychar = CHAR_UNDEFINED;

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Key %#X pressed. (%#X)\n",
            __FUNCTION__, __LINE__,
            uio_event.data.keyboard.keycode, uio_event.data.keyboard.rawcode);

    // Fire key pressed event.
    dispatch_event(&uio_event);

    // If the pressed event was not consumed and we got a char in the buffer.
    if (uio_event.reserved ^ 0x01) {
        for (unsigned int i = 0; i < count; i++) {
            // Populate key typed event.
            uio_event.time = x_event->serial;
            uio_event.reserved = 0x00;

            uio_event.type = EVENT_KEY_TYPED;
            uio_event.mask = get_modifiers();

            uio_event.data.keyboard.keycode = VC_UNDEFINED;
            uio_event.data.keyboard.rawcode = keysym;
            uio_event.data.keyboard.keychar = surrogate[i];

            logger(LOG_LEVEL_DEBUG, "%s [%u]: Key %#X typed. (%lc)\n",
                    __FUNCTION__, __LINE__,
                    uio_event.data.keyboard.keycode,
                    uio_event.data.keyboard.keychar);

            // Fire key typed event.
            dispatch_event(&uio_event);
        }
    }
}

void dispatch_key_release(XKeyReleasedEvent * const x_event) {
    // The X11 KeyCode associated with this event.
    KeySym keysym = 0x00;

    x_key_event_lookup(x_event, NULL, 0, &keysym);

    uint16_t uiocode = keysym_to_vcode(keysym);

    // FIXME This can happen inside of keycode_to_scancode()
    if      (uiocode == VC_SHIFT_L)   { unset_modifier_mask(MASK_SHIFT_L); }
    else if (uiocode == VC_SHIFT_R)   { unset_modifier_mask(MASK_SHIFT_R); }
    else if (uiocode == VC_CONTROL_L) { unset_modifier_mask(MASK_CTRL_L);  }
    else if (uiocode == VC_CONTROL_R) { unset_modifier_mask(MASK_CTRL_R);  }
    else if (uiocode == VC_ALT_L)     { unset_modifier_mask(MASK_ALT_L);   }
    else if (uiocode == VC_ALT_R)     { unset_modifier_mask(MASK_ALT_R);   }
    else if (uiocode == VC_META_L)    { unset_modifier_mask(MASK_META_L);  }
    else if (uiocode == VC_META_R)    { unset_modifier_mask(MASK_META_R);  }

    // FIXME We shouldn't be doing this on each key release.
    //initialize_locks();


    // Populate key released event.
    uio_event.time = x_event->serial;
    uio_event.reserved = 0x00;

    uio_event.type = EVENT_KEY_RELEASED;
    uio_event.mask = get_modifiers();

    uio_event.data.keyboard.keycode = uiocode;
    uio_event.data.keyboard.rawcode = keysym;
    uio_event.data.keyboard.keychar = CHAR_UNDEFINED;

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Key %#X released. (%#X)\n",
            __FUNCTION__, __LINE__,
            uio_event.data.keyboard.keycode, uio_event.data.keyboard.rawcode);

    // Fire key released event.
    dispatch_event(&uio_event);
}

static void dispatch_mouse_wheel_rotated(XButtonEvent * const x_event) {
    // Reset the click count and previous button.
    click.count = 0;
    click.button = MOUSE_NOBUTTON;

    // Populate mouse wheel event.
    uio_event.time = x_event->serial;
    uio_event.reserved = 0x00;

    uio_event.type = EVENT_MOUSE_WHEEL;
    uio_event.mask = get_modifiers();

    uio_event.data.wheel.x = x_event->x_root;
    uio_event.data.wheel.y = x_event->y_root;

    #if defined(USE_XINERAMA) || defined(USE_XRANDR)
    uint8_t count;
    screen_data *screens = hook_create_screen_info(&count);
    if (count > 1) {
        uio_event.data.wheel.x -= screens[0].x;
        uio_event.data.wheel.y -= screens[0].y;
    }

    if (screens != NULL) {
        free(screens);
    }
    #endif

    /* X11 does not have an API call for acquiring the mouse scroll type. This maybe part of the XInput2 (XI2)
     * extension but I will wont know until it is available on my platform. For the time being we will just use the
     * unit scroll value. */
    uio_event.data.wheel.type = WHEEL_UNIT_SCROLL;

    /* Some scroll wheel properties are available via the new XInput2 (XI2) extension. Unfortunately the extension is
     * not available on my development platform at this time. For the time being we will just use the Windows default
     * value of 3. */
    uio_event.data.wheel.delta = 100;
    if (x_event->button == WheelDown || x_event->button == WheelLeft) {
        // Wheel Rotated Up and Away.
        uio_event.data.wheel.rotation = -3 * uio_event.data.wheel.delta;
    } else { // event.button == WheelUp || event.button == WheelRight
        // Wheel Rotated Down and Towards.
        uio_event.data.wheel.rotation = 3 * uio_event.data.wheel.delta;
    }

    if (x_event->button == WheelUp || x_event->button == WheelDown) {
        // Wheel Rotated Up or Down.
        uio_event.data.wheel.direction = WHEEL_VERTICAL_DIRECTION;
    } else { // event.button == WheelLeft || event.button == WheelRight
        // Wheel Rotated Left or Right.
        uio_event.data.wheel.direction = WHEEL_HORIZONTAL_DIRECTION;
    }

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Mouse wheel %i / %u of type %u in the %u direction at %u, %u.\n",
            __FUNCTION__, __LINE__,
            uio_event.data.wheel.rotation, uio_event.data.wheel.delta,
            uio_event.data.wheel.type, uio_event.data.wheel.direction,
            uio_event.data.wheel.x, uio_event.data.wheel.y);

    // Fire mouse wheel event.
    dispatch_event(&uio_event);
}

static void dispatch_mouse_button_pressed(XButtonPressedEvent * const x_event) {
    switch (x_event->button) {
        case Button1:
            x_event->button = MOUSE_BUTTON1;
            set_modifier_mask(MASK_BUTTON1);
            break;

        case Button2:
            x_event->button = MOUSE_BUTTON2;
            set_modifier_mask(MASK_BUTTON2);
            break;

        case Button3:
            x_event->button = MOUSE_BUTTON3;
            set_modifier_mask(MASK_BUTTON3);
            break;

        case XButton1:
            x_event->button = MOUSE_BUTTON4;
            set_modifier_mask(MASK_BUTTON5);
            break;

        case XButton2:
            x_event->button = MOUSE_BUTTON5;
            set_modifier_mask(MASK_BUTTON5);
            break;

        default:
            if (x_event->button > XButton2) {
                // Do not set modifier masks past button MASK_BUTTON5.
                x_event->button = MOUSE_BUTTON5 + (x_event->button - XButton2);
            } else {
                // Something screwed up, default to MOUSE_NOBUTTON
                x_event->button = MOUSE_NOBUTTON;
            }
    }

    // Track the number of clicks, the button must match the previous button.
    if (x_event->button == click.button && x_event->serial - click.time <= hook_get_multi_click_time()) {
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
        click.button = x_event->button;
    }

    // Save this events time to calculate multi-clicks.
    click.time = x_event->serial;

    // Populate mouse pressed event.
    uio_event.time = x_event->serial;
    uio_event.reserved = 0x00;

    uio_event.type = EVENT_MOUSE_PRESSED;
    uio_event.mask = get_modifiers();

    uio_event.data.mouse.button = x_event->button;
    uio_event.data.mouse.clicks = click.count;
    uio_event.data.mouse.x = x_event->x_root;
    uio_event.data.mouse.y = x_event->y_root;

    #if defined(USE_XINERAMA) || defined(USE_XRANDR)
    // FIXME There is something still broken about this.
    uint8_t count;
    screen_data *screens = hook_create_screen_info(&count);
    if (count > 1) {
        uio_event.data.mouse.x -= screens[0].x;
        uio_event.data.mouse.y -= screens[0].y;
    }

    if (screens != NULL) {
        free(screens);
    }
    #endif

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Button %u  pressed %u time(s). (%u, %u)\n",
            __FUNCTION__, __LINE__,
            uio_event.data.mouse.button, uio_event.data.mouse.clicks,
            uio_event.data.mouse.x, uio_event.data.mouse.y);

    // Fire mouse pressed event.
    dispatch_event(&uio_event);
}

void dispatch_mouse_press(XButtonEvent * const x_event) {
    x_event->button = button_map_lookup(x_event->button);

    switch (x_event->button) {
        case WheelUp:
        case WheelDown:
        case WheelLeft:
        case WheelRight:
            dispatch_mouse_wheel_rotated((XButtonEvent *) x_event);
            break;

        default:
            dispatch_mouse_button_pressed((XButtonPressedEvent *) x_event);
            break;
    }
}

static void dispatch_mouse_button_released(XButtonReleasedEvent * const x_event) {
    switch (button_map_lookup(x_event->button)) {
        case Button1:
            x_event->button = MOUSE_BUTTON1;
            unset_modifier_mask(MASK_BUTTON1);
            break;

        case Button2:
            x_event->button = MOUSE_BUTTON2;
            unset_modifier_mask(MASK_BUTTON2);
            break;

        case Button3:
            x_event->button = MOUSE_BUTTON3;
            unset_modifier_mask(MASK_BUTTON3);
            break;

        case XButton1:
            x_event->button = MOUSE_BUTTON4;
            unset_modifier_mask(MASK_BUTTON5);
            break;

        case XButton2:
            x_event->button = MOUSE_BUTTON5;
            unset_modifier_mask(MASK_BUTTON5);
            break;

        default:
            if (x_event->button > XButton2) {
                // Do not set modifier masks past button MASK_BUTTON5.
                x_event->button = MOUSE_BUTTON5 + (x_event->button - XButton2);
            } else {
                // Something screwed up, default to MOUSE_NOBUTTON
                x_event->button = MOUSE_NOBUTTON;
            }
    }

    // Populate mouse released event.
    uio_event.time = x_event->serial;
    uio_event.reserved = 0x00;

    uio_event.type = EVENT_MOUSE_RELEASED;
    uio_event.mask = get_modifiers();

    uio_event.data.mouse.button = x_event->button;
    uio_event.data.mouse.clicks = click.count;
    uio_event.data.mouse.x = x_event->x_root;
    uio_event.data.mouse.y = x_event->y_root;

    #if defined(USE_XINERAMA) || defined(USE_XRANDR)
    uint8_t count;
    screen_data *screens = hook_create_screen_info(&count);
    if (count > 1) {
        uio_event.data.mouse.x -= screens[0].x;
        uio_event.data.mouse.y -= screens[0].y;
    }

    if (screens != NULL) {
        free(screens);
    }
    #endif

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Button %u released %u time(s). (%u, %u)\n",
            __FUNCTION__, __LINE__,
            uio_event.data.mouse.button, uio_event.data.mouse.clicks,
            uio_event.data.mouse.x, uio_event.data.mouse.y);

    // Fire mouse released event.
    dispatch_event(&uio_event);
}

static void dispatch_mouse_button_clicked(XButtonEvent * const x_event) {
    // Populate mouse clicked event.
    uio_event.time = x_event->serial;
    uio_event.reserved = 0x00;

    uio_event.type = EVENT_MOUSE_CLICKED;
    uio_event.mask = get_modifiers();

    uio_event.data.mouse.button = x_event->button;
    uio_event.data.mouse.clicks = click.count;
    uio_event.data.mouse.x = x_event->x_root;
    uio_event.data.mouse.y = x_event->y_root;

    #if defined(USE_XINERAMA) || defined(USE_XRANDR)
    uint8_t count;
    screen_data *screens = hook_create_screen_info(&count);
    if (count > 1) {
        uio_event.data.mouse.x -= screens[0].x;
        uio_event.data.mouse.y -= screens[0].y;
    }

    if (screens != NULL) {
        free(screens);
    }
    #endif

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Button %u clicked %u time(s). (%u, %u)\n",
            __FUNCTION__, __LINE__,
            uio_event.data.mouse.button, uio_event.data.mouse.clicks,
            uio_event.data.mouse.x, uio_event.data.mouse.y);

    // Fire mouse clicked event.
    dispatch_event(&uio_event);
}

void dispatch_mouse_release(XButtonEvent * const x_event) {
    x_event->button = button_map_lookup(x_event->button);

    switch (x_event->button) {
        case WheelUp:
        case WheelDown:
        case WheelLeft:
        case WheelRight:
            return;
    }

    dispatch_mouse_button_released((XButtonReleasedEvent *) x_event);

    // If the pressed event was not consumed...
    bool is_dragged = (bool) (get_modifiers() & 0x1F00);
    if (uio_event.reserved ^ 0x01 && !is_dragged) {
        dispatch_mouse_button_clicked(x_event);
    }

    // Reset the number of clicks.
    // FIXME
    if (x_event->button == click.button && (x_event->serial - click.time) > hook_get_multi_click_time()) {
        // Reset the click count.
        click.count = 0;
    }
}

void dispatch_mouse_move(XMotionEvent * const x_event) {
    // Reset the click count.
    if (click.count != 0 && x_event->serial - click.time > hook_get_multi_click_time()) {
        click.count = 0;
    }

    // Populate mouse move event.
    uio_event.time = x_event->serial;
    uio_event.reserved = 0x00;

    uio_event.mask = get_modifiers();

    // Check the upper half of virtual modifiers for non-zero values and set the mouse
    // dragged flag.  The last 3 bits are reserved for lock masks.
    bool is_dragged = (bool) (get_modifiers() & 0x1F00);
    if (is_dragged) {
        // Create Mouse Dragged event.
        uio_event.type = EVENT_MOUSE_DRAGGED;
    } else {
        // Create a Mouse Moved event.
        uio_event.type = EVENT_MOUSE_MOVED;
    }

    uio_event.data.mouse.button = MOUSE_NOBUTTON;
    uio_event.data.mouse.clicks = click.count;
    uio_event.data.mouse.x = x_event->x_root;
    uio_event.data.mouse.y = x_event->y_root;

    #if defined(USE_XINERAMA) || defined(USE_XRANDR)
    uint8_t count;
    screen_data *screens = hook_create_screen_info(&count);
    if (count > 1) {
        uio_event.data.mouse.x -= screens[0].x;
        uio_event.data.mouse.y -= screens[0].y;
    }

    if (screens != NULL) {
        free(screens);
    }
    #endif

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Mouse %s to %i, %i. (%#X)\n",
            __FUNCTION__, __LINE__,
            is_dragged ? "dragged" : "moved",
            uio_event.data.mouse.x, uio_event.data.mouse.y, uio_event.mask);

    // Fire mouse move event.
    dispatch_event(&uio_event);
}
