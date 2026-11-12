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

#ifndef _included_x11_helper
#define _included_x11_helper

#include <stdbool.h>
#include <X11/Xlib.h>
#include <X11/Xlib-xcb.h>
#include <X11/extensions/Xinerama.h>
#include <xkbcommon/xkbcommon-x11.h>

// X11 is a runtime dependency, not a link-time one: the backing libraries are loaded
// with dlopen() so libuiohook builds and starts on systems without them (e.g. a
// headless or pure-Wayland box).  Each library can be present independently, so callers
// check the capability they need before using the matching function pointers.
enum x11_capability {
    X11_CAP_DISPLAY  = 1 << 0,   // libX11 loaded and the shared connection is open.
    X11_CAP_XKB      = 1 << 1,   // libX11-xcb + libxkbcommon-x11 for server keymap load.
    X11_CAP_XINERAMA = 1 << 2,   // libXinerama for the multi-monitor screen list.
};

// Resolved X11 entry points.  A pointer is non-NULL only when its capability bit is set.
// Invariant: if x11_display() is non-NULL then every libX11 pointer below is valid, since
// they are all resolved from the same handle before the connection is opened.
struct x11_api {
    // libX11
    Status   (*XInitThreads)(void);
    Display *(*XOpenDisplay)(const char *);
    int      (*XCloseDisplay)(Display *);
    Bool     (*XQueryPointer)(Display *, Window, Window *, Window *, int *, int *, int *, int *, unsigned int *);
    int      (*XGetPointerControl)(Display *, int *, int *, int *);
    char    *(*XGetDefault)(Display *, const char *, const char *);
    Bool     (*XkbGetAutoRepeatRate)(Display *, unsigned int, unsigned int *, unsigned int *);
    int      (*XFree)(void *);

    // libX11-xcb
    xcb_connection_t *(*XGetXCBConnection)(Display *);

    // libXinerama
    Bool                (*XineramaIsActive)(Display *);
    XineramaScreenInfo *(*XineramaQueryScreens)(Display *, int *);

    // libxkbcommon-x11
    int32_t (*xkb_x11_get_core_keyboard_device_id)(xcb_connection_t *);
    struct xkb_keymap *(*xkb_x11_keymap_new_from_device)(struct xkb_context *, xcb_connection_t *, int32_t, enum xkb_keymap_compile_flags);
};

extern struct x11_api x11;

// True when every library backing the capability is loaded and usable.
extern bool x11_has(enum x11_capability cap);

/* The shared display connection, or NULL when X11 is unavailable. */
extern Display *x11_display(void);

// dlopen the available X11 libraries, resolve their symbols and open the single shared
// display connection.  Returns UIOHOOK_SUCCESS when at least X11_CAP_DISPLAY is available.
// Safe to call when X is absent; callers must still gate use on x11_has()/x11_display().
extern int load_x11_helper(void);

/* Close the shared connection and dlclose every handle.  Idempotent. */
extern void unload_x11_helper(void);

#endif
