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

#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <linux/input.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <uiohook.h>

#include "input_helper.h"
#include "logger.h"

// Dedicated virtual device used to inject synthetic events.  Created lazily on the first
// hook_post_event() call so hook-only users never spawn an unused virtual input device.
// Posting needs no X11: events are written straight to uinput.
static struct libevdev_uinput *post_uinput = NULL;

// Compute the exclusive width/height of the whole screen layout so the absolute pointer
// axes can be sized to match the coordinate space callers post in.  Falls back to a full
// 16-bit range when the layout is unavailable (e.g. no X11 to enumerate monitors).
static void post_screen_extents(int *width, int *height) {
    *width = 0;
    *height = 0;

    unsigned char count = 0;
    screen_data *screens = hook_create_screen_info(&count);
    if (screens != NULL) {
        for (unsigned char i = 0; i < count; i++) {
            int right = screens[i].x + screens[i].width;
            int bottom = screens[i].y + screens[i].height;
            if (right > *width) { *width = right; }
            if (bottom > *height) { *height = bottom; }
        }

        free(screens);
    }

    if (*width <= 0) { *width = UINT16_MAX; }
    if (*height <= 0) { *height = UINT16_MAX; }
}

// Build the virtual device: keyboard keys and mouse buttons (EV_KEY), a scroll wheel
// (EV_REL) and an absolute pointer (EV_ABS) sized to the screen so motion posts land on
// their target coordinate rather than accumulating deltas.
static int create_post_device() {
    struct libevdev *evdev = libevdev_new();
    if (evdev == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to allocate evdev for the post device!\n",
                __FUNCTION__, __LINE__);
        return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }

    libevdev_set_name(evdev, "libuiohook");

    // Keyboard keys and mouse buttons share the EV_KEY type.
    libevdev_enable_event_type(evdev, EV_KEY);
    for (unsigned int code = KEY_ESC; code < KEY_MAX; code++) {
        libevdev_enable_event_code(evdev, EV_KEY, code, NULL);
    }
    for (unsigned int code = BTN_LEFT; code <= BTN_TASK; code++) {
        libevdev_enable_event_code(evdev, EV_KEY, code, NULL);
    }

    // Scroll wheel.
    libevdev_enable_event_type(evdev, EV_REL);
    libevdev_enable_event_code(evdev, EV_REL, REL_WHEEL, NULL);
    libevdev_enable_event_code(evdev, EV_REL, REL_HWHEEL, NULL);

    // Absolute pointer axes sized to the screen bounds.
    int width, height;
    post_screen_extents(&width, &height);

    struct input_absinfo abs_x = { .minimum = 0, .maximum = width - 1 };
    struct input_absinfo abs_y = { .minimum = 0, .maximum = height - 1 };
    libevdev_enable_event_type(evdev, EV_ABS);
    libevdev_enable_event_code(evdev, EV_ABS, ABS_X, &abs_x);
    libevdev_enable_event_code(evdev, EV_ABS, ABS_Y, &abs_y);

    int err = libevdev_uinput_create_from_device(evdev, LIBEVDEV_UINPUT_OPEN_MANAGED, &post_uinput);

    // The uinput device does not retain the template, so it can be freed either way.
    libevdev_free(evdev);

    if (err != 0) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to create post uinput device! (%d)\n",
                __FUNCTION__, __LINE__, err);
        post_uinput = NULL;
        return UIOHOOK_FAILURE;
    }

    return UIOHOOK_SUCCESS;
}

static int ensure_post_device() {
    if (post_uinput != NULL) {
        return UIOHOOK_SUCCESS;
    }

    return create_post_device();
}

static uint16_t uiocode_to_button(uint16_t button) {
    switch (button) {
        case MOUSE_BUTTON1:
            return BTN_LEFT;
        case MOUSE_BUTTON2:
            return BTN_RIGHT;
        case MOUSE_BUTTON3:
            return BTN_MIDDLE;
        case MOUSE_BUTTON4:
            return BTN_SIDE;
        case MOUSE_BUTTON5:
            return BTN_EXTRA;
        default:
            return 0;
    }
}

static int post_key_event(uiohook_event * const event) {
    // Prefer the rawcode carried on a captured event (the XKB key code) so round-tripping
    // needs no keymap lookup; fall back to the virtual-code mapping for crafted events.
    xkb_keycode_t keycode = event->data.keyboard.rawcode;
    if (keycode == 0) {
        keycode = uiocode_to_keycode(event->data.keyboard.keycode);
    }

    uint16_t code = keycode_to_event(keycode);
    if (code == 0) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Unable to resolve scancode for keycode %u!\n",
                __FUNCTION__, __LINE__, event->data.keyboard.keycode);
        return UIOHOOK_FAILURE;
    }

    int value = event->type == EVENT_KEY_PRESSED ? 1 : 0;
    libevdev_uinput_write_event(post_uinput, EV_KEY, code, value);

    return UIOHOOK_SUCCESS;
}

static int post_mouse_button_event(uiohook_event * const event) {
    uint16_t code = uiocode_to_button(event->data.mouse.button);
    if (code == 0) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Invalid button specified for mouse event! (%u)\n",
                __FUNCTION__, __LINE__, event->data.mouse.button);
        return UIOHOOK_FAILURE;
    }

    int value = event->type == EVENT_MOUSE_PRESSED ? 1 : 0;
    libevdev_uinput_write_event(post_uinput, EV_KEY, code, value);

    return UIOHOOK_SUCCESS;
}

static int post_mouse_wheel_event(uiohook_event * const event) {
    unsigned int code = event->data.wheel.direction == WHEEL_HORIZONTAL_DIRECTION
            ? REL_HWHEEL : REL_WHEEL;

    // Post one notch per event in the rotation's direction.
    int value = 0;
    if (event->data.wheel.rotation > 0) {
        value = 1;
    } else if (event->data.wheel.rotation < 0) {
        value = -1;
    }

    libevdev_uinput_write_event(post_uinput, EV_REL, code, value);

    return UIOHOOK_SUCCESS;
}

static int post_mouse_motion_event(uiohook_event * const event) {
    // Absolute axes: post the target coordinate directly.
    libevdev_uinput_write_event(post_uinput, EV_ABS, ABS_X, event->data.mouse.x);
    libevdev_uinput_write_event(post_uinput, EV_ABS, ABS_Y, event->data.mouse.y);

    return UIOHOOK_SUCCESS;
}

UIOHOOK_API int hook_post_event(uiohook_event * const event) {
    if (ensure_post_device() != UIOHOOK_SUCCESS) {
        return UIOHOOK_FAILURE;
    }

    int status = UIOHOOK_FAILURE;
    switch (event->type) {
        case EVENT_KEY_PRESSED:
        case EVENT_KEY_RELEASED:
            status = post_key_event(event);
            break;

        case EVENT_MOUSE_PRESSED:
        case EVENT_MOUSE_RELEASED:
            status = post_mouse_button_event(event);
            break;

        case EVENT_MOUSE_WHEEL:
            status = post_mouse_wheel_event(event);
            break;

        case EVENT_MOUSE_MOVED:
        case EVENT_MOUSE_DRAGGED:
            status = post_mouse_motion_event(event);
            break;

        default:
            logger(LOG_LEVEL_WARN, "%s [%u]: Ignoring post event type %#X\n",
                    __FUNCTION__, __LINE__, event->type);
            status = UIOHOOK_FAILURE;
    }

    // Terminate the event frame so the kernel delivers it.
    if (status == UIOHOOK_SUCCESS) {
        libevdev_uinput_write_event(post_uinput, EV_SYN, SYN_REPORT, 0);
    }

    return status;
}

// Destroy the lazily created post device at library unload.
__attribute__ ((destructor))
static void unload_post_event() {
    if (post_uinput != NULL) {
        libevdev_uinput_destroy(post_uinput);
        post_uinput = NULL;
    }
}
