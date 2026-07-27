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

#include <dlfcn.h>
#include <string.h>
#include <uiohook.h>

#include "logger.h"
#include "x11_helper.h"

struct x11_api x11 = { 0 };

static void *lib_x11 = NULL;
static void *lib_x11_xcb = NULL;
static void *lib_xkb_x11 = NULL;
static void *lib_xinerama = NULL;

static Display *disp = NULL;
static unsigned int capabilities = 0;

// Resolve one symbol into its function-pointer slot.  target is the address of the x11.*
// pointer cast to void **, which also avoids the ISO C warning about converting object
// pointers to function pointers.
static void x11_resolve(void **target, void *handle, const char *name) {
    *target = dlsym(handle, name);
    if (*target == NULL) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Failed to resolve %s: %s\n",
                __FUNCTION__, __LINE__, name, dlerror());
    }
}


bool x11_has(enum x11_capability cap) {
    return (capabilities & cap) == (unsigned int) cap;
}

Display *x11_display(void) {
    return disp;
}

int load_x11_helper(void) {
    // libX11: display connection, pointer and keycode queries.
    lib_x11 = dlopen("libX11.so.6", RTLD_NOW | RTLD_GLOBAL);
    if (lib_x11 == NULL) {
        logger(LOG_LEVEL_INFO, "%s [%u]: libX11 is unavailable: %s\n",
                __FUNCTION__, __LINE__, dlerror());
        return UIOHOOK_ERROR_X_OPEN_DISPLAY;
    }

    x11_resolve((void **) &x11.XInitThreads,         lib_x11, "XInitThreads");
    x11_resolve((void **) &x11.XOpenDisplay,         lib_x11, "XOpenDisplay");
    x11_resolve((void **) &x11.XCloseDisplay,        lib_x11, "XCloseDisplay");
    x11_resolve((void **) &x11.XQueryPointer,        lib_x11, "XQueryPointer");
    x11_resolve((void **) &x11.XGetPointerControl,   lib_x11, "XGetPointerControl");
    x11_resolve((void **) &x11.XGetDefault,          lib_x11, "XGetDefault");
    x11_resolve((void **) &x11.XkbGetAutoRepeatRate, lib_x11, "XkbGetAutoRepeatRate");
    x11_resolve((void **) &x11.XFree,                lib_x11, "XFree");

    // The shared connection is read from both the hook thread and API callers, so put
    // Xlib into thread-safe mode before opening it.  Must precede any other Xlib call.
    if (x11.XInitThreads != NULL) {
        x11.XInitThreads();
    }

    // Open the single shared connection every query-style caller uses.
    if (x11.XOpenDisplay != NULL) {
        disp = x11.XOpenDisplay(NULL);
    }

    if (disp == NULL) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Failed to open shared X11 display!\n",
                __FUNCTION__, __LINE__);
        unload_x11_helper();
        return UIOHOOK_ERROR_X_OPEN_DISPLAY;
    }

    capabilities |= X11_CAP_DISPLAY;

    // libX11-xcb + libxkbcommon-x11: load the server keymap.  Optional; the keysym
    // translation still works from the compose/keymap state without it.
    lib_x11_xcb = dlopen("libX11-xcb.so.1", RTLD_NOW | RTLD_GLOBAL);
    lib_xkb_x11 = dlopen("libxkbcommon-x11.so.0", RTLD_NOW | RTLD_GLOBAL);
    if (lib_x11_xcb != NULL && lib_xkb_x11 != NULL) {
        x11_resolve((void **) &x11.XGetXCBConnection,                   lib_x11_xcb, "XGetXCBConnection");
        x11_resolve((void **) &x11.xkb_x11_get_core_keyboard_device_id, lib_xkb_x11, "xkb_x11_get_core_keyboard_device_id");
        x11_resolve((void **) &x11.xkb_x11_keymap_new_from_device,      lib_xkb_x11, "xkb_x11_keymap_new_from_device");

        if (x11.XGetXCBConnection != NULL
                && x11.xkb_x11_get_core_keyboard_device_id != NULL
                && x11.xkb_x11_keymap_new_from_device != NULL) {
            capabilities |= X11_CAP_XKB;
        }
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: libxkbcommon-x11 is unavailable, server keymap disabled!\n",
                __FUNCTION__, __LINE__);
    }

    // libXinerama: multi-monitor screen list.  Optional; callers fall back to the single
    // default screen when it is absent.
    lib_xinerama = dlopen("libXinerama.so.1", RTLD_NOW | RTLD_GLOBAL);
    if (lib_xinerama != NULL) {
        x11_resolve((void **) &x11.XineramaIsActive,     lib_xinerama, "XineramaIsActive");
        x11_resolve((void **) &x11.XineramaQueryScreens, lib_xinerama, "XineramaQueryScreens");

        if (x11.XineramaIsActive != NULL && x11.XineramaQueryScreens != NULL) {
            capabilities |= X11_CAP_XINERAMA;
        }
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: libXinerama is unavailable, multi-monitor disabled!\n",
                __FUNCTION__, __LINE__);
    }

    return UIOHOOK_SUCCESS;
}

void unload_x11_helper(void) {
    if (disp != NULL && x11.XCloseDisplay != NULL) {
        x11.XCloseDisplay(disp);
    }
    disp = NULL;

    if (lib_xinerama != NULL) {
        dlclose(lib_xinerama);
        lib_xinerama = NULL;
    }

    if (lib_xkb_x11 != NULL) {
        dlclose(lib_xkb_x11);
        lib_xkb_x11 = NULL;
    }

    if (lib_x11_xcb != NULL) {
        dlclose(lib_x11_xcb);
        lib_x11_xcb = NULL;
    }

    if (lib_x11 != NULL) {
        dlclose(lib_x11);
        lib_x11 = NULL;
    }

    memset(&x11, 0, sizeof(x11));
    capabilities = 0;
}
