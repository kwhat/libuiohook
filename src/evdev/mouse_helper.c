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
#include <stdbool.h>
#include <uiohook.h>

#include "mouse_helper.h"
#include "logger.h"
#include "numeric_helper.h"
#include "wayland_helper.h"
#include "x11_helper.h"

// Motion after a button press suppresses the synthesized click on release.
static bool mouse_dragged = false;

bool is_mouse_dragged(void) {
    return mouse_dragged;
}

void set_mouse_dragged(bool dragged) {
    mouse_dragged = dragged;
}

// Self-tracked absolute pointer position.  evdev only reports relative deltas, so we
// accumulate them into an absolute position and clamp it to the display bounds.  The
// clamp doubles as a free re-anchor: the compositor pins the cursor at every screen edge,
// so clamping our tracked value to the same bounds re-syncs an axis whenever the pointer
// is driven into an edge.  Ground-truth seed / resync providers feed set_pointer_position.
static int tracked_x = 0, tracked_y = 0;

// Cached pointer bounding box (inclusive) derived from the monitor layout.
static bool bounds_valid = false;
static int bounds_min_x = 0, bounds_min_y = 0;
static int bounds_max_x = 0, bounds_max_y = 0;

static int clamp_int(int value, int lo, int hi) {
    if (value < lo) { return lo; }
    if (value > hi) { return hi; }
    return value;
}

// Recompute the pointer bounding box across all monitors from the current layout.
static void refresh_pointer_bounds() {
    unsigned char count = 0;
    screen_data *screens = hook_create_screen_info(&count);

    if (screens != NULL && count > 0) {
        int min_x = screens[0].x;
        int min_y = screens[0].y;
        int max_x = screens[0].x + screens[0].width;
        int max_y = screens[0].y + screens[0].height;

        for (unsigned char i = 1; i < count; i++) {
            if (screens[i].x < min_x) { min_x = screens[i].x; }
            if (screens[i].y < min_y) { min_y = screens[i].y; }
            if (screens[i].x + screens[i].width  > max_x) { max_x = screens[i].x + screens[i].width;  }
            if (screens[i].y + screens[i].height > max_y) { max_y = screens[i].y + screens[i].height; }
        }

        bounds_min_x = min_x;
        bounds_min_y = min_y;
        bounds_max_x = max_x - 1; // last addressable pixel
        bounds_max_y = max_y - 1;
        bounds_valid = true;
    } else {
        bounds_valid = false;
        logger(LOG_LEVEL_WARN, "%s [%u]: Unable to determine display bounds for pointer tracking!\n",
                __FUNCTION__, __LINE__);
    }

    if (screens != NULL) {
        free(screens);
    }
}

// Query the current absolute pointer position.  Used only to seed the self-tracker at
// startup; there is no per-event query.
int query_pointer_position(int16_t *x, int16_t *y) {
    // Favor the Wayland layer-shell probe: XWayland's XQueryPointer only knows the cursor
    // while it is over X surfaces, so it is unreliable under a Wayland session.
    if (getenv("WAYLAND_DISPLAY") != NULL) {
        if (wayland_query_pointer_position(x, y) == UIOHOOK_SUCCESS) {
            return UIOHOOK_SUCCESS;
        }

        logger(LOG_LEVEL_WARN, "%s [%u]: Wayland pointer probe failed, trying X11.\n",
                __FUNCTION__, __LINE__);
    }

    Display *display = x11_display();
    if (display == NULL) {
        logger(LOG_LEVEL_WARN, "%s [%u]: XDisplay is unavailable!\n",
                __FUNCTION__, __LINE__);
        return UIOHOOK_ERROR_X_OPEN_DISPLAY;
    }

    Window root, child;
    int root_x = 0, root_y = 0;
    int win_x = 0, win_y = 0;
    unsigned int mask = 0;

    // Query relative to the default root window; root_x/root_y come back in
    // root (global) coordinates regardless of which screen the pointer is on.
    if (!x11.XQueryPointer(display, DefaultRootWindow(display), &root, &child, &root_x, &root_y, &win_x, &win_y, &mask)) {
        logger(LOG_LEVEL_WARN, "%s [%u]: XQueryPointer failed!\n",
                __FUNCTION__, __LINE__);
        return UIOHOOK_FAILURE;
    }

    *x = (int16_t) root_x;
    *y = (int16_t) root_y;

    return UIOHOOK_SUCCESS;
}

// Initialize pointer tracking: compute bounds and seed from the X server, falling back
// to the top-left corner if the pointer position cannot be queried.
// TODO Replace the fallback with a uinput corner-slam so the real cursor matches on startup.
void init_pointer_tracking() {
    refresh_pointer_bounds();

    int16_t seed_x, seed_y;
    if (query_pointer_position(&seed_x, &seed_y) == UIOHOOK_SUCCESS) {
        set_pointer_position(seed_x, seed_y);
    } else {
        tracked_x = bounds_min_x;
        tracked_y = bounds_min_y;
    }
}

// Apply a relative motion delta to the tracked absolute pointer position.
void track_pointer_delta(int dx, int dy) {
    tracked_x += dx;
    tracked_y += dy;

    if (bounds_valid) {
        tracked_x = clamp_int(tracked_x, bounds_min_x, bounds_max_x);
        tracked_y = clamp_int(tracked_y, bounds_min_y, bounds_max_y);
    }
}

// Anchor the tracked position to a known absolute coordinate (seed / ground-truth resync).
void set_pointer_position(int16_t x, int16_t y) {
    tracked_x = x;
    tracked_y = y;

    if (bounds_valid) {
        tracked_x = clamp_int(tracked_x, bounds_min_x, bounds_max_x);
        tracked_y = clamp_int(tracked_y, bounds_min_y, bounds_max_y);
    }
}

// Return the current tracked absolute pointer position.
void get_pointer_position(int16_t *x, int16_t *y) {
    *x = clamp_int16(tracked_x);
    *y = clamp_int16(tracked_y);
}
