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

#include <dlfcn.h>

#ifdef USE_APPKIT
#include <objc/objc.h>
#include <objc/objc-runtime.h>
#endif

#include <stdbool.h>
#include <uiohook.h>

#include "dispatch_event.h"
#include "input_helper.h"
#include "logger.h"




#ifdef USE_APPKIT
static id auto_release_pool;
#endif


#if __MAC_OS_X_VERSION_MAX_ALLOWED <= 1050
typedef void* dispatch_queue_t;
#endif




UIOHOOK_API int hook_run() {
    // Check for accessibility before we start the loop.
    if (!is_accessibility_enabled()) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Accessibility API is disabled!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_ERROR_AXAPI_DISABLED;
    } else {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Accessibility API is enabled.\n",
                __FUNCTION__, __LINE__);
    }

    CFRunLoopRef event_loop = CFRunLoopGetCurrent();
    if (event_loop == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: CFRunLoopGetCurrent failure!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_ERROR_GET_RUNLOOP;
    } else {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: CFRunLoopGetCurrent successful.\n",
                __FUNCTION__, __LINE__);
    }

    set_event_loop(event_loop);

    do {
        // Reset the restart flag...
        set_tap_timeout(false);

        // Initialize starting modifiers.
        initialize_modifiers();

        // Try and allocate memory for event_runloop_info.
        event_runloop_info *hook = NULL;
        int event_runloop_status = create_event_runloop_info(&hook);
        if (event_runloop_status != UIOHOOK_SUCCESS) {
            destroy_event_runloop_info(&hook);
            return event_runloop_status;
        }

        int input_helper_status = load_input_helper();
        if (input_helper_status != UIOHOOK_SUCCESS) {
            // TODO Do we really need to unload here?
            unload_input_helper();
            return input_helper_status;
        }

        #ifdef USE_APPKIT
        // Contributed by Alex <universailp@web.de>
        // Create a garbage collector to handle Cocoa events correctly.
        Class NSAutoreleasePool_class = (Class) objc_getClass("NSAutoreleasePool");
        id pool = class_createInstance(NSAutoreleasePool_class, 0);

        id (*eventWithoutCGEvent)(id, SEL) = (id (*)(id, SEL)) objc_msgSend;
        auto_release_pool = eventWithoutCGEvent(pool, sel_registerName("init"));
        #endif


        // Start the hook thread runloop.
        CFRunLoopRun();


        #ifdef USE_APPKIT
        // Contributed by Alex <universailp@web.de>
        eventWithoutCGEvent(auto_release_pool, sel_registerName("release"));
        #endif

        destroy_event_runloop_info(&hook);

        // Cleanup native input functions.
        unload_input_helper();
    } while (is_tap_timeout());

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Something, something, something, complete.\n",
            __FUNCTION__, __LINE__);

    return UIOHOOK_SUCCESS;
}

UIOHOOK_API int hook_stop() {
    CFRunLoopRef event_loop = get_event_loop();
    CFStringRef mode = CFRunLoopCopyCurrentMode(event_loop);
    if (mode == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: CFRunLoopCopyCurrentMode failure!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_FAILURE;
    }

    CFRelease(mode);

    // Make sure the tap doesn't restart.
    set_tap_timeout(false);

    // Stop the run loop.
    CFRunLoopStop(event_loop);

    return UIOHOOK_SUCCESS;
}
