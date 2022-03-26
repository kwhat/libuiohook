/* libUIOHook: Cross-platform keyboard and mouse hooking from userland.
 * Copyright (C) 2006-2023 Alexander Barker.  All Rights Reserved.
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

#include <inttypes.h>
#include <limits.h>

#include <stdint.h>
#ifdef USE_EPOCH_TIME
#include <sys/time.h>
#endif
#include <uiohook.h>

#include <xcb/xkb.h>
#include <X11/XKBlib.h>

#include <X11/keysym.h>
#include <X11/Xlibint.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/record.h>

#if defined(USE_XINERAMA) && !defined(USE_XRANDR)
#include <X11/extensions/Xinerama.h>
#elif defined(USE_XRANDR)
#include <X11/extensions/Xrandr.h>
#else
// TODO We may need to fallback to the xf86vm extension for things like TwinView.
#pragma message("*** Warning: Xinerama or XRandR support is required to produce cross-platform mouse coordinates for multi-head configurations!")
#pragma message("... Assuming single-head display.")
#endif

#include "dispatch_event.h"
#include "input_helper.h"
#include "logger.h"

typedef struct _hook_info {
    struct _data {
        Display *display;
        XRecordRange *range;
    } data;
    struct _ctrl {
        Display *display;
        XRecordContext context;
    } ctrl;
} hook_info;
static hook_info *hook;

#ifdef USE_EPOCH_TIME
/* Get the current timestamp in unix epoch time. */
static uint64_t get_unix_timestamp() {
    struct timeval system_time;

    // Get the local system time in UTC.
    gettimeofday(&system_time, NULL);

    // Convert the local system time to a Unix epoch in MS.
    uint64_t timestamp = (system_time.tv_sec * 1000) + (system_time.tv_usec / 1000);

    return timestamp;
}
#endif

void hook_event_proc(XPointer closeure, XRecordInterceptData *recorded_data) {
    #ifdef USE_EPOCH_TIME
    uint64_t timestamp = get_unix_timestamp();
    #else
    uint64_t timestamp = (uint64_t) recorded_data->server_time;
    #endif

    XEvent event;
    wire_data_to_event(recorded_data, &event);

    XRecordDatum *data = (XRecordDatum *) recorded_data->data;
    switch (recorded_data->category) {
        case XRecordStartOfData:
            dispatch_hook_enabled(timestamp);
            break;

        case XRecordEndOfData:
            dispatch_hook_disabled(timestamp);
            break;

        //case XRecordFromClient: // TODO Should we be listening for Client Events?
        case XRecordFromServer:
            switch (data->type) {
                case KeyPress:
                    dispatch_key_press(timestamp, (XKeyPressedEvent *) &event);
                    break;

                case KeyRelease:
                    dispatch_key_release(timestamp, (XKeyReleasedEvent *) &event);
                    break;

                case ButtonPress:
                    dispatch_mouse_press(timestamp, (XButtonPressedEvent *) &event);
                    break;

                case ButtonRelease:
                    dispatch_mouse_release(timestamp, (XButtonReleasedEvent *) &event);
                    break;

                case MotionNotify:
                    dispatch_mouse_move(timestamp, (XMotionEvent *) &event);
                    break;

                case MappingNotify:
                    // FIXME
                    // event with a request member of MappingKeyboard or MappingModifier occurs
                    //XRefreshKeyboardMapping(event_map)
                    //XMappingEvent *event_map;
                    break;

                default:
                    logger(LOG_LEVEL_DEBUG, "%s [%u]: Unhandled X11 event: %#X.\n",
                            __FUNCTION__, __LINE__,
                            (unsigned int) data->type);
            }
            break;

        default:
            logger(LOG_LEVEL_WARN, "%s [%u]: Unhandled X11 hook category! (%#X)\n",
                    __FUNCTION__, __LINE__, recorded_data->category);
    }

    // TODO There is no way to consume the XRecord event.
}

static int xrecord_block() {
    int status = UIOHOOK_FAILURE;

    // Initialize native input helper.
    int input_helper_status = load_input_helper();
    if (input_helper_status != UIOHOOK_SUCCESS) {
        unload_input_helper();
        return input_helper_status;
    }

    // Save the data display associated with this hook so it is passed to each event.
    XPointer closure = NULL;

    // Sync blocks until XRecordDisableContext() is called.
    if (XRecordEnableContext(hook->data.display, hook->ctrl.context, hook_event_proc, closure)) {
        status = UIOHOOK_SUCCESS;
    } else {
        logger(LOG_LEVEL_ERROR, "%s [%u]: XRecordEnableContext failure!\n",
                __FUNCTION__, __LINE__);

        status = UIOHOOK_ERROR_X_RECORD_ENABLE_CONTEXT;
    }

    // Uninitialize native input helper functions.
    unload_input_helper();

    return status;
}

static int xrecord_alloc() {
    int status = UIOHOOK_FAILURE;

    // Make sure the data display is synchronized to prevent late event delivery!
    // See Bug 42356 for more information.
    // https://bugs.freedesktop.org/show_bug.cgi?id=42356#c4
    XSynchronize(hook->data.display, True);

    // Setup XRecord range.
    XRecordClientSpec clients = XRecordAllClients;

    hook->data.range = XRecordAllocRange();
    if (hook->data.range != NULL) {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: XRecordAllocRange successful.\n",
                __FUNCTION__, __LINE__);

        hook->data.range->device_events.first = KeyPress;
        hook->data.range->device_events.last = MappingNotify;

        // Note that the documentation for this function is incorrect,
        // hook->data.display should be used!
        // See: http://www.x.org/releases/X11R7.6/doc/libXtst/recordlib.txt
        hook->ctrl.context = XRecordCreateContext(hook->data.display, XRecordFromServerTime, &clients, 1, &hook->data.range, 1);
        if (hook->ctrl.context != 0) {
            logger(LOG_LEVEL_DEBUG, "%s [%u]: XRecordCreateContext successful.\n",
                    __FUNCTION__, __LINE__);

            // Block until hook_stop() is called.
            status = xrecord_block();

            // Free up the context if it was set.
            XRecordFreeContext(hook->data.display, hook->ctrl.context);
        } else {
            logger(LOG_LEVEL_ERROR, "%s [%u]: XRecordCreateContext failure!\n",
                    __FUNCTION__, __LINE__);

            // Set the exit status.
            status = UIOHOOK_ERROR_X_RECORD_CREATE_CONTEXT;
        }

        // Free the XRecord range.
        XFree(hook->data.range);
    } else {
        logger(LOG_LEVEL_ERROR, "%s [%u]: XRecordAllocRange failure!\n",
                __FUNCTION__, __LINE__);

        // Set the exit status.
        status = UIOHOOK_ERROR_X_RECORD_ALLOC_RANGE;
    }

    return status;
}

static int xrecord_query() {
    int status = UIOHOOK_FAILURE;

    // Check to make sure XRecord is installed and enabled.
    int major, minor;
    if (XRecordQueryVersion(hook->ctrl.display, &major, &minor) != 0) {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: XRecord version: %i.%i.\n",
                __FUNCTION__, __LINE__, major, minor);

        status = xrecord_alloc();
    } else {
        logger(LOG_LEVEL_ERROR, "%s [%u]: XRecord is not currently available!\n",
                __FUNCTION__, __LINE__);

        status = UIOHOOK_ERROR_X_RECORD_NOT_FOUND;
    }

    return status;
}

static int xrecord_start() {
    int status = UIOHOOK_FAILURE;

    // Use the helper display for XRecord.
    hook->ctrl.display = XOpenDisplay(NULL);

    // Open a data display for XRecord.
    // NOTE This display must be opened on the same thread as XRecord.
    hook->data.display = XOpenDisplay(NULL);
    if (hook->ctrl.display != NULL && hook->data.display != NULL) {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: XOpenDisplay successful.\n",
                __FUNCTION__, __LINE__);

        bool is_auto_repeat = enable_key_repeat();
        if (!is_auto_repeat) {
            logger(LOG_LEVEL_WARN, "%s [%u]: Could not enable detectable auto-repeat!\n",
                    __FUNCTION__, __LINE__);
        } else {
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Successfully enabled detectable auto-repeat.\n",
                    __FUNCTION__, __LINE__);
        }

        status = xrecord_query();
    } else {
        logger(LOG_LEVEL_ERROR, "%s [%u]: XOpenDisplay failure!\n",
                __FUNCTION__, __LINE__);

        status = UIOHOOK_ERROR_X_OPEN_DISPLAY;
    }

    // Close down the XRecord data display.
    if (hook->data.display != NULL) {
        XCloseDisplay(hook->data.display);
        hook->data.display = NULL;
    }

    // Close down the XRecord control display.
    if (hook->ctrl.display) {
        XCloseDisplay(hook->ctrl.display);
        hook->ctrl.display = NULL;
    }

    return status;
}

UIOHOOK_API int hook_run() {
    // Hook data for future cleanup.
    hook = malloc(sizeof(hook_info));
    if (hook == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to allocate memory for hook structure!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }

    int status = xrecord_start();

    // Free data associated with this hook.
    free(hook);
    hook = NULL;

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Something, something, something, complete.\n",
            __FUNCTION__, __LINE__);

    return status;
}

UIOHOOK_API int hook_stop() {
    int status = UIOHOOK_FAILURE;

    if (hook != NULL && hook->ctrl.display != NULL && hook->ctrl.context != 0) {
        // We need to make sure the context is still valid.
        XRecordState *state = malloc(sizeof(XRecordState));
        if (state != NULL) {
            if (XRecordGetContext(hook->ctrl.display, hook->ctrl.context, &state) != 0) {
                // Try to exit the thread naturally.
                if (state->enabled && XRecordDisableContext(hook->ctrl.display, hook->ctrl.context) != 0) {
                    // See Bug 42356 for more information.
                    // https://bugs.freedesktop.org/show_bug.cgi?id=42356#c4
                    //XFlush(hook->ctrl.display);
                    XSync(hook->ctrl.display, False);

                    status = UIOHOOK_SUCCESS;
                }
            } else {
                logger(LOG_LEVEL_ERROR, "%s [%u]: XRecordGetContext failure!\n",
                        __FUNCTION__, __LINE__);

                status = UIOHOOK_ERROR_X_RECORD_GET_CONTEXT;
            }

            free(state);
        } else {
            logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to allocate memory for XRecordState!\n",
                    __FUNCTION__, __LINE__);

            status = UIOHOOK_ERROR_OUT_OF_MEMORY;
        }
    }

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Status: %#X.\n",
            __FUNCTION__, __LINE__, status);

    return status;
}
