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


typedef struct _event_runloop_info {
    CFMachPortRef port;
    CFRunLoopSourceRef source;
    CFRunLoopObserverRef observer;
} event_runloop_info;

// This is a reference to the thread that is being blocked by the event hook.
static CFRunLoopRef event_loop;

#ifdef USE_APPKIT
static id auto_release_pool;
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED <= 1050
typedef void* dispatch_queue_t;
#endif


#ifdef USE_EPOCH_TIME
#include <sys/time.h>
// Structure for the current Unix epoch in milliseconds.
static struct timeval system_time;
#else
#include <mach/mach_time.h>
#endif

// We define the event_runloop_info as a static so that hook_event_proc can
// re-enable the tap when it gets disabled by a timeout
static event_runloop_info *hook = NULL;

#ifdef USE_EPOCH_TIME
static uint64_t get_unix_timestamp() {
	// Get the local system time in UTC.
	gettimeofday(&system_time, NULL);

	// Convert the local system time to a Unix epoch in MS.
	uint64_t timestamp = (system_time.tv_sec * 1000) + (system_time.tv_usec / 1000);

	return timestamp;
}
#endif


static CGEventRef hook_event_proc(CGEventTapProxy tap_proxy, CGEventType type, CGEventRef event_ref, void *refcon) {
    bool consumed = false;
    #ifdef USE_EPOCH_TIME
    uint64_t timestamp = get_unix_timestamp();
    #else
    uint64_t timestamp = (uint64_t) CGEventGetTimestamp(event_ref);
    #endif

    // Get the event class.
    switch (type) {
        case kCGEventKeyDown:
            consumed = dispatch_key_press(timestamp, event_ref);
            break;

        case kCGEventKeyUp:
            consumed = dispatch_key_release(timestamp, event_ref);
            break;

        case kCGEventFlagsChanged:
            consumed = dispatch_modifier_change(timestamp, event_ref);
            break;

        case NX_SYSDEFINED:
            consumed = dispatch_system_key(timestamp, event_ref);
            break;

        case kCGEventLeftMouseDown:
            set_modifier_mask(MASK_BUTTON1);
            consumed = dispatch_button_press(timestamp, event_ref, MOUSE_BUTTON1);
            break;

        case kCGEventRightMouseDown:
            set_modifier_mask(MASK_BUTTON2);
            consumed = dispatch_button_press(timestamp, event_ref, MOUSE_BUTTON2);
            break;

        case kCGEventOtherMouseDown:
            // Extra mouse buttons.
            if (CGEventGetIntegerValueField(event_ref, kCGMouseEventButtonNumber) < UINT16_MAX) {
                uint16_t button = (uint16_t) CGEventGetIntegerValueField(event_ref, kCGMouseEventButtonNumber) + 1;

                // Add support for mouse 4 & 5.
                if (button == 4) {
                    set_modifier_mask(MOUSE_BUTTON4);
                } else if (button == 5) {
                    set_modifier_mask(MOUSE_BUTTON5);
                }

                consumed = dispatch_button_press(timestamp, event_ref, button);
            }
            break;

        case kCGEventLeftMouseUp:
            unset_modifier_mask(MASK_BUTTON1);
            consumed = dispatch_button_release(timestamp, event_ref, MOUSE_BUTTON1);
            break;

        case kCGEventRightMouseUp:
            unset_modifier_mask(MASK_BUTTON2);
            consumed = dispatch_button_release(timestamp, event_ref, MOUSE_BUTTON2);
            break;

        case kCGEventOtherMouseUp:
            // Extra mouse buttons.
            if (CGEventGetIntegerValueField(event_ref, kCGMouseEventButtonNumber) < UINT16_MAX) {
                uint16_t button = (uint16_t) CGEventGetIntegerValueField(event_ref, kCGMouseEventButtonNumber) + 1;

                // Add support for mouse 4 & 5.
                if (button == 4) {
                    unset_modifier_mask(MOUSE_BUTTON4);
                } else if (button == 5) {
                    unset_modifier_mask(MOUSE_BUTTON5);
                }

                consumed = dispatch_button_press(timestamp, event_ref, button);
            }
            break;


        case kCGEventLeftMouseDragged:
        case kCGEventRightMouseDragged:
        case kCGEventOtherMouseDragged:
            // FIXME The drag flag is confusing.  Use prev x,y to determine click.
            // Set the mouse dragged flag.
            set_mouse_dragged(true);
            consumed = dispatch_mouse_move(timestamp, event_ref);
            break;

        case kCGEventMouseMoved:
            // Set the mouse dragged flag.
            set_mouse_dragged(false);
            consumed = dispatch_mouse_move(timestamp, event_ref);
            break;


        case kCGEventScrollWheel:
            consumed = dispatch_mouse_wheel(timestamp, event_ref);
            break;

        default:
            // Check for an old OS X bug where the tap seems to timeout for no reason.
            // See: http://stackoverflow.com/questions/2969110/cgeventtapcreate-breaks-down-mysteriously-with-key-down-events#2971217
            if (type == (CGEventType) kCGEventTapDisabledByTimeout) {
                logger(LOG_LEVEL_WARN, "%s [%u]: CGEventTap timeout!\n",
                        __FUNCTION__, __LINE__);

                // We need to re-enable the tap
                if (hook->port) {
                    CGEventTapEnable(hook->port, true);
                }
            } else {
                // In theory this *should* never execute.
                logger(LOG_LEVEL_DEBUG, "%s [%u]: Unhandled Darwin event: %#X.\n",
                        __FUNCTION__, __LINE__, (unsigned int) type);
            }
            break;
    }

    CGEventRef result_ref = NULL;
    if (!consumed) {
        result_ref = event_ref;
    } else {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Consuming the current event. (%#X) (%#p)\n",
                __FUNCTION__, __LINE__, type, event_ref);
    }

    return result_ref;
}

static void hook_status_proc(CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *info) {
    #ifdef USE_EPOCH_TIME
	uint64_t timestamp = get_unix_timestamp();
    #else
    uint64_t timestamp = mach_absolute_time();
    #endif

    switch (activity) {
        case kCFRunLoopEntry:
            dispatch_hook_enabled(timestamp);
            break;

        case kCFRunLoopExit:
            dispatch_hook_disabled(timestamp);
            break;

        default:
            logger(LOG_LEVEL_WARN, "%s [%u]: Unhandled RunLoop activity! (%#X)\n",
                    __FUNCTION__, __LINE__, (unsigned int) activity);
    }
}

static int create_event_runloop_info(event_runloop_info **hook) {
    if (*hook != NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Expected unallocated event_runloop_info pointer!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_FAILURE;
    }

    // Try and allocate memory for event_runloop_info.
    *hook = calloc(1, sizeof(event_runloop_info));
    if (*hook == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to allocate memory for event_runloop_info structure!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }

    // Setup the event mask to listen for.
    CGEventMask event_mask = CGEventMaskBit(kCGEventKeyDown) |
            CGEventMaskBit(kCGEventKeyUp) |
            CGEventMaskBit(kCGEventFlagsChanged) |

            CGEventMaskBit(kCGEventLeftMouseDown) |
            CGEventMaskBit(kCGEventLeftMouseUp) |
            CGEventMaskBit(kCGEventLeftMouseDragged) |

            CGEventMaskBit(kCGEventRightMouseDown) |
            CGEventMaskBit(kCGEventRightMouseUp) |
            CGEventMaskBit(kCGEventRightMouseDragged) |

            CGEventMaskBit(kCGEventOtherMouseDown) |
            CGEventMaskBit(kCGEventOtherMouseUp) |
            CGEventMaskBit(kCGEventOtherMouseDragged) |

            CGEventMaskBit(kCGEventMouseMoved) |
            CGEventMaskBit(kCGEventScrollWheel) |

            // NOTE This event is undocumented and used for caps-lock release and multi-media keys.
            CGEventMaskBit(NX_SYSDEFINED);

    // Create the event tap.
    (*hook)->port = CGEventTapCreate(
            kCGSessionEventTap,       // kCGHIDEventTap
            kCGHeadInsertEventTap,    // kCGTailAppendEventTap
            kCGEventTapOptionDefault, // kCGEventTapOptionListenOnly See https://github.com/kwhat/jnativehook/issues/22
            event_mask,
            hook_event_proc,
            NULL);
    if ((*hook)->port == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to create event port!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_ERROR_CREATE_EVENT_PORT;
    } else {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: CGEventTapCreate Successful.\n",
                __FUNCTION__, __LINE__);
    }

    // Create the runloop event source from the event tap.
    (*hook)->source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, (*hook)->port, 0);
    if ((*hook)->source == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: CFMachPortCreateRunLoopSource failure!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_ERROR_CREATE_RUN_LOOP_SOURCE;
    } else {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: CFMachPortCreateRunLoopSource successful.\n",
                __FUNCTION__, __LINE__);
    }

    // Create run loop observers.
    (*hook)->observer = CFRunLoopObserverCreate(
            kCFAllocatorDefault,
            kCFRunLoopEntry | kCFRunLoopExit, //kCFRunLoopAllActivities,
            true,
            0,
            hook_status_proc,
            NULL);
    if ((*hook)->observer == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: CFRunLoopObserverCreate failure!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_ERROR_CREATE_OBSERVER;
    } else {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: CFRunLoopObserverCreate successful.\n",
                __FUNCTION__, __LINE__);
    }

    // Add the event source and observer to the runloop mode.
    CFRunLoopAddSource(event_loop, (*hook)->source, kCFRunLoopDefaultMode);
    CFRunLoopAddObserver(event_loop, (*hook)->observer, kCFRunLoopDefaultMode);

    return UIOHOOK_SUCCESS;
}

static void destroy_event_runloop_info(event_runloop_info **hook) {
    // FIXME check event_loop for null ?

    if (*hook != NULL) {
        if ((*hook)->observer != NULL) {
            if (CFRunLoopContainsObserver(event_loop, (*hook)->observer, kCFRunLoopDefaultMode)) {
                CFRunLoopRemoveObserver(event_loop, (*hook)->observer, kCFRunLoopDefaultMode);
            }

            // Invalidate and free hook observer.
            CFRunLoopObserverInvalidate((*hook)->observer);
            CFRelease((*hook)->observer);
            (*hook)->observer = NULL;
        }

        if ((*hook)->source != NULL) {
            if (CFRunLoopContainsSource(event_loop, (*hook)->source, kCFRunLoopDefaultMode)) {
                CFRunLoopRemoveSource(event_loop, (*hook)->source, kCFRunLoopDefaultMode);
            }

            // Clean up the event source.
            CFRelease((*hook)->source);
            (*hook)->source = NULL;
        }

        if ((*hook)->port != NULL) {
            // Stop the CFMachPort from receiving any more messages.
            CFMachPortInvalidate((*hook)->port);
            CFRelease((*hook)->port);
            (*hook)->port = NULL;
        }

        // Free the hook structure.
        free(*hook);
        *hook = NULL;
    }
}

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

    event_loop = CFRunLoopGetCurrent();
    if (event_loop == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: CFRunLoopGetCurrent failure!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_ERROR_GET_RUNLOOP;
    } else {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: CFRunLoopGetCurrent successful.\n",
                __FUNCTION__, __LINE__);
    }

    // Try and allocate memory for event_runloop_info.
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

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Something, something, something, complete.\n",
            __FUNCTION__, __LINE__);

    return UIOHOOK_SUCCESS;
}

UIOHOOK_API int hook_stop() {
    CFStringRef mode = CFRunLoopCopyCurrentMode(event_loop);
    if (mode == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: CFRunLoopCopyCurrentMode failure!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_FAILURE;
    }
    CFRelease(mode);

        // Stop the run loop.
        CFRunLoopStop(event_loop);

    return UIOHOOK_SUCCESS;
}
