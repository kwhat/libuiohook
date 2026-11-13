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

#ifdef USE_APPLICATION_SERVICES
#include <CoreFoundation/CoreFoundation.h>
#endif

#include <dlfcn.h>
#include <stdbool.h>
#include <uiohook.h>

#include "dispatch_event.h"
#include "input_helper.h"
#include "input_state.h"
#include "logger.h"
#include "mouse_helper.h"
#include "runloop_helper.h"

#ifdef USE_APPKIT
#include <objc/objc.h>
#include <objc/objc-runtime.h>
#endif

#ifdef USE_APPLICATION_SERVICES
// Current dead key state.
static UInt32 deadkey_state;

// Input source data for the keyboard.
static TISInputSourceRef prev_keyboard_layout = NULL;
#endif

// These are the structures used to pass messages to the main runloop.
typedef struct {
    CGEventRef event;
    UniChar *buffer;
    UniCharCount size;
    UniCharCount length;
} TISKeycodeMessage;

typedef struct {
    CGEventRef event;
    UInt32 subtype;
    UInt32 data1;
} TISObjCMessage;

bool is_accessibility_enabled() {
    bool is_enabled = false;

    // Dynamically load the application services framework for examination.
    Boolean (*AXIsProcessTrustedWithOptions_t)(CFDictionaryRef);
    *(void **) (&AXIsProcessTrustedWithOptions_t) = dlsym(RTLD_DEFAULT, "AXIsProcessTrustedWithOptions");
    const char *dlError = dlerror();
    if (AXIsProcessTrustedWithOptions_t != NULL) {
        // Check for property CFStringRef kAXTrustedCheckOptionPrompt
        void ** kAXTrustedCheckOptionPrompt_t = dlsym(RTLD_DEFAULT, "kAXTrustedCheckOptionPrompt");

        dlError = dlerror();
        if (dlError != NULL) {
            // Could not load the AXIsProcessTrustedWithOptions function!
            logger(LOG_LEVEL_WARN, "%s [%u]: %s.\n",
                    __FUNCTION__, __LINE__, dlError);
        } else if (kAXTrustedCheckOptionPrompt_t != NULL) {
            // New accessibility API 10.9 and later.
            const void * keys[] = { *kAXTrustedCheckOptionPrompt_t };
            const void * values[] = { kCFBooleanTrue };

            CFDictionaryRef options = CFDictionaryCreate(
                    kCFAllocatorDefault,
                    keys,
                    values,
                    sizeof(keys) / sizeof(*keys),
                    &kCFCopyStringDictionaryKeyCallBacks,
                    &kCFTypeDictionaryValueCallBacks);

            is_enabled = (*AXIsProcessTrustedWithOptions_t)(options);
        }
    } else {
        if (dlError != NULL) {
            logger(LOG_LEVEL_WARN, "%s [%u]: %s.\n",
                    __FUNCTION__, __LINE__, dlError);
        }

        logger(LOG_LEVEL_DEBUG, "%s [%u]: AXIsProcessTrustedWithOptions not found.\n",
                __FUNCTION__, __LINE__);

        logger(LOG_LEVEL_DEBUG, "%s [%u]: Falling back to AXAPIEnabled().\n",
                __FUNCTION__, __LINE__);
        
        // Old accessibility check 10.8 and older.
        Boolean (*AXAPIEnabled_f)();
        *(void **) (&AXAPIEnabled_f) = dlsym(RTLD_DEFAULT, "AXAPIEnabled");
        dlError = dlerror();
        if (dlError != NULL) {
            // Could not load the AXIsProcessTrustedWithOptions function!
            logger(LOG_LEVEL_WARN, "%s [%u]: %s.\n",
                    __FUNCTION__, __LINE__, dlError);
        } else if (AXAPIEnabled_f != NULL) {
            is_enabled = (*AXAPIEnabled_f)();
        }
    }

    return is_enabled;
}

// Initialize the modifier mask to the current modifiers.
static void initialize_modifiers() {
    if (CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, kVK_Shift)) {
        set_modifier_mask(MASK_SHIFT_L);
    }
    if (CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, kVK_RightShift)) {
        set_modifier_mask(MASK_SHIFT_R);
    }

    if (CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, kVK_Control)) {
        set_modifier_mask(MASK_CTRL_L);
    }
    if (CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, kVK_RightControl)) {
        set_modifier_mask(MASK_CTRL_R);
    }

    if (CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, kVK_Option)) {
        set_modifier_mask(MASK_ALT_L);
    }
    if (CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, kVK_RightOption)) {
        set_modifier_mask(MASK_ALT_R);
    }

    if (CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, kVK_Command)) {
        set_modifier_mask(MASK_META_L);
    }
    if (CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, kVK_RightCommand)) {
        set_modifier_mask(MASK_META_R);
    }

    if (CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState, kVK_LBUTTON)) {
        set_modifier_mask(MASK_BUTTON1);
    }
    if (CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState, kVK_RBUTTON)) {
        set_modifier_mask(MASK_BUTTON2);
    }

    if (CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState, kVK_MBUTTON)) {
        set_modifier_mask(MASK_BUTTON3);
    }
    if (CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState, kVK_XBUTTON1)) {
        set_modifier_mask(MASK_BUTTON4);
    }
    if (CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState, kVK_XBUTTON2)) {
        set_modifier_mask(MASK_BUTTON5);
    }

    if (CGEventSourceFlagsState(kCGEventSourceStateCombinedSessionState) & kCGEventFlagMaskAlphaShift) {
        set_modifier_mask(MASK_CAPS_LOCK);
    }
    // Best I can tell, OS X does not support Num or Scroll lock.
    unset_modifier_mask(MASK_NUM_LOCK);
    unset_modifier_mask(MASK_SCROLL_LOCK);
}

static void tis_message_to_nsevent(void *info) {
    TISObjCMessage *tis = (TISObjCMessage *) info;

    if (tis != NULL && tis->event != NULL) {
        tis->subtype = 0;
        tis->data1 = 0;

        if (CFEqual(CFRunLoopGetCurrent(), CFRunLoopGetMain())) {
            #ifdef USE_APPKIT
            // NOTE The following block must execute on the main runloop,
            // Ex: CFEqual(CFRunLoopGetCurrent(), CFRunLoopGetMain()) to avoid "Exception detected while handling key input"
            // and "TSMProcessRawKeyCode failed (-192)" errors.
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Using objc_msgSend for system key events.\n",
                    __FUNCTION__, __LINE__);

            // Contributed by Iván Munsuri Ibáñez <munsuri@gmail.com> and Alex <universailp@web.de>
            id (*eventWithCGEvent)(id, SEL, CGEventRef) = (id (*)(id, SEL, CGEventRef)) objc_msgSend;
            id event_data = eventWithCGEvent((id) objc_getClass("NSEvent"), sel_registerName("eventWithCGEvent:"), tis->event);

            UInt32 (*eventWithoutCGEvent)(id, SEL) = (UInt32 (*)(id, SEL)) objc_msgSend;
            tis->subtype = eventWithoutCGEvent(event_data, sel_registerName("subtype"));
            tis->data1 = eventWithoutCGEvent(event_data, sel_registerName("data1"));
            #else
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Using CFDataGetBytes for system key events.\n",
                    __FUNCTION__, __LINE__);

            // If we are not using ObjC, the only way I've found to access CGEvent->subtype and CGEvent>data1 is to
            // serialize the event and read the byte offsets.  I am not sure why, but CGEventCreateData appears to use
            // big-endian byte ordering even though all current apple architectures are little-endian.
            CFDataRef data_ref = CGEventCreateData(kCFAllocatorDefault, tis->event);
            if (data_ref != NULL) {
                if (CFDataGetLength(data_ref) >= 132) {
                    UInt8 buffer[4];
                    CFDataGetBytes(data_ref, CFRangeMake(120, 4), &buffer[0]);
                    tis->subtype = CFSwapInt32BigToHost(*((UInt32 *) &buffer));

                    CFDataGetBytes(data_ref, CFRangeMake(128, 4), &buffer[0]);
                    tis->data1 = CFSwapInt32BigToHost(*((UInt32 *) &buffer));

                    CFRelease(data_ref);
                } else {
                    CFRelease(data_ref);
                    logger(LOG_LEVEL_ERROR, "%s [%u]: Insufficient CFData range size!\n",
                            __FUNCTION__, __LINE__);
                }
            } else {
                logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to allocate memory for CGEventRef copy!\n",
                        __FUNCTION__, __LINE__);
            }
            #endif
        }
    }
}

void event_to_objc(CGEventRef event_ref, UInt32 *subtype, UInt32 *data1) {
    TISObjCMessage message = { .event = event_ref, .subtype = 0, .data1 = 0 };
    if (!main_runloop_call(tis_message_to_nsevent, &message)) {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Main-runloop conversion cancelled.\n",
                __FUNCTION__, __LINE__);
    }
    *subtype = message.subtype;
    *data1 = message.data1;
}

// Perform Unicode lookup on the main thread through the shared request queue.
static void tis_message_to_unicode(void *info) {
    TISKeycodeMessage *tis = (TISKeycodeMessage *) info;

     if (tis != NULL && tis->event != NULL) {
        tis->length = 0;

        #ifdef USE_APPLICATION_SERVICES
        if (CFEqual(CFRunLoopGetCurrent(), CFRunLoopGetMain())) {
            // NOTE The following block must execute on the main runloop,
            // Ex: CFEqual(CFRunLoopGetCurrent(), CFRunLoopGetMain()) to avoid "Exception detected while handling key input"
            // and "TSMProcessRawKeyCode failed (-192)" errors.
            TISInputSourceRef curr_keyboard_layout = TISCopyCurrentKeyboardLayoutInputSource();
            if (curr_keyboard_layout != NULL && CFGetTypeID(curr_keyboard_layout) == TISInputSourceGetTypeID()) {

                const CFDataRef data = (CFDataRef) TISGetInputSourceProperty(curr_keyboard_layout, kTISPropertyUnicodeKeyLayoutData);
                if (data != NULL && CFGetTypeID(data) == CFDataGetTypeID() && CFDataGetLength(data) > 0) {

                    const UCKeyboardLayout *keyboard_layout = (const UCKeyboardLayout *) CFDataGetBytePtr(data);
                    if (keyboard_layout != NULL) {
                        //Extract keycode and modifier information.
                        CGKeyCode keycode = CGEventGetIntegerValueField(tis->event, kCGKeyboardEventKeycode);
                        CGEventFlags modifiers = CGEventGetFlags(tis->event);

                        // Ignore Command and Control shortcuts, retaining Option for
                        // layout-specific characters and dead-key combinations.
                        static const CGEventFlags cmd_modifiers = kCGEventFlagMaskCommand | kCGEventFlagMaskControl;
                        modifiers &= ~cmd_modifiers;

                        // I don't know why but UCKeyTranslate does not process the
                        // kCGEventFlagMaskAlphaShift (A.K.A. Caps Lock Mask) correctly.
                        // We need to basically turn off the mask and process the capital
                        // letters after UCKeyTranslate().
                        bool is_caps_lock = modifiers & kCGEventFlagMaskAlphaShift;
                        modifiers &= ~kCGEventFlagMaskAlphaShift;

                        // Run the translation with the saved deadkey_state.
                        OSStatus status = UCKeyTranslate(
                                keyboard_layout,
                                keycode,
                                kUCKeyActionDown, //kUCKeyActionDisplay,
                                (modifiers >> 16) & 0xFF, //(modifiers >> 16) & 0xFF, || (modifiers >> 8) & 0xFF,
                                LMGetKbdType(),
                                kNilOptions, //kNilOptions, //kUCKeyTranslateNoDeadKeysMask
                                &deadkey_state,
                                tis->size,
                                &(tis->length),
                                tis->buffer);

                        if (status == noErr && tis->length > 0) {
                            if (is_caps_lock) {
                                // We *had* a caps lock mask so we need to convert to uppercase.
                                CFMutableStringRef keytxt = CFStringCreateMutableWithExternalCharactersNoCopy(
                                    kCFAllocatorDefault, tis->buffer, tis->length, tis->size, kCFAllocatorNull
                                );

                                if (keytxt != NULL) {
                                    CFLocaleRef locale = CFLocaleCopyCurrent();
                                    CFStringUppercase(keytxt, locale);
                                    CFRelease(locale);
                                    CFRelease(keytxt);
                                } else {
                                    // There was an problem creating the CFMutableStringRef.
                                    tis->length = 0;
                                }
                            }
                        } else {
                            // Make sure the tis->buffer tis->length is zero if an error occurred.
                            tis->length = 0;
                        }
                    }
                }
            }

            // Check if the keyboard layout has changed to see if the dead key state needs to be discarded.
            if (prev_keyboard_layout != NULL && curr_keyboard_layout != NULL && CFEqual(curr_keyboard_layout, prev_keyboard_layout) == false) {
                deadkey_state = 0x00;
            }

            // Release the previous keyboard layout.
            if (prev_keyboard_layout != NULL) {
                CFRelease(prev_keyboard_layout);
                prev_keyboard_layout = NULL;
            }

            // Set the previous keyboard layout to the current layout.
            if (curr_keyboard_layout != NULL) {
                prev_keyboard_layout = curr_keyboard_layout;
            }
        }
        #else
        CGEventKeyboardGetUnicodeString(tis->event, tis->size, &(tis->length), tis->buffer);
        #endif

        // The following codes should not be processed because they are invalid.
        if (tis->length == 1) {
            switch (tis->buffer[0]) {
                case 0x01: // Home
                case 0x04: // End
                case 0x05: // Help Key
                case 0x10: // Function Keys
                case 0x0B: // Page Up
                case 0x0C: // Page Down
                case 0x1F: // Volume Up
                    tis->length = 0;
            }
        }
    }
}

UniCharCount event_to_unicode(CGEventRef event_ref, UniChar *buffer, UniCharCount size) {
    TISKeycodeMessage message = {
        .event = event_ref, .buffer = buffer, .size = size, .length = 0
    };
    if (!main_runloop_call(tis_message_to_unicode, &message)) {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Main-runloop conversion cancelled.\n",
                __FUNCTION__, __LINE__);
    }
    return message.length;
}

int load_input_helper() {
    int status = main_runloop_start();
    if (status != UIOHOOK_SUCCESS) {
        return status;
    }
    #ifdef USE_APPLICATION_SERVICES
    // Start with a fresh dead key state.
    deadkey_state = 0;
    #endif

    // Initialize the current state of the modifiers.
    initialize_modifiers();

    return UIOHOOK_SUCCESS;
}

void unload_input_helper() {
    // Pending requests are cancelled; a running conversion has completed before
    // its hook-thread caller can reach teardown and release the cached layout.
    main_runloop_destroy();

    #ifdef USE_APPLICATION_SERVICES
    if (prev_keyboard_layout != NULL) {
        // Cleanup tracking of the previous layout.
        CFRelease(prev_keyboard_layout);
        prev_keyboard_layout = NULL;
    }
    #endif
}
