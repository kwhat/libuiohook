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
#include <mach/mach_time.h>

#ifdef USE_APPKIT
#include <objc/objc.h>
#include <objc/objc-runtime.h>
#endif

#include <pthread.h>
#include <stdbool.h>
#include <sys/time.h>
#include <uiohook.h>

#include "input_helper.h"
#include "logger.h"


typedef struct _event_runloop_info {
    CFMachPortRef port;
    CFRunLoopSourceRef source;
    CFRunLoopObserverRef observer;
} event_runloop_info;

#ifdef USE_APPKIT
static id auto_release_pool;

typedef struct {
    CGEventRef event;
    UInt32 subtype;
    UInt32 data1;
} TISEventMessage;
TISEventMessage *tis_event_message;
#endif

// Event runloop reference.
CFRunLoopRef event_loop;

// Flag to restart the event tap incase of timeout.
static Boolean restart_tap = false;

// Required to transport messages between the main runloop and our thread for
// Unicode lookups.
#define KEY_BUFFER_SIZE 4
typedef struct {
    CGEventRef event;
    UniChar buffer[KEY_BUFFER_SIZE];
    UniCharCount length;
} TISKeycodeMessage;
TISKeycodeMessage *tis_keycode_message;

#if __MAC_OS_X_VERSION_MAX_ALLOWED <= 1050
typedef void* dispatch_queue_t;
#endif
static struct dispatch_queue_s *dispatch_main_queue_s;
static void (*dispatch_sync_f_f)(dispatch_queue_t, void *, void (*function)(void *));

#if defined(USE_APPLICATION_SERVICES)
typedef struct _main_runloop_info {
    CFRunLoopSourceRef source;
    CFRunLoopObserverRef observer;
} main_runloop_info;

main_runloop_info *main_runloop_keycode = NULL;

static pthread_cond_t main_runloop_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t main_runloop_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

// Click count globals.
static unsigned short click_count = 0;
static CGEventTimestamp click_time = 0;
static unsigned short int click_button = MOUSE_NOBUTTON;
static bool mouse_dragged = false;

#ifdef USE_EPOCH_TIME
// Structure for the current Unix epoch in milliseconds.
static struct timeval system_time;
#endif



#ifdef USE_EPOCH_TIME
static uint64_t get_unix_timestamp() {
	// Get the local system time in UTC.
	gettimeofday(&system_time, NULL);

	// Convert the local system time to a Unix epoch in MS.
	uint64_t timestamp = (system_time.tv_sec * 1000) + (system_time.tv_usec / 1000);

	return timestamp;
}
#endif

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


#ifdef USE_APPKIT
static void obcj_message(void *info) {
    TISEventMessage *data = (TISEventMessage *) info;

    if (data != NULL && data->event != NULL) {
        // Contributed by Iván Munsuri Ibáñez <munsuri@gmail.com> and Alex <universailp@web.de>
        id (*eventWithCGEvent)(id, SEL, CGEventRef) = (id (*)(id, SEL, CGEventRef)) objc_msgSend;
        id event_data = eventWithCGEvent((id) objc_getClass("NSEvent"), sel_registerName("eventWithCGEvent:"), data->event);

        UInt32 (*eventWithoutCGEvent)(id, SEL) = (UInt32 (*)(id, SEL)) objc_msgSend;
        data->subtype = eventWithoutCGEvent(event_data, sel_registerName("subtype"));
        data->data1 = eventWithoutCGEvent(event_data, sel_registerName("data1"));
    }
}
#endif

CGEventRef hook_event_proc(CGEventTapProxy tap_proxy, CGEventType type, CGEventRef event_ref, void *refcon) {
    #ifdef USE_EPOCH_TIME
	uint64_t timestamp = get_unix_timestamp();
    #else
    uint64_t timestamp = (uint64_t) CGEventGetTimestamp(event_ref);
    #endif

    // Get the event class.
    switch (type) {
        case kCGEventKeyDown:
            dispatch_key_press(timestamp, event_ref);
            break;

        case kCGEventKeyUp:
            dispatch_key_release(timestamp, event_ref);
            break;

        case kCGEventFlagsChanged:
            dispatch_modifier_change(timestamp, event_ref);
            break;

        case NX_SYSDEFINED:
            dispatch_system_key(timestamp, event_ref);
            break;

        case kCGEventLeftMouseDown:
            set_modifier_mask(MASK_BUTTON1);
            dispatch_button_press(timestamp, event_ref, MOUSE_BUTTON1);
            break;

        case kCGEventRightMouseDown:
            set_modifier_mask(MASK_BUTTON2);
            dispatch_button_press(timestamp, event_ref, MOUSE_BUTTON2);
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

                dispatch_button_press(timestamp, event_ref, button);
            }
            break;

        case kCGEventLeftMouseUp:
            unset_modifier_mask(MASK_BUTTON1);
            dispatch_button_release(timestamp, event_ref, MOUSE_BUTTON1);
            break;

        case kCGEventRightMouseUp:
            unset_modifier_mask(MASK_BUTTON2);
            dispatch_button_release(timestamp, event_ref, MOUSE_BUTTON2);
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

                dispatch_button_press(timestamp, event_ref, button);
            }
            break;


        case kCGEventLeftMouseDragged:
        case kCGEventRightMouseDragged:
        case kCGEventOtherMouseDragged:
            // FIXME The drag flag is confusing.  Use prev x,y to determine click.
            // Set the mouse dragged flag.
            mouse_dragged = true;
            dispatch_mouse_move(timestamp, event_ref);
            break;

        case kCGEventMouseMoved:
            // Set the mouse dragged flag.
            mouse_dragged = false;
            dispatch_mouse_move(timestamp, event_ref);
            break;


        case kCGEventScrollWheel:
            dispatch_mouse_wheel(timestamp, event_ref);
            break;

        default:
            // Check for an old OS X bug where the tap seems to timeout for no reason.
            // See: http://stackoverflow.com/questions/2969110/cgeventtapcreate-breaks-down-mysteriously-with-key-down-events#2971217
            if (type == (CGEventType) kCGEventTapDisabledByTimeout) {
                logger(LOG_LEVEL_WARN, "%s [%u]: CGEventTap timeout!\n",
                        __FUNCTION__, __LINE__);

                // We need to restart the tap!
                restart_tap = true;
                CFRunLoopStop(CFRunLoopGetCurrent());
            } else {
                // In theory this *should* never execute.
                logger(LOG_LEVEL_DEBUG, "%s [%u]: Unhandled Darwin event: %#X.\n",
                        __FUNCTION__, __LINE__, (unsigned int) type);
            }
            break;
    }

    CGEventRef result_ref = NULL;
    if (event.reserved ^ 0x01) {
        result_ref = event_ref;
    } else {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Consuming the current event. (%#X) (%#p)\n",
                __FUNCTION__, __LINE__, type, event_ref);
    }

    return result_ref;
}


#ifdef USE_APPLICATION_SERVICES
void main_runloop_status_proc(CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *info) {
    switch (activity) {
        case kCFRunLoopExit:
            // Acquire a lock on the msg_port and signal that anyone waiting should continue.
            pthread_mutex_lock(&main_runloop_mutex);
            pthread_cond_broadcast(&main_runloop_cond);
            pthread_mutex_unlock(&main_runloop_mutex);
            break;
    }
}

// Runloop to execute KeyCodeToString on the "Main" runloop due to an undocumented thread safety requirement.
static void main_runloop_keycode_proc(void *info) {
    // Lock the msg_port mutex as we enter the main runloop.
    pthread_mutex_lock(&main_runloop_mutex);

    TISKeycodeMessage *data = (TISKeycodeMessage *) info;
    if (data != NULL && data->event != NULL) {
        // Preform Unicode lookup.
        data->length = keycode_to_unicode(data->event, data->buffer, KEY_BUFFER_SIZE);
    }

    // Unlock the msg_port mutex to signal to the hook_thread that we have
    // finished on the main runloop.
    pthread_cond_broadcast(&main_runloop_cond);
    pthread_mutex_unlock(&main_runloop_mutex);
}

static int create_main_runloop_info(main_runloop_info **main, CFRunLoopSourceContext *context) {
    if (*main != NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Expected unallocated main_runloop_info pointer!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_FAILURE;
    }

    // Try and allocate memory for event_runloop_info.
    *main = malloc(sizeof(main_runloop_info));
    if (*main == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to allocate memory for main_runloop_info structure!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }

    // Create a runloop observer for the main runloop.
    (*main)->observer = CFRunLoopObserverCreate(
            kCFAllocatorDefault,
            kCFRunLoopExit, //kCFRunLoopEntry | kCFRunLoopExit, //kCFRunLoopAllActivities,
            true,
            0,
            main_runloop_status_proc,
            NULL
        );
    if ((*main)->observer == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: CFRunLoopObserverCreate failure!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_ERROR_CREATE_OBSERVER;
    } else {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: CFRunLoopObserverCreate success!\n",
                __FUNCTION__, __LINE__);
    }

    (*main)->source = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, context);

    if ((*main)->source == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: CFRunLoopSourceCreate failure!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_ERROR_CREATE_RUN_LOOP_SOURCE;
    } else {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: CFRunLoopSourceCreate success!\n",
                __FUNCTION__, __LINE__);
    }

    // FIXME Check for null ?
    CFRunLoopRef main_loop = CFRunLoopGetMain();

    pthread_mutex_lock(&main_runloop_mutex);

    CFRunLoopAddSource(main_loop, (*main)->source, kCFRunLoopDefaultMode);
    CFRunLoopAddObserver(main_loop, (*main)->observer, kCFRunLoopDefaultMode);

    pthread_mutex_unlock(&main_runloop_mutex);

    return UIOHOOK_SUCCESS;
}

static void destroy_main_runloop_info(main_runloop_info **main) {
    if (*main != NULL) {
         CFRunLoopRef main_loop = CFRunLoopGetMain();

         if ((*main)->observer != NULL) {
             if (CFRunLoopContainsObserver(main_loop, (*main)->observer, kCFRunLoopDefaultMode)) {
                 CFRunLoopRemoveObserver(main_loop, (*main)->observer, kCFRunLoopDefaultMode);
             }

             CFRunLoopObserverInvalidate((*main)->observer);
             CFRelease((*main)->observer);
             (*main)->observer = NULL;
         }

         if ((*main)->source != NULL) {
             if (CFRunLoopContainsSource(main_loop, (*main)->source, kCFRunLoopDefaultMode)) {
                 CFRunLoopRemoveSource(main_loop, (*main)->source, kCFRunLoopDefaultMode);
             }

             CFRelease((*main)->source);
             (*main)->source = NULL;
         }

        // Free the main structure.
        free(*main);
        *main = NULL;
    }
}
#endif


static int create_event_runloop_info(event_runloop_info **hook) {
    if (*hook != NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Expected unallocated event_runloop_info pointer!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_FAILURE;
    }

    // Try and allocate memory for event_runloop_info.
    *hook = malloc(sizeof(event_runloop_info));
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

            // NOTE This event is undocumented and used
            // for caps-lock release and multi-media keys.
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

        // Stop the CFMachPort from receiving any more messages.
        CFMachPortInvalidate((*hook)->port);
        CFRelease((*hook)->port);
        (*hook)->port = NULL;

        // Free the hook structure.
        free(*hook);
        *hook = NULL;
    }
}

UIOHOOK_API int hook_run() {
    int status = UIOHOOK_SUCCESS;

    // Check for accessibility before we start the loop.
    if (is_accessibility_enabled()) {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Accessibility API is enabled.\n",
                __FUNCTION__, __LINE__);

        event_loop = CFRunLoopGetCurrent();
        if (event_loop != NULL) {
            logger(LOG_LEVEL_DEBUG, "%s [%u]: CFRunLoopGetCurrent successful.\n",
                    __FUNCTION__, __LINE__);

            do {
                // Reset the restart flag...
                restart_tap = false;

                // Initialize starting modifiers.
                initialize_modifiers();

                // Try and allocate memory for event_runloop_info.
                event_runloop_info *hook = NULL;
                int event_runloop_status = create_event_runloop_info(&hook);
                if (event_runloop_status != UIOHOOK_SUCCESS) {
                    destroy_event_runloop_info(&hook);
                    return event_runloop_status;
                }


                tis_keycode_message = (TISKeycodeMessage *) calloc(1, sizeof(TISKeycodeMessage));
                if (tis_keycode_message == NULL) {
                    logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to allocate memory for TIS message structure!\n",
                            __FUNCTION__, __LINE__);

                    return UIOHOOK_ERROR_OUT_OF_MEMORY;
                }

                #ifdef USE_APPKIT
                tis_event_message = (TISEventMessage *) calloc(1, sizeof(TISEventMessage));
                if (tis_event_message == NULL) {
                    logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to allocate memory for TIS event structure!\n",
                            __FUNCTION__, __LINE__);

                    return UIOHOOK_ERROR_OUT_OF_MEMORY;
                }
                #endif

                // If we are not running on the main runloop, we need to setup a runloop dispatcher.
                if (!CFEqual(event_loop, CFRunLoopGetMain())) {
                    // Dynamically load dispatch_sync_f to maintain 10.5 compatibility.
                    *(void **) (&dispatch_sync_f_f) = dlsym(RTLD_DEFAULT, "dispatch_sync_f");
                    const char *dlError = dlerror();
                    if (dlError != NULL) {
                        logger(LOG_LEVEL_DEBUG, "%s [%u]: %s.\n",
                                __FUNCTION__, __LINE__, dlError);
                    }

                    // This load is equivalent to calling dispatch_get_main_queue().  We use
                    // _dispatch_main_q because dispatch_get_main_queue is not exported from
                    // libdispatch.dylib and the upstream function only dereferences the pointer.
                    dispatch_main_queue_s = (struct dispatch_queue_s *) dlsym(RTLD_DEFAULT, "_dispatch_main_q");
                    dlError = dlerror();
                    if (dlError != NULL) {
                        logger(LOG_LEVEL_DEBUG, "%s [%u]: %s.\n",
                                __FUNCTION__, __LINE__, dlError);
                    }

                    if (dispatch_sync_f_f == NULL || dispatch_main_queue_s == NULL) {
                        logger(LOG_LEVEL_DEBUG, "%s [%u]: Failed to locate dispatch_sync_f() or dispatch_get_main_queue()!\n",
                                __FUNCTION__, __LINE__);

                        #ifdef USE_APPLICATION_SERVICES
                        logger(LOG_LEVEL_DEBUG, "%s [%u]: Falling back to runloop signaling.\n",
                                __FUNCTION__, __LINE__);

                        // TODO The only thing that maybe needed in this struct is the .perform
                        CFRunLoopSourceContext main_runloop_keycode_context = {
                            .version         = 0,
                            .info            = tis_keycode_message,
                            .retain          = NULL,
                            .release         = NULL,
                            .copyDescription = NULL,
                            .equal           = NULL,
                            .hash            = NULL,
                            .schedule        = NULL,
                            .cancel          = NULL,
                            .perform         = main_runloop_keycode_proc
                        };

                        int keycode_runloop_status = create_main_runloop_info(&main_runloop_keycode, &main_runloop_keycode_context);
                        if (keycode_runloop_status != UIOHOOK_SUCCESS) {
                            destroy_main_runloop_info(&main_runloop_keycode);
                            return keycode_runloop_status;
                        }
                        #endif
                    }
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

                #ifdef USE_APPLICATION_SERVICES
                pthread_mutex_lock(&main_runloop_mutex);
                if (!CFEqual(event_loop, CFRunLoopGetMain())) {
                    if (dispatch_sync_f_f == NULL || dispatch_main_queue_s == NULL) {
                        destroy_main_runloop_info(&main_runloop_keycode);
                    }
                }
                pthread_mutex_unlock(&main_runloop_mutex);
                #endif

                #ifdef USE_APPKIT
                free(tis_event_message);
                #endif
                free(tis_keycode_message);

                destroy_event_runloop_info(&hook);
            } while (restart_tap);
        } else {
            logger(LOG_LEVEL_ERROR, "%s [%u]: CFRunLoopGetCurrent failure!\n",
                    __FUNCTION__, __LINE__);

            status = UIOHOOK_ERROR_GET_RUNLOOP;
        }
    } else {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Accessibility API is disabled!\n",
                __FUNCTION__, __LINE__);

        status = UIOHOOK_ERROR_AXAPI_DISABLED;
    }

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Something, something, something, complete.\n",
            __FUNCTION__, __LINE__);

    return status;
}

UIOHOOK_API int hook_stop() {
    int status = UIOHOOK_FAILURE;

    CFStringRef mode = CFRunLoopCopyCurrentMode(event_loop);
    if (mode != NULL) {
        CFRelease(mode);

        // Make sure the tap doesn't restart.
        restart_tap = false;

        // Stop the run loop.
        CFRunLoopStop(event_loop);

        // Cleanup native input functions.
        unload_input_helper();

        status = UIOHOOK_SUCCESS;
    }

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Status: %#X.\n",
            __FUNCTION__, __LINE__, status);

    return status;
}
