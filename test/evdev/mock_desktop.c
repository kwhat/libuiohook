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

#include <stdlib.h>
#include <string.h>

#include "mock_desktop.h"
#include "x11_helper.h"
#include "wayland_helper.h"

struct mock_desktop_state mock_desktop;
struct x11_api x11;
static Screen mock_screen;
static _XPrivDisplay mock_display;

static Bool mock_repeat(Display *display, unsigned int device, unsigned int *delay, unsigned int *rate) {
    (void) display;
    (void) device;
    mock_desktop.x11_calls++;
    *delay = mock_desktop.x11_delay;
    *rate = mock_desktop.x11_rate;
    return mock_desktop.repeat_success;
}

static int mock_pointer(Display *display, int *numerator, int *denominator, int *threshold) {
    (void) display;
    mock_desktop.x11_calls++;
    *numerator = mock_desktop.numerator;
    *denominator = mock_desktop.denominator;
    *threshold = mock_desktop.threshold;
    return 1;
}

static char * mock_default(Display *display, const char *program, const char *option) {
    (void) display;
    (void) option;
    mock_desktop.x11_calls++;
    return (char *) (strcmp(program, "*") == 0
            ? mock_desktop.click_time : mock_desktop.fallback_click_time);
}

static Bool mock_xinerama_active(Display *display) {
    (void) display;
    return True;
}

static XineramaScreenInfo * mock_xinerama_screens(Display *display, int *count) {
    (void) display;
    *count = 2;
    XineramaScreenInfo *screens = calloc(2, sizeof(*screens));
    if (screens != NULL) {
        screens[0] = (XineramaScreenInfo) { 0, -1280, 0, 1280, 1024 };
        screens[1] = (XineramaScreenInfo) { 1, 0, 0, 1920, 1080 };
    }
    return screens;
}

static int mock_free(void *data) {
    free(data);
    return 0;
}

void mock_desktop_reset(void) {
    unsetenv("WAYLAND_DISPLAY");
    mock_desktop = (struct mock_desktop_state) {
        .repeat_success = true,
        .wayland_rate = 25,
        .wayland_delay = 400,
        .wayland_status = UIOHOOK_SUCCESS,
        .x11_rate = 33,
        .x11_delay = 600,
        .numerator = 3,
        .denominator = 2,
        .threshold = 4
    };
    x11 = (struct x11_api) {
        .XkbGetAutoRepeatRate = mock_repeat,
        .XGetPointerControl = mock_pointer,
        .XGetDefault = mock_default,
        .XineramaIsActive = mock_xinerama_active,
        .XineramaQueryScreens = mock_xinerama_screens,
        .XFree = mock_free
    };
}

Display * x11_display(void) {
    if (!mock_desktop.x11_available) {
        return NULL;
    }
    if (mock_display == NULL) {
        mock_display = calloc(1, sizeof(*mock_display));
        if (mock_display == NULL) {
            abort();
        }
        mock_screen.width = 1920;
        mock_screen.height = 1080;
        mock_display->screens = &mock_screen;
        mock_display->default_screen = 0;
    }
    return (Display *) mock_display;
}

bool x11_has(enum x11_capability capability) {
    return capability == X11_CAP_XINERAMA && mock_desktop.xinerama_available;
}

int load_x11_helper(void) {
    // The real property object's constructor calls this mock, never XOpenDisplay.
    return UIOHOOK_SUCCESS;
}

void unload_x11_helper(void) {
    free(mock_display);
    mock_display = NULL;
}

int wayland_get_repeat_info(int32_t *rate, int32_t *delay) {
    mock_desktop.wayland_calls++;
    *rate = mock_desktop.wayland_rate;
    *delay = mock_desktop.wayland_delay;
    return mock_desktop.wayland_status;
}

screen_data * wayland_create_screen_info(unsigned char *count) {
    mock_desktop.wayland_calls++;
    *count = 0;
    if (!mock_desktop.wayland_screens) {
        return NULL;
    }
    screen_data *screens = calloc(1, sizeof(*screens));
    if (screens != NULL) {
        *count = 1;
        screens[0] = (screen_data) { 1, 0, 0, 1920, 1080 };
    }
    return screens;
}

char * wayland_get_keymap(int32_t *rate, int32_t *delay) {
    (void) rate;
    (void) delay;
    // Self-contained fixture: no compositor, host layout, or XKB rules files.
    return strdup("xkb_keymap { "
            "xkb_keycodes { minimum=8; maximum=255; <AC01>=38; }; "
            "xkb_types { type \"ONE_LEVEL\" { modifiers=None; map[None]=Level1; }; }; "
            "xkb_compatibility { }; "
            "xkb_symbols { key <AC01> { type=\"ONE_LEVEL\", [ a ] }; }; "
            "};");
}

int wayland_query_pointer_position(int16_t *x, int16_t *y) {
    *x = 100;
    *y = 80;
    return UIOHOOK_SUCCESS;
}
