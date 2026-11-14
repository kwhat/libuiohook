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

#ifndef _included_mock_desktop
#define _included_mock_desktop

#include <stdbool.h>
#include <stdint.h>
#include <uiohook.h>

struct mock_desktop_state {
    bool x11_available;
    bool xinerama_available;
    bool repeat_success;
    int32_t wayland_rate, wayland_delay;
    int wayland_status;
    unsigned int x11_rate, x11_delay;
    int numerator, denominator, threshold;
    const char *click_time;
    const char *fallback_click_time;
    bool wayland_screens;
    int x11_calls, wayland_calls;
};

extern struct mock_desktop_state mock_desktop;

/* Reset mock desktop configuration, call counters, and X11 provider functions, and unset WAYLAND_DISPLAY. */
extern void mock_desktop_reset(void);

#endif
