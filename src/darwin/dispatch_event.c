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

#include "dispatch_event.h"
#include "input_helper.h"
#include "logger.h"

#ifdef USE_EPOCH_TIME
#define TIMER_RESOLUTION_MS 1
#else
#define TIMER_RESOLUTION_MS 1000000
#endif

// Virtual event pointer.
static uiohook_event uio_event;

// Event dispatch callback.
static dispatcher_t dispatch = NULL;
static void *dispatch_data = NULL;

#ifdef USE_APPKIT
static id auto_release_pool;

typedef struct {
    CGEventRef event;
    UInt32 subtype;
    UInt32 data1;
} TISEventMessage;
static TISEventMessage tis_event_message;
#endif

UIOHOOK_API void hook_set_dispatch_proc(dispatcher_t dispatch_proc, void *user_data) {
    logger(LOG_LEVEL_DEBUG, "%s [%u]: Setting new dispatch callback to %#p.\n",
            __FUNCTION__, __LINE__, dispatch_proc);

    dispatch = dispatch_proc;
    dispatch_data = user_data;
}

// Send out an event if a dispatcher was set.
static void dispatch_event(uiohook_event *const event) {
    if (dispatch != NULL) {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Dispatching event type %u.\n",
                __FUNCTION__, __LINE__, event->type);

        dispatch(event, dispatch_data);
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: No dispatch callback set!\n",
                __FUNCTION__, __LINE__);
    }
}

bool dispatch_hook_enabled(uint64_t timestamp) {
    bool consumed = false;

    // Initialize Native Input Functions.
    load_input_helper();

    // Populate the hook start event.
    uio_event.time = timestamp;
    uio_event.reserved = 0x00;

    uio_event.type = EVENT_HOOK_ENABLED;
    uio_event.mask = 0x00;

    // Fire the hook start event.
    dispatch_event(&uio_event);
    consumed = uio_event.reserved & 0x01;

    return consumed;
}

bool dispatch_hook_disabled(uint64_t timestamp) {
    bool consumed = false;

    // Populate the hook stop event.
    uio_event.time = timestamp;
    uio_event.reserved = 0x00;

    uio_event.type = EVENT_HOOK_DISABLED;
    uio_event.mask = 0x00;

    // Fire the hook stop event.
    dispatch_event(&uio_event);

    // Deinitialize native input helper functions.
    unload_input_helper();
    consumed = uio_event.reserved & 0x01;

    return consumed;
}

bool dispatch_key_press(uint64_t timestamp, CGEventRef event_ref) {
    bool consumed = false;

    UInt64 keycode = CGEventGetIntegerValueField(event_ref, kCGKeyboardEventKeycode);

    // Populate key pressed event.
    uio_event.time = timestamp;
    uio_event.reserved = 0x00;

    uio_event.type = EVENT_KEY_PRESSED;
    uio_event.mask = get_modifiers();

    uio_event.data.keyboard.keycode = keycode_to_scancode(keycode);
    uio_event.data.keyboard.rawcode = keycode;
    uio_event.data.keyboard.keychar = CHAR_UNDEFINED;

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Key %#X pressed. (%#X)\n",
            __FUNCTION__, __LINE__,
            uio_event.data.keyboard.keycode, uio_event.data.keyboard.rawcode);

    // Fire key pressed event.
    dispatch_event(&uio_event);
    consumed = uio_event.reserved & 0x01;

    // If the pressed event was not consumed...
    if (!consumed) {
        tis_keycode_message->event = event_ref;
        tis_keycode_message->length = 0;
        bool is_runloop_main = CFEqual(event_loop, CFRunLoopGetMain());

        if (dispatch_sync_f_f != NULL && dispatch_main_queue_s != NULL && !is_runloop_main) {
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Using dispatch_sync_f for key typed events.\n",
                    __FUNCTION__, __LINE__);
            (*dispatch_sync_f_f)(dispatch_main_queue_s, tis_keycode_message, &keycode_to_lookup);
        }
        #ifdef USE_APPLICATION_SERVICES
        else if (!is_runloop_main) {
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Using CFRunLoopWakeUp for key typed events.\n",
                    __FUNCTION__, __LINE__);

            // Lock for code dealing with the main runloop.
            pthread_mutex_lock(&main_runloop_mutex);

            // Check to see if the main runloop is still running.
            // TODO I would rather this be a check on hook_enable(),
            // but it makes the usage complicated by requiring a separate
            // thread for the main runloop and hook registration.
            CFStringRef mode = CFRunLoopCopyCurrentMode(CFRunLoopGetMain());
            if (mode != NULL) {
                CFRelease(mode);

                // Lookup the Unicode representation for this event.
                //CFRunLoopSourceContext context = { .version = 0 };
                //CFRunLoopSourceGetContext(main_runloop_keycode->source, &context);

                // Get the run loop context info pointer.
                //TISKeycodeMessage *info = (TISKeycodeMessage *) context.info;

                // Set the event pointer.
                //info->event = event_ref;


                // Signal the custom source and wakeup the main runloop.
                CFRunLoopSourceSignal(main_runloop_keycode->source);
                CFRunLoopWakeUp(CFRunLoopGetMain());

                // Wait for a lock while the main runloop processes they key typed event.
                pthread_cond_wait(&main_runloop_cond, &main_runloop_mutex);
            }
            else {
                logger(LOG_LEVEL_WARN, "%s [%u]: Failed to signal RunLoop main!\n",
                        __FUNCTION__, __LINE__);
            }

            // Unlock for code dealing with the main runloop.
            pthread_mutex_unlock(&main_runloop_mutex);
        }
        #endif
        else {
            keycode_to_lookup(tis_keycode_message);
        }

        for (unsigned int i = 0; i < tis_keycode_message->length; i++) {
            // Populate key typed event.
            uio_event.time = timestamp;
            uio_event.reserved = 0x00;

            uio_event.type = EVENT_KEY_TYPED;
            uio_event.mask = get_modifiers();

            uio_event.data.keyboard.keycode = VC_UNDEFINED;
            uio_event.data.keyboard.rawcode = keycode;
            uio_event.data.keyboard.keychar = tis_keycode_message->buffer[i];

            logger(LOG_LEVEL_DEBUG, "%s [%u]: Key %#X typed. (%lc)\n",
                    __FUNCTION__, __LINE__,
                    uio_event.data.keyboard.keycode,
                    (wint_t) uio_event.data.keyboard.keychar);

            // Populate key typed event.
            dispatch_event(&uio_event);
            consumed = uio_event.reserved & 0x01;
        }
    }

    return consumed;
}

bool dispatch_key_release(uint64_t timestamp, CGEventRef event_ref) {
    bool consumed = false;

    UInt64 keycode = CGEventGetIntegerValueField(event_ref, kCGKeyboardEventKeycode);

    // Populate key released event.
    uio_event.time = timestamp;
    uio_event.reserved = 0x00;

    uio_event.type = EVENT_KEY_RELEASED;
    uio_event.mask = get_modifiers();

    uio_event.data.keyboard.keycode = keycode_to_scancode(keycode);
    uio_event.data.keyboard.rawcode = keycode;
    uio_event.data.keyboard.keychar = CHAR_UNDEFINED;

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Key %#X released. (%#X)\n",
            __FUNCTION__, __LINE__,
            uio_event.data.keyboard.keycode, uio_event.data.keyboard.rawcode);

    // Fire key released event.
    dispatch_event(&uio_event);
    consumed = uio_event.reserved & 0x01;

    return consumed;
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

bool dispatch_system_key(uint64_t timestamp, CGEventRef event_ref) {
    bool consumed = false;

    if (CGEventGetType(event_ref) == NX_SYSDEFINED) {
        UInt32 subtype = 0;
        UInt32 data1 = 0;

        #ifdef USE_APPKIT
        bool is_runloop_main = CFEqual(event_loop, CFRunLoopGetMain());
        tis_event_message.event = event_ref;
        tis_event_message.subtype = 0;
        tis_event_message.data1 = 0;

        if (dispatch_sync_f_f != NULL && dispatch_main_queue_s != NULL && !is_runloop_main) {
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Using dispatch_sync_f for system key events.\n",
                    __FUNCTION__, __LINE__);

            (*dispatch_sync_f_f)(dispatch_main_queue_s, &tis_event_message, &obcj_message);
            subtype = tis_event_message.subtype;
            data1 = tis_event_message.data1;
        } else if (is_runloop_main) {
            obcj_message(tis_event_message);
            subtype = tis_event_message.subtype;
            data1 = tis_event_message.data1;
        } else {
        #endif
        // If we are not using ObjC, the only way I've found to access CGEvent->subtype and CGEvent>data1 is to
        // serialize the event and read the byte offsets.  I am not sure why, but CGEventCreateData appears to use
        // big-endian byte ordering even though all current apple architectures are little-endian.
        CFDataRef data_ref = CGEventCreateData(kCFAllocatorDefault, event_ref);
        if (data_ref == NULL) {
            logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to allocate memory for CGEventRef copy!\n",
                    __FUNCTION__, __LINE__);
            return;
        }

        if (CFDataGetLength(data_ref) < 132)
        {
            CFRelease(data_ref);
            logger(LOG_LEVEL_ERROR, "%s [%u]: Insufficient CFData range size!\n",
                    __FUNCTION__, __LINE__);
            return;
        }

        UInt8 *buffer = malloc(4);
        if (buffer == NULL) {
            CFRelease(data_ref);
            logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to allocate memory for CFData range buffer!\n",
                    __FUNCTION__, __LINE__);
            return;
        }

        CFDataGetBytes(data_ref, CFRangeMake(120, 4), buffer);
        subtype = CFSwapInt32BigToHost(*((UInt32 *) buffer));

        CFDataGetBytes(data_ref, CFRangeMake(128, 4), buffer);
        data1 = CFSwapInt32BigToHost(*((UInt32 *) buffer));

        free(buffer);
        CFRelease(data_ref);
        #ifdef USE_APPKIT
        }
        #endif

        if (subtype == 8) {
            int key_code = (data1 & 0xFFFF0000) >> 16;
            int key_flags = (data1 & 0xFFFF);
            int key_state = (key_flags & 0xFF00) >> 8;
            bool key_down = (key_state & 0x1) == 0;

            if (key_code == NX_KEYTYPE_CAPS_LOCK) {
                // It doesn't appear like we can modify the event coming in, so we will fabricate a new event.
                CGEventSourceRef src = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
                CGEventRef ns_event = CGEventCreateKeyboardEvent(src, kVK_CapsLock, key_down);
                CGEventSetFlags(ns_event, CGEventGetFlags(event_ref));

                if (key_down) {
                    consumed = dispatch_key_press(timestamp, ns_event);
                } else {
                    consumed = dispatch_key_release(timestamp, ns_event);
                }

                CFRelease(ns_event);
                CFRelease(src);
            } else if (key_code == NX_KEYTYPE_SOUND_UP) {
                // It doesn't appear like we can modify the event coming in, so we will fabricate a new event.
                CGEventSourceRef src = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
                CGEventRef ns_event = CGEventCreateKeyboardEvent(src, kVK_VolumeUp, key_down);
                CGEventSetFlags(ns_event, CGEventGetFlags(event_ref));

                if (key_down) {
                    consumed = dispatch_key_press(timestamp, ns_event);
                } else {
                    consumed = dispatch_key_release(timestamp, ns_event);
                }

                CFRelease(ns_event);
                CFRelease(src);
            } else if (key_code == NX_KEYTYPE_SOUND_DOWN) {
                // It doesn't appear like we can modify the event coming in, so we will fabricate a new event.
                CGEventSourceRef src = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
                CGEventRef ns_event = CGEventCreateKeyboardEvent(src, kVK_VolumeDown, key_down);
                CGEventSetFlags(ns_event, CGEventGetFlags(event_ref));

                if (key_down) {
                    consumed = dispatch_key_press(timestamp, ns_event);
                } else {
                    consumed = dispatch_key_release(timestamp, ns_event);
                }

                CFRelease(ns_event);
                CFRelease(src);
            } else if (key_code == NX_KEYTYPE_MUTE) {
                // It doesn't appear like we can modify the event coming in, so we will fabricate a new event.
                CGEventSourceRef src = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
                CGEventRef ns_event = CGEventCreateKeyboardEvent(src, kVK_Mute, key_down);
                CGEventSetFlags(ns_event, CGEventGetFlags(event_ref));

                if (key_down) {
                    consumed = dispatch_key_press(timestamp, ns_event);
                } else {
                    consumed = dispatch_key_release(timestamp, ns_event);
                }

                CFRelease(ns_event);
                CFRelease(src);
            } else if (key_code == NX_KEYTYPE_EJECT) {
                // It doesn't appear like we can modify the event coming in, so we will fabricate a new event.
                CGEventSourceRef src = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
                CGEventRef ns_event = CGEventCreateKeyboardEvent(src, kVK_NX_Eject, key_down);
                CGEventSetFlags(ns_event, CGEventGetFlags(event_ref));

                if (key_down) {
                    consumed = dispatch_key_press(timestamp, ns_event);
                } else {
                    consumed = dispatch_key_release(timestamp, ns_event);
                }

                CFRelease(ns_event);
                CFRelease(src);
            } else if (key_code == NX_KEYTYPE_PLAY) {
                // It doesn't appear like we can modify the event coming in, so we will fabricate a new event.
                CGEventSourceRef src = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
                CGEventRef ns_event = CGEventCreateKeyboardEvent(src, kVK_MEDIA_Play, key_down);
                CGEventSetFlags(ns_event, CGEventGetFlags(event_ref));

                if (key_down) {
                    consumed = dispatch_key_press(timestamp, ns_event);
                } else {
                    consumed = dispatch_key_release(timestamp, ns_event);
                }

                CFRelease(ns_event);
                CFRelease(src);
            } else if (key_code == NX_KEYTYPE_FAST) {
                // It doesn't appear like we can modify the event coming in, so we will fabricate a new event.
                CGEventSourceRef src = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
                CGEventRef ns_event = CGEventCreateKeyboardEvent(src, kVK_MEDIA_Next, key_down);
                CGEventSetFlags(ns_event, CGEventGetFlags(event_ref));

                if (key_down) {
                    consumed = dispatch_key_press(timestamp, ns_event);
                } else {
                    consumed = dispatch_key_release(timestamp, ns_event);
                }

                CFRelease(ns_event);
                CFRelease(src);
            } else if (key_code == NX_KEYTYPE_REWIND) {
                // It doesn't appear like we can modify the event coming in, so we will fabricate a new event.
                CGEventSourceRef src = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
                CGEventRef ns_event = CGEventCreateKeyboardEvent(src, kVK_MEDIA_Previous, key_down);
                CGEventSetFlags(ns_event, CGEventGetFlags(event_ref));

                if (key_down) {
                    consumed = dispatch_key_press(timestamp, ns_event);
                } else {
                    consumed = dispatch_key_release(timestamp, ns_event);
                }

                CFRelease(ns_event);
                CFRelease(src);
            }
        }
    }

    return consumed;
}

bool dispatch_modifier_change(uint64_t timestamp, CGEventRef event_ref) {
    bool consumed = false;

    CGEventFlags event_mask = CGEventGetFlags(event_ref);
    UInt64 keycode = CGEventGetIntegerValueField(event_ref, kCGKeyboardEventKeycode);

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Modifiers Changed for key %#X. (%#X)\n",
            __FUNCTION__, __LINE__, (unsigned long) keycode, (unsigned int) event_mask);

    /* Because Apple treats modifier keys differently than normal key
     * events, any changes to the modifier keys will require a key state
     * change to be fired manually.
     *
     * NOTE Left and right keyboard masks like NX_NEXTLSHIFTKEYMASK exist and
     * appear to be in use on Darwin, however they are removed by comment or
     * preprocessor with a note that reads "device-dependent (really?)."  To
     * ensure compatability, we will do this the verbose way.
     *
     * NOTE The masks for scroll and number lock are set in the key event.
     */
    if (keycode == kVK_Shift) {
        if (event_mask & kCGEventFlagMaskShift) {
            // Process as a key pressed event.
            set_modifier_mask(MASK_SHIFT_L);
            consumed = dispatch_key_press(timestamp, event_ref);
        } else {
            // Process as a key released event.
            unset_modifier_mask(MASK_SHIFT_L);
            consumed = dispatch_key_release(timestamp, event_ref);
        }
    } else if (keycode == kVK_Control) {
        if (event_mask & kCGEventFlagMaskControl) {
            // Process as a key pressed event.
            set_modifier_mask(MASK_CTRL_L);
            dispatch_key_press(timestamp, event_ref);
        } else {
            // Process as a key released event.
            unset_modifier_mask(MASK_CTRL_L);
            consumed = dispatch_key_release(timestamp, event_ref);
        }
    } else if (keycode == kVK_Command) {
        if (event_mask & kCGEventFlagMaskCommand) {
            // Process as a key pressed event.
            set_modifier_mask(MASK_META_L);
            consumed = dispatch_key_press(timestamp, event_ref);
        } else {
            // Process as a key released event.
            unset_modifier_mask(MASK_META_L);
            consumed = dispatch_key_release(timestamp, event_ref);
        }
    } else if (keycode == kVK_Option) {
        if (event_mask & kCGEventFlagMaskAlternate) {
            // Process as a key pressed event.
            set_modifier_mask(MASK_ALT_L);
            consumed = dispatch_key_press(timestamp, event_ref);
        } else {
            // Process as a key released event.
            unset_modifier_mask(MASK_ALT_L);
            consumed = dispatch_key_release(timestamp, event_ref);
        }
    } else if (keycode == kVK_RightShift) {
        if (event_mask & kCGEventFlagMaskShift) {
            // Process as a key pressed event.
            set_modifier_mask(MASK_SHIFT_R);
            consumed = dispatch_key_press(timestamp, event_ref);
        } else {
            // Process as a key released event.
            unset_modifier_mask(MASK_SHIFT_R);
            consumed = dispatch_key_release(timestamp, event_ref);
        }
    } else if (keycode == kVK_RightControl) {
        if (event_mask & kCGEventFlagMaskControl) {
            // Process as a key pressed event.
            set_modifier_mask(MASK_CTRL_R);
            consumed = dispatch_key_press(timestamp, event_ref);
        } else {
            // Process as a key released event.
            unset_modifier_mask(MASK_CTRL_R);
            consumed = dispatch_key_release(timestamp, event_ref);
        }
    } else if (keycode == kVK_RightCommand) {
        if (event_mask & kCGEventFlagMaskCommand) {
            // Process as a key pressed event.
            set_modifier_mask(MASK_META_R);
            consumed = dispatch_key_press(timestamp, event_ref);
        } else {
            // Process as a key released event.
            unset_modifier_mask(MASK_META_R);
            consumed = dispatch_key_release(timestamp, event_ref);
        }
    } else if (keycode == kVK_RightOption) {
        if (event_mask & kCGEventFlagMaskAlternate) {
            // Process as a key pressed event.
            set_modifier_mask(MASK_ALT_R);
            consumed = dispatch_key_press(timestamp, event_ref);
        } else {
            // Process as a key released event.
            unset_modifier_mask(MASK_ALT_R);
            consumed = dispatch_key_release(timestamp, event_ref);
        }
    } else if (keycode == kVK_CapsLock) {
        if (current_modifiers & MASK_CAPS_LOCK) {
            // Process as a key pressed event.
            unset_modifier_mask(MASK_CAPS_LOCK);
            // Key released handled by dispatch_system_key
        } else {
            // Process as a key released event.
            set_modifier_mask(MASK_CAPS_LOCK);
            // Key pressed handled by dispatch_system_key
        }
    }

    return consumed;
}

bool dispatch_button_press(uint64_t timestamp, CGEventRef event_ref, uint16_t button) {
    bool consumed = false;

    // Track the number of clicks.
    if (button == click_button && (long int) (timestamp - click_time) / TIMER_RESOLUTION_MS <= hook_get_multi_click_time()) {
        if (click_count < USHRT_MAX) {
            click_count++;
        }
        else {
            logger(LOG_LEVEL_WARN, "%s [%u]: Click count overflow detected!\n",
                    __FUNCTION__, __LINE__);
        }
    } else {
        // Reset the click count.
        click_count = 1;

        // Set the previous button.
        click_button = button;
    }

    // Save this events time to calculate the click_count.
    click_time = timestamp;

    CGPoint event_point = CGEventGetLocation(event_ref);

    // Populate mouse pressed event.
    uio_event.time = timestamp;
    uio_event.reserved = 0x00;

    uio_event.type = EVENT_MOUSE_PRESSED;
    uio_event.mask = get_modifiers();

    uio_event.data.mouse.button = button;
    uio_event.data.mouse.clicks = click_count;
    uio_event.data.mouse.x = event_point.x;
    uio_event.data.mouse.y = event_point.y;

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Button %u pressed %u time(s). (%u, %u)\n",
            __FUNCTION__, __LINE__,
            uio_event.data.mouse.button, uio_event.data.mouse.clicks,
            uio_event.data.mouse.x, uio_event.data.mouse.y);

    // Fire mouse pressed event.
    dispatch_event(&uio_event);
    consumed = uio_event.reserved & 0x01;

    return consumed;
}

void dispatch_button_release(uint64_t timestamp, CGEventRef event_ref, uint16_t button) {
    bool consumed = false;

    CGPoint event_point = CGEventGetLocation(event_ref);

    // Populate mouse released event.
    uio_event.time = timestamp;
    uio_event.reserved = 0x00;

    uio_event.type = EVENT_MOUSE_RELEASED;
    uio_event.mask = get_modifiers();

    uio_event.data.mouse.button = button;
    uio_event.data.mouse.clicks = click_count;
    uio_event.data.mouse.x = event_point.x;
    uio_event.data.mouse.y = event_point.y;

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Button %u released %u time(s). (%u, %u)\n",
            __FUNCTION__, __LINE__,
            uio_event.data.mouse.button, uio_event.data.mouse.clicks,
            uio_event.data.mouse.x, uio_event.data.mouse.y);

    // Fire mouse released event.
    dispatch_event(&uio_event);
    consumed = uio_event.reserved & 0x01;

    // If the pressed event was not consumed...
    if (event.reserved ^ 0x01 && mouse_dragged != true) {
        // Populate mouse clicked event.
        uio_event.time = timestamp;
        uio_event.reserved = 0x00;

        uio_event.type = EVENT_MOUSE_CLICKED;
        uio_event.mask = get_modifiers();

        uio_event.data.mouse.button = button;
        uio_event.data.mouse.clicks = click_count;
        uio_event.data.mouse.x = event_point.x;
        uio_event.data.mouse.y = event_point.y;

        logger(LOG_LEVEL_DEBUG, "%s [%u]: Button %u clicked %u time(s). (%u, %u)\n",
                __FUNCTION__, __LINE__,
                uio_event.data.mouse.button, uio_event.data.mouse.clicks,
                uio_event.data.mouse.x, uio_event.data.mouse.y);

        // Fire mouse clicked event.
        dispatch_event(&uio_event);
        consumed = uio_event.reserved & 0x01; // TODO Should we track this flag for consumed events?
    }

    // Reset the number of clicks.
    if ((long int) (timestamp - click_time) / TIMER_RESOLUTION_MS > hook_get_multi_click_time()) {
        // Reset the click count.
        click_count = 0;
    }

    return consumed;
}

bool dispatch_mouse_move(uint64_t timestamp, CGEventRef event_ref) {
    bool consumed = false;

    // Reset the click count.
    if (click_count != 0 && (long int) (timestamp - click_time) / TIMER_RESOLUTION_MS > hook_get_multi_click_time()) {
        click_count = 0;
    }

    CGPoint event_point = CGEventGetLocation(event_ref);

    // Populate mouse motion event.
    uio_event.time = timestamp;
    uio_event.reserved = 0x00;

    if (mouse_dragged) {
        uio_event.type = EVENT_MOUSE_DRAGGED;
    }
    else {
        uio_event.type = EVENT_MOUSE_MOVED;
    }
    uio_event.mask = get_modifiers();

    uio_event.data.mouse.button = MOUSE_NOBUTTON;
    uio_event.data.mouse.clicks = click_count;
    uio_event.data.mouse.x = event_point.x;
    uio_event.data.mouse.y = event_point.y;

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Mouse %s to %u, %u.\n",
            __FUNCTION__, __LINE__, mouse_dragged ? "dragged" : "moved",
            uio_event.data.mouse.x, uio_event.data.mouse.y);

    // Fire mouse motion event.
    dispatch_event(&uio_event);
    consumed = uio_event.reserved & 0x01;

    return consumed;
}

bool dispatch_mouse_wheel(uint64_t timestamp, CGEventRef event_ref) {
    bool consumed = false;
    
    // Reset the click count and previous button.
    click_count = 1;
    click_button = MOUSE_NOBUTTON;

    // Check to see what axis was rotated, we only care about axis 1 for vertical rotation.
    // TODO Implement horizontal scrolling by examining axis 2.
    // NOTE kCGScrollWheelEventDeltaAxis3 is currently unused.
    if (CGEventGetIntegerValueField(event_ref, kCGScrollWheelEventDeltaAxis1) != 0
            || CGEventGetIntegerValueField(event_ref, kCGScrollWheelEventDeltaAxis2) != 0) {
        CGPoint event_point = CGEventGetLocation(event_ref);

        // Populate mouse wheel event.
        uio_event.time = timestamp;
        uio_event.reserved = 0x00;

        uio_event.type = EVENT_MOUSE_WHEEL;
        uio_event.mask = get_modifiers();

        uio_event.data.wheel.clicks = click_count;
        uio_event.data.wheel.x = event_point.x;
        uio_event.data.wheel.y = event_point.y;

        // TODO Figure out if kCGScrollWheelEventDeltaAxis2 causes mouse events with zero rotation.
        if (CGEventGetIntegerValueField(event_ref, kCGScrollWheelEventIsContinuous) == 0) {
            // Scrolling data is line-based.
            uio_event.data.wheel.type = WHEEL_BLOCK_SCROLL;
        } else {
            // Scrolling data is pixel-based.
            uio_event.data.wheel.type = WHEEL_UNIT_SCROLL;
        }

        // TODO The result of kCGScrollWheelEventIsContinuous may effect this value.
        // Calculate the amount based on the Point Delta / Event Delta.  Integer sign should always be homogeneous resulting in a positive result.
        // NOTE kCGScrollWheelEventFixedPtDeltaAxis1 a floating point value (+0.1/-0.1) that takes acceleration into account.
        // NOTE kCGScrollWheelEventPointDeltaAxis1 will not build on OS X < 10.5

        if (CGEventGetIntegerValueField(event_ref, kCGScrollWheelEventDeltaAxis1) != 0) {
            uio_event.data.wheel.amount = CGEventGetIntegerValueField(event_ref, kCGScrollWheelEventPointDeltaAxis1) / CGEventGetIntegerValueField(event_ref, kCGScrollWheelEventDeltaAxis1);

            // Scrolling data uses a fixed-point 16.16 signed integer format (Ex: 1.0 = 0x00010000).
            uio_event.data.wheel.rotation = CGEventGetIntegerValueField(event_ref, kCGScrollWheelEventDeltaAxis1) * -1;

        } else if (CGEventGetIntegerValueField(event_ref, kCGScrollWheelEventDeltaAxis2) != 0) {
            uio_event.data.wheel.amount = CGEventGetIntegerValueField(event_ref, kCGScrollWheelEventPointDeltaAxis2) / CGEventGetIntegerValueField(event_ref, kCGScrollWheelEventDeltaAxis2);

            // Scrolling data uses a fixed-point 16.16 signed integer format (Ex: 1.0 = 0x00010000).
            uio_event.data.wheel.rotation = CGEventGetIntegerValueField(event_ref, kCGScrollWheelEventDeltaAxis2) * -1;
        } else {
            //Fail Silently if a 3rd axis gets added without changing this section of code.
            uio_event.data.wheel.amount = 0;
            uio_event.data.wheel.rotation = 0;
        }


        if (CGEventGetIntegerValueField(event_ref, kCGScrollWheelEventDeltaAxis1) != 0) {
            // Wheel Rotated Up or Down.
            uio_event.data.wheel.direction = WHEEL_VERTICAL_DIRECTION;
        } else { // data->event.u.u.detail == WheelLeft || data->event.u.u.detail == WheelRight
            // Wheel Rotated Left or Right.
            uio_event.data.wheel.direction = WHEEL_HORIZONTAL_DIRECTION;
        }

        logger(LOG_LEVEL_DEBUG, "%s [%u]: Mouse wheel type %u, rotated %i units in the %u direction at %u, %u.\n",
                __FUNCTION__, __LINE__, event.data.wheel.type,
                uio_event.data.wheel.amount * uio_event.data.wheel.rotation,
                uio_event.data.wheel.direction,
                uio_event.data.wheel.x, uio_event.data.wheel.y);

        // Fire mouse wheel event.
        dispatch_event(&uio_event);
        consumed = uio_event.reserved & 0x01;
    }

    return consumed;
}
