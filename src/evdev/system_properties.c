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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <uiohook.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>

#ifdef USE_XF86MISC
#include <X11/extensions/xf86misc.h>
#include <X11/extensions/xf86mscstr.h>
#endif

#if defined(USE_XINERAMA) && !defined(USE_XRANDR)
#include <X11/extensions/Xinerama.h>
#elif defined(USE_XRANDR)
#include <pthread.h>
#include <X11/extensions/Xrandr.h>
#endif


static Display *display;

#include "input_helper.h"
#include "logger.h"
#include "wayland_helper.h"
#include "x11_helper.h"

#ifdef USE_XRANDR
static pthread_mutex_t xrandr_mutex = PTHREAD_MUTEX_INITIALIZER;
static XRRScreenResources *xrandr_resources = NULL;

static void settings_cleanup_proc(void *arg) {
    if (pthread_mutex_trylock(&xrandr_mutex) == 0) {
        if (xrandr_resources != NULL) {
            XRRFreeScreenResources(xrandr_resources);
            xrandr_resources = NULL;
        }

        if (arg != NULL) {
            XCloseDisplay((Display *) arg);
            arg = NULL;
        }

        pthread_mutex_unlock(&xrandr_mutex);
    }
}

static void *settings_thread_proc(void *arg) {
    Display *settings_disp = XOpenDisplay(XDisplayName(NULL));;
    if (settings_disp != NULL) {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: %s\n",
                __FUNCTION__, __LINE__, "XOpenDisplay success.");

        pthread_cleanup_push(settings_cleanup_proc, settings_disp);

        int event_base = 0;
        int error_base = 0;
        if (XRRQueryExtension(settings_disp, &event_base, &error_base)) {
            Window root = XDefaultRootWindow(settings_disp);
            unsigned long event_mask = RRScreenChangeNotifyMask;
            XRRSelectInput(settings_disp, root, event_mask);

            XEvent ev;

            while(settings_disp != NULL) {
                XNextEvent(settings_disp, &ev);

                if (ev.type == event_base + RRScreenChangeNotifyMask) {
                    logger(LOG_LEVEL_DEBUG, "%s [%u]: Received XRRScreenChangeNotifyEvent.\n",
                            __FUNCTION__, __LINE__);

                    pthread_mutex_lock(&xrandr_mutex);
                    if (xrandr_resources != NULL) {
                        XRRFreeScreenResources(xrandr_resources);
                    }

                    xrandr_resources = XRRGetScreenResources(settings_disp, root);
                    if (xrandr_resources == NULL) {
                        logger(LOG_LEVEL_WARN, "%s [%u]: XRandR could not get screen resources!\n",
                                __FUNCTION__, __LINE__);
                    }
                    pthread_mutex_unlock(&xrandr_mutex);
                } else {
                    logger(LOG_LEVEL_WARN, "%s [%u]: XRandR is not currently available!\n",
                            __FUNCTION__, __LINE__);
                }
            }
        }

        // Execute the thread cleanup handler.
        pthread_cleanup_pop(1);

    } else {
        logger(LOG_LEVEL_ERROR, "%s [%u]: XOpenDisplay failure!\n",
                __FUNCTION__, __LINE__);
    }

    return NULL;
}
#endif

// FIXME XINERAMA and XRADNR can prob be replaced by https://wayland.app/protocols/xdg-output-unstable-v1#zxdg_output_v1:event:logical_size on wayland.
UIOHOOK_API screen_data* hook_create_screen_info(unsigned char *count) {
    *count = 0;
    screen_data *screens = NULL;

    // Favor Wayland: XOpenDisplay() succeeds under XWayland but reports XWayland's view,
    // so whenever a Wayland session is present use the native xdg-output layout first.
    if (getenv("WAYLAND_DISPLAY") != NULL) {
        screen_data *wl_screens = wayland_create_screen_info(count);
        if (wl_screens != NULL) {
            return wl_screens;
        }

        logger(LOG_LEVEL_WARN, "%s [%u]: Wayland screen info unavailable, trying X11.\n",
                __FUNCTION__, __LINE__);
    }

    Display *display = x11_display();
    if (display == NULL) {
        logger(LOG_LEVEL_WARN, "%s [%u]: X11 display is unavailable!\n",
                __FUNCTION__, __LINE__);
        return NULL;
    }

    // Prefer the multi-monitor layout from Xinerama; query it fresh so a monitor
    // hotplug is always reflected without a cached-resource watcher.
    if (x11_has(X11_CAP_XINERAMA) && x11.XineramaIsActive(display)) {
        int xine_count = 0;
        XineramaScreenInfo *xine_info = x11.XineramaQueryScreens(display, &xine_count);

        if (xine_info != NULL) {
            if (xine_count > UINT8_MAX) {
                *count = UINT8_MAX;

                logger(LOG_LEVEL_WARN, "%s [%u]: Screen count overflow detected!\n",
                        __FUNCTION__, __LINE__);
            } else {
                *count = (uint8_t) xine_count;
            }

            screens = malloc(sizeof(screen_data) * xine_count);

            if (screens != NULL) {
                for (int i = 0; i < xine_count; i++) {
                    screens[i] = (screen_data) {
                        .number = xine_info[i].screen_number,
                        .x = xine_info[i].x_org,
                        .y = xine_info[i].y_org,
                        .width = xine_info[i].width,
                        .height = xine_info[i].height
                    };
                }
            }

            x11.XFree(xine_info);
        }
    } else {
        // Single-screen fallback from the default screen geometry.
        Screen *default_screen = DefaultScreenOfDisplay(display);

        if (default_screen->width > 0 && default_screen->height > 0) {
            screens = malloc(sizeof(screen_data));

            if (screens != NULL) {
                *count = 1;
                screens[0] = (screen_data) {
                    .number = 1,
                    .x = 0,
                    .y = 0,
                    .width = default_screen->width,
                    .height = default_screen->height
                };
            }
        }
    }

    return screens;
}

UIOHOOK_API long int hook_get_auto_repeat_rate() {
    bool successful = false;
    long int value = -1;
    unsigned int delay = 0, rate = 0;

    // Favor the compositor's wl_keyboard repeat_info under Wayland.
    if (getenv("WAYLAND_DISPLAY") != NULL) {
        int32_t wl_rate = -1, wl_delay = -1;
        if (wayland_get_repeat_info(&wl_rate, &wl_delay) == UIOHOOK_SUCCESS && wl_rate > 0) {
            // Wayland reports repeats per second; return the inter-repeat interval in ms
            // to match the value the XKB path historically returned.
            return (long int) (1000 / wl_rate);
        }
    }

    // Check and make sure we could connect to the x server.
    if (display != NULL) {
        // Attempt to acquire the keyboard auto repeat rate using the XKB extension.
        if (!successful) {
            successful = x11.XkbGetAutoRepeatRate(display, XkbUseCoreKbd, &delay, &rate);

            if (successful) {
                logger(LOG_LEVEL_DEBUG, "%s [%u]: XkbGetAutoRepeatRate: %u.\n",
                        __FUNCTION__, __LINE__, rate);
            }
        }

        #ifdef USE_XF86MISC
        // Fallback to the XF86 Misc extension if available and other efforts failed.
        if (!successful) {
            XF86MiscKbdSettings kb_info;
            successful = (bool) XF86MiscGetKbdSettings(display, &kb_info);
            if (successful) {
                logger(LOG_LEVEL_DEBUG, "%s [%u]: XF86MiscGetKbdSettings: %i.\n",
                        __FUNCTION__, __LINE__, kbdinfo.rate);

                delay = (unsigned int) kbdinfo.delay;
                rate = (unsigned int) kbdinfo.rate;
            }
        }
        #endif
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: XDisplay display is unavailable!\n",
                __FUNCTION__, __LINE__);
    }

    if (successful) {
        value = (long int) rate;
    }

    return value;
}

UIOHOOK_API long int hook_get_auto_repeat_delay() {
    bool successful = false;
    long int value = -1;
    unsigned int delay = 0, rate = 0;

    // Favor the compositor's wl_keyboard repeat_info under Wayland.  The delay is in ms
    // on both, so it is returned directly.
    if (getenv("WAYLAND_DISPLAY") != NULL) {
        int32_t wl_rate = -1, wl_delay = -1;
        if (wayland_get_repeat_info(&wl_rate, &wl_delay) == UIOHOOK_SUCCESS && wl_delay >= 0) {
            return (long int) wl_delay;
        }
    }

    // Check and make sure we could connect to the x server.
    if (display != NULL) {
        // Attempt to acquire the keyboard auto repeat rate using the XKB extension.
        if (!successful) {
            successful = x11.XkbGetAutoRepeatRate(display, XkbUseCoreKbd, &delay, &rate);

            if (successful) {
                logger(LOG_LEVEL_DEBUG, "%s [%u]: XkbGetAutoRepeatRate: %u.\n",
                        __FUNCTION__, __LINE__, delay);
            }
        }

        #ifdef USE_XF86MISC
        // Fallback to the XF86 Misc extension if available and other efforts failed.
        if (!successful) {
            XF86MiscKbdSettings kb_info;
            successful = (bool) XF86MiscGetKbdSettings(display, &kb_info);
            if (successful) {
                logger(LOG_LEVEL_DEBUG, "%s [%u]: XF86MiscGetKbdSettings: %i.\n",
                        __FUNCTION__, __LINE__, kbdinfo.delay);

                delay = (unsigned int) kbdinfo.delay;
                rate = (unsigned int) kbdinfo.rate;
            }
        }
        #endif
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: XDisplay display is unavailable!\n",
                __FUNCTION__, __LINE__);
    }

    if (successful) {
        value = (long int) delay;
    }

    return value;
}

UIOHOOK_API long int hook_get_pointer_acceleration_multiplier() {
    long int value = -1;
    int accel_numerator, accel_denominator, threshold;

    // Check and make sure we could connect to the x server.
    if (display != NULL) {
        x11.XGetPointerControl(display, &accel_numerator, &accel_denominator, &threshold);
        if (accel_denominator >= 0) {
            logger(LOG_LEVEL_DEBUG, "%s [%u]: XGetPointerControl: %i.\n",
                    __FUNCTION__, __LINE__, accel_denominator);

            value = (long int) accel_denominator;
        }
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: XDisplay display is unavailable!\n",
                __FUNCTION__, __LINE__);
    }

    return value;
}

UIOHOOK_API long int hook_get_pointer_acceleration_threshold() {
    long int value = -1;
    int accel_numerator, accel_denominator, threshold;

    // Check and make sure we could connect to the x server.
    if (display != NULL) {
        x11.XGetPointerControl(display, &accel_numerator, &accel_denominator, &threshold);
        if (threshold >= 0) {
            logger(LOG_LEVEL_DEBUG, "%s [%u]: XGetPointerControl: %i.\n",
                    __FUNCTION__, __LINE__, threshold);

            value = (long int) threshold;
        }
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: XDisplay display is unavailable!\n",
                __FUNCTION__, __LINE__);
    }

    return value;
}

UIOHOOK_API long int hook_get_pointer_sensitivity() {
    long int value = -1;
    int accel_numerator, accel_denominator, threshold;

    // Check and make sure we could connect to the x server.
    if (display != NULL) {
        x11.XGetPointerControl(display, &accel_numerator, &accel_denominator, &threshold);
        if (accel_numerator >= 0) {
            logger(LOG_LEVEL_DEBUG, "%s [%u]: XGetPointerControl: %i.\n",
                    __FUNCTION__, __LINE__, accel_numerator);

            value = (long int) accel_numerator;
        }
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: XDisplay display is unavailable!\n",
                __FUNCTION__, __LINE__);
    }

    return value;
}

UIOHOOK_API long int hook_get_multi_click_time() {
    long int value = 200;
    int click_time;
    bool successful = false;

    // Check and make sure we could connect to the x server.
    if (display != NULL) {
        // Try and acquire the multi-click time from the user defined X defaults.
        if (!successful) {
            char *xprop = x11.XGetDefault(display, "*", "multiClickTime");
            if (xprop != NULL && sscanf(xprop, "%4i", &click_time) != EOF) {
                logger(LOG_LEVEL_DEBUG, "%s [%u]: X default 'multiClickTime' property: %i.\n",
                        __FUNCTION__, __LINE__, click_time);

                successful = true;
            }
        }

        if (!successful) {
            char *xprop = x11.XGetDefault(display, "OpenWindows", "MultiClickTimeout");
            if (xprop != NULL && sscanf(xprop, "%4i", &click_time) != EOF) {
                logger(LOG_LEVEL_DEBUG, "%s [%u]: X default 'MultiClickTimeout' property: %i.\n",
                        __FUNCTION__, __LINE__, click_time);

                successful = true;
            }
        }
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: XDisplay display is unavailable!\n",
                __FUNCTION__, __LINE__);
    }

    if (successful) {
        value = (long int) click_time;
    }

    return value;
}

// Create a shared object constructor.
__attribute__ ((constructor))
void on_library_load() {
    // Load the X11 libraries at runtime and open the shared connection (also calls
    // XInitThreads).  Owned here for the whole library lifetime so the public property
    // queries work whether or not the hook is currently running.
    if (load_x11_helper() != UIOHOOK_SUCCESS) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: %s\n",
                __FUNCTION__, __LINE__, "Failed to open shared X11 display!");
    }
    display = x11_display();

    #ifdef USE_XRANDR
    // Create the thread attribute.
    pthread_attr_t settings_thread_attr;
    pthread_attr_init(&settings_thread_attr);

    pthread_t settings_thread_id;
    if (pthread_create(&settings_thread_id, &settings_thread_attr, settings_thread_proc, NULL) == 0) {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Successfully created settings thread.\n",
                __FUNCTION__, __LINE__);
    } else {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to create settings thread!\n",
                __FUNCTION__, __LINE__);
    }

    // Make sure the thread attribute is removed.
    pthread_attr_destroy(&settings_thread_attr);
    #endif
}

// Create a shared object destructor.
__attribute__ ((destructor))
void on_library_unload() {
    // Disable the event hook.
    //hook_stop();

    // Cleanup.
    unload_input_helper();

    // Closes the shared connection and dlcloses the X11 libraries.
    unload_x11_helper();
    display = NULL;
}
