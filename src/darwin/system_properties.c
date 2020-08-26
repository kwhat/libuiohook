/* UIOHook: Cross-platform keyboard and mouse hooking from userland
 * Copyright (C) 2006-2020 Alexander Barker.  All Rights Received.
 * https://github.com/kwhat/uiohook/
 *
 * UIOHook is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * UIOHook is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef USE_CARBON_LEGACY
#include <Carbon/Carbon.h>
#endif

#if defined(USE_APPLICATION_SERVICES) || defined(USE_IOKIT)
#include <CoreFoundation/CoreFoundation.h>
#endif

#ifdef USE_IOKIT
#include <IOKit/hidsystem/IOHIDLib.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#endif

#include <stdbool.h>
#include <uiohook.h>

#include "logger.h"
#include "input_helper.h"

#ifdef USE_IOKIT
static io_connect_t connection;
#endif

/* The following function was contributed by Anthony Liguori Jan 18 2015.
 * https://github.com/kwhat/libuiohook/pull/18
 */
UIOHOOK_API screen_data* hook_create_screen_info(unsigned char *count) {
    CGError status = kCGErrorFailure;
    screen_data* screens = NULL;

    // Initialize count to zero.
    *count = 0;

    // Allocate memory to hold each display id.  We will just allocate our MAX
    // because its only about 1K of memory.
    // TODO This can probably be realistically cut to something like 16 or 32....
    // If you have more than 32 monitors, send me a picture and make a donation ;)
    CGDirectDisplayID *display_ids = malloc(sizeof(CGDirectDisplayID) * UCHAR_MAX);
    if (display_ids != NULL) {
        // NOTE Pass UCHAR_MAX to make sure uint32_t doesn't overflow uint8_t.
        // TOOD Test/Check whether CGGetOnlineDisplayList is more suitable...
        status = CGGetActiveDisplayList(UCHAR_MAX, display_ids, (uint32_t *) count);

        // If there is no error and at least one monitor.
        if (status == kCGErrorSuccess && *count > 0) {
            logger(LOG_LEVEL_DEBUG, "%s [%u]: CGGetActiveDisplayList: %li.\n",
                    __FUNCTION__, __LINE__, *count);

            // Allocate memory for the number of screens found.
            screens = malloc(sizeof(screen_data) * (*count));
            if (screens != NULL) {
                for (uint8_t i = 0; i < *count; i++) {
                    //size_t width = CGDisplayPixelsWide(display_ids[i]);
                    //size_t height = CGDisplayPixelsHigh(display_ids[i]);
                    CGRect boundsDisp = CGDisplayBounds(display_ids[i]);
                    if (boundsDisp.size.width > 0 && boundsDisp.size.height > 0) {
                        screens[i] = (screen_data) {
                            .number = i + 1,
                            //TODO: make sure we follow the same convention for the origin
                            //in all other platform implementations (upper-left)
                            //TODO: document the approach with examples in order to show different
                            //cases -> different resolutions (secondary monitors origin might be
                            //negative)
                            .x = boundsDisp.origin.x,
                            .y = boundsDisp.origin.y,
                            .width = boundsDisp.size.width,
                            .height = boundsDisp.size.height
                        };
                }
                }
            }
        } else {
            logger(LOG_LEVEL_DEBUG, "%s [%u]: multiple_get_screen_info failed: %ld. Fallback.\n",
                    __FUNCTION__, __LINE__, status);

            size_t width = CGDisplayPixelsWide(CGMainDisplayID());
            size_t height = CGDisplayPixelsHigh(CGMainDisplayID());

            if (width > 0 && height > 0) {
                screens = malloc(sizeof(screen_data));

                if (screens != NULL) {
                    *count = 1;
                    screens[0] = (screen_data) {
                        .number = 1,
                        .x = 0,
                        .y = 0,
                        .width = width,
                        .height = height
                    };
                }
            }
        }

        // Free the id's after we are done.
        free(display_ids);
    }

    return screens;
}

/*
 * Apple's documentation is not very good.  I was finally able to find this
 * information after many hours of googling.  Value is the slider value in the
 * system preferences. That value * 15 is the rate in MS.  66 / the value is the
 * chars per second rate.
 *
 * Value    MS      Char/Sec
 *
 * 1        15      66        * Out of standard range *
 * 2        30      33
 * 6        90      11
 * 12       180     5.5
 * 30       450     2.2
 * 60       900     1.1
 * 90       1350    0.73
 * 120      1800    0.55
 *
 * V = MS / 15
 * V = 66 / CharSec
 *
 * MS = V * 15
 * MS = (66 / CharSec) * 15
 *
 * CharSec = 66 / V
 * CharSec = 66 / (MS / 15)
 */
UIOHOOK_API long int hook_get_auto_repeat_rate() {
    #if defined USE_IOKIT || defined USE_APPLICATION_SERVICES || defined USE_CARBON_LEGACY
    bool successful = false;
    SInt64 rate;
    #endif

    long int value = -1;

    #ifdef USE_IOKIT
    if (!successful) {
        CFTypeRef cf_type = NULL;
        kern_return_t kern_return = IOHIDCopyCFTypeParameter(connection, CFSTR(kIOHIDKeyRepeatKey), &cf_type);
        if (kern_return == kIOReturnSuccess) {
            if (cf_type != NULL) {
                if (CFGetTypeID(cf_type) == CFNumberGetTypeID()) {
                    if (CFNumberGetValue((CFNumberRef) cf_type, kCFNumberSInt64Type, &rate)) {
                        /* This is in some undefined unit of time that if we happen
                         * to multiply by 900 gives us the time in milliseconds. We
                         * add 0.5 to the result so that when we cast to long we
                         * actually get a rounded result.  Saves the math.h depend.
                         *
                         *    33,333,333.0 / 1000.0 / 1000.0 / 1000.0 == 0.033333333    * Fast *
                         *   100,000,000.0 / 1000.0 / 1000.0 / 1000.0 == 0.1
                         *   200,000,000.0 / 1000.0 / 1000.0 / 1000.0 == 0.2
                         *   500,000,000.0 / 1000.0 / 1000.0 / 1000.0 == 0.5
                         * 1,000,000,000.0 / 1000.0 / 1000.0 / 1000.0 == 1
                         * 1,500,000,000.0 / 1000.0 / 1000.0 / 1000.0 == 1.5
                         * 2,000,000,000.0 / 1000.0 / 1000.0 / 1000.0 == 2                * Slow *
                         *
                         * TODO Try rate / 256 / 1000.0 / 1000.0 / 1000.0;
                         */
                        value = (long) (900.0 * ((double) rate) / 1000.0 / 1000.0 / 1000.0 + 0.5);
                        successful = true;

                        logger(LOG_LEVEL_DEBUG, "%s [%u]: IORegistryEntryCreateCFProperty: %li.\n",
                                __FUNCTION__, __LINE__, value);
                    }
                }
                
                CFRelease(cf_type);
            }
        }
    }
    #endif

    #ifdef USE_APPLICATION_SERVICES
    if (!successful) {
        CFTypeRef pref_val = CFPreferencesCopyValue(CFSTR("KeyRepeat"), kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
        if (pref_val != NULL) {
            if (CFGetTypeID(pref_val) == CFNumberGetTypeID() && CFNumberGetValue((CFNumberRef) pref_val, kCFNumberSInt32Type, &rate)) {
                // This is the slider value, we must multiply by 15 to convert to milliseconds.
                value = (long) rate * 15;
                successful = true;

                logger(LOG_LEVEL_DEBUG, "%s [%u]: CFPreferencesCopyValue: %li.\n",
                        __FUNCTION__, __LINE__, value);
            }
            
            CFRelease(pref_val);
        }
    }
    #endif

    #ifdef USE_CARBON_LEGACY
    if (!successful) {
        // Apple documentation states that value is in 'ticks'. I am not sure
        // what that means, but it looks a lot like the arbitrary slider value.
        rate = LMGetKeyRepThresh();
        if (rate > -1) {
            /* This is the slider value, we must multiply by 15 to convert to
             * milliseconds.
             */
            value = (long) rate * 15;
            successful = true;

            logger(LOG_LEVEL_DEBUG, "%s [%u]: LMGetKeyRepThresh: %li.\n",
                    __FUNCTION__, __LINE__, value);
        }
    }
    #endif

    return value;
}

UIOHOOK_API long int hook_get_auto_repeat_delay() {
    #if defined USE_IOKIT || defined USE_APPLICATION_SERVICES || defined USE_CARBON_LEGACY
    bool successful = false;
    SInt64 delay;
    #endif

    long int value = -1;

    #ifdef USE_IOKIT
    if (!successful) {
        CFTypeRef cf_type = NULL;
        kern_return_t kern_return = IOHIDCopyCFTypeParameter(connection, CFSTR(kIOHIDInitialKeyRepeatKey), &cf_type);
        if (kern_return == kIOReturnSuccess) {
            if (cf_type != NULL) {
                if (CFGetTypeID(cf_type) == CFNumberGetTypeID()) {
                    if (CFNumberGetValue((CFNumberRef) cf_type, kCFNumberSInt64Type, &delay)) {
                        /* This is in some undefined unit of time that if we happen
                         * to multiply by 900 gives us the time in milliseconds. We
                         * add 0.5 to the result so that when we cast to long we
                         * actually get a rounded result.  Saves the math.h depend.
                         *
                         *    33,333,333.0 / 1000.0 / 1000.0 / 1000.0 == 0.033333333    * Fast *
                         *   100,000,000.0 / 1000.0 / 1000.0 / 1000.0 == 0.1
                         *   200,000,000.0 / 1000.0 / 1000.0 / 1000.0 == 0.2
                         *   500,000,000.0 / 1000.0 / 1000.0 / 1000.0 == 0.5
                         * 1,000,000,000.0 / 1000.0 / 1000.0 / 1000.0 == 1
                         * 1,500,000,000.0 / 1000.0 / 1000.0 / 1000.0 == 1.5
                         * 2,000,000,000.0 / 1000.0 / 1000.0 / 1000.0 == 2              * Slow *
                         *
                         * TODO Try rate / 256 / 1000.0 / 1000.0 / 1000.0;
                         */
                        value = (long) (900.0 * ((double) delay) / 1000.0 / 1000.0 / 1000.0 + 0.5);
                        successful = true;

                        logger(LOG_LEVEL_DEBUG, "%s [%u]: IORegistryEntryCreateCFProperty: %li.\n",
                                __FUNCTION__, __LINE__, value);
                    }
                }

                CFRelease(cf_type);
            }
        }
    }
    #endif

    #ifdef USE_APPLICATION_SERVICES
    if (!successful) {
        CFTypeRef pref_val = CFPreferencesCopyValue(CFSTR("InitialKeyRepeat"), kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
        if (pref_val != NULL) {
            if (CFGetTypeID(pref_val) == CFNumberGetTypeID() && CFNumberGetValue((CFNumberRef) pref_val, kCFNumberSInt32Type, &delay)) {
                // This is the slider value, we must multiply by 15 to convert to
                // milliseconds.
                value = (long) delay * 15;
                successful = true;

                logger(LOG_LEVEL_DEBUG, "%s [%u]: CFPreferencesCopyValue: %li.\n",
                        __FUNCTION__, __LINE__, value);
            }

            CFRelease(pref_val);
        }
    }
    #endif

    #ifdef USE_CARBON_LEGACY
    if (!successful) {
        // Apple documentation states that value is in 'ticks'. I am not sure
        // what that means, but it looks a lot like the arbitrary slider value.
        delay = LMGetKeyThresh();
        if (delay > -1) {
            // This is the slider value, we must multiply by 15 to convert to
            // milliseconds.
            value = (long) delay * 15;
            successful = true;

            logger(LOG_LEVEL_DEBUG, "%s [%u]: LMGetKeyThresh: %li.\n",
                    __FUNCTION__, __LINE__, value);
        }
    }
    #endif

    return value;
}

UIOHOOK_API long int hook_get_pointer_acceleration_multiplier() {
    #if defined USE_IOKIT || defined USE_APPLICATION_SERVICES
    bool successful = false;
    SInt64 multiplier;
    #endif

    long int value = -1;

    #ifdef USE_IOKIT
    if (!successful) {
        CFTypeRef cf_type = NULL;
        kern_return_t kern_return = IOHIDCopyCFTypeParameter(connection, CFSTR(kIOHIDMouseAccelerationType), &cf_type);
        if (kern_return == kIOReturnSuccess) {
            if (cf_type != NULL) {
                if (CFGetTypeID(cf_type) == CFNumberGetTypeID()) {
                    if (CFNumberGetValue((CFNumberRef) cf_type, kCFNumberSInt64Type, &multiplier)) {
                        // Calculate the greatest common factor.
                        unsigned long denominator = 1000000, d = denominator;
                        unsigned long numerator = (((double) multiplier) / 65536.0) * denominator, gcf = numerator;

                        while (d != 0) {
                            unsigned long i = gcf % d;
                            gcf = d;
                            d = i;
                        }

                        value = denominator / gcf;
                        successful = true;

                        logger(LOG_LEVEL_DEBUG, "%s [%u]: IORegistryEntryCreateCFProperty: %li.\n",
                                __FUNCTION__, __LINE__, value);
                    }
                }

                CFRelease(cf_type);
            }
        }
    }
    #endif

    #ifdef USE_APPLICATION_SERVICES
    if (!successful) {
        CFTypeRef pref_val = CFPreferencesCopyValue(CFSTR("com.apple.mouse.scaling"), kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
        if (pref_val != NULL) {
            if (CFGetTypeID(pref_val) == CFNumberGetTypeID() && CFNumberGetValue((CFNumberRef) pref_val, kCFNumberSInt64Type, &multiplier)) {
                value = (long) multiplier;

                logger(LOG_LEVEL_DEBUG, "%s [%u]: CFPreferencesCopyValue: %li.\n",
                        __FUNCTION__, __LINE__, value);
            }

            CFRelease(pref_val);
        }
    }
    #endif

    return value;
}

UIOHOOK_API long int hook_get_pointer_acceleration_threshold() {
    #if defined USE_APPLICATION_SERVICES
    bool successful = false;
    SInt32 threshold;
    #endif

    long int value = -1;

    #ifdef USE_APPLICATION_SERVICES
    if (!successful) {
        CFTypeRef pref_val = CFPreferencesCopyValue(CFSTR("mouseDriverMaxSpeed"), CFSTR("com.apple.universalaccess"), kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
        if (pref_val != NULL) {
            if (CFGetTypeID(pref_val) == CFNumberGetTypeID() && CFNumberGetValue((CFNumberRef) pref_val, kCFNumberSInt32Type, &threshold)) {
                value = (long) threshold;

                logger(LOG_LEVEL_DEBUG, "%s [%u]: CFPreferencesCopyValue: %li.\n",
                        __FUNCTION__, __LINE__, value);
            }

            CFRelease(pref_val);
        }
    }
    #endif

    return value;
}

UIOHOOK_API long int hook_get_pointer_sensitivity() {
    #ifdef USE_IOKIT
    bool successful = false;
    SInt32 sensitivity;
    #endif

    long int value = -1;

    #ifdef USE_IOKIT
    if (!successful) {
        CFTypeRef cf_type = NULL;
        kern_return_t kern_return = IOHIDCopyCFTypeParameter(connection, CFSTR(kIOHIDMouseAccelerationType), &cf_type);
        if (kern_return == kIOReturnSuccess) {
            if (cf_type != NULL) {
                if (CFGetTypeID(cf_type) == CFNumberGetTypeID()) {
                    if (CFNumberGetValue((CFNumberRef) cf_type, kCFNumberSInt32Type, &sensitivity)) {
                        // Calculate the greatest common factor.
                        unsigned long denominator = 1000000, d = denominator;
                        unsigned long numerator = (((double) sensitivity) / 65536.0) * denominator, gcf = numerator;

                        while (d != 0) {
                            unsigned long i = gcf % d;
                            gcf = d;
                            d = i;
                        }

                        value = numerator / gcf;
                        successful = true;

                        logger(LOG_LEVEL_DEBUG, "%s [%u]: IORegistryEntryCreateCFProperty: %li.\n",
                                __FUNCTION__, __LINE__, value);
                    }
                }

                CFRelease(cf_type);
            }
        }
    }
    #endif

    return value;
}

UIOHOOK_API long int hook_get_multi_click_time() {
    #if defined USE_IOKIT || defined USE_APPLICATION_SERVICES || defined USE_CARBON_LEGACY
    bool successful = false;
    #if defined USE_IOKIT || defined USE_CARBON_LEGACY
    // This needs to be defined only if we have USE_IOKIT or USE_CARBON_LEGACY.
    SInt64 time;
    #endif
    #endif

    long int value = -1;

    #ifdef USE_IOKIT
    if (!successful) {
        CFTypeRef cf_type = NULL;
        kern_return_t kern_return = IOHIDCopyCFTypeParameter(connection, CFSTR(kIOHIDClickTimeKey), &cf_type);
        if (kern_return == kIOReturnSuccess) {
            if (cf_type != NULL) {
                if (CFGetTypeID(cf_type) == CFNumberGetTypeID()) {
                    if (CFNumberGetValue((CFNumberRef) cf_type, kCFNumberSInt64Type, &time)) {
                        /* This is in some undefined unit of time that if we happen
                         * to multiply by 900 gives us the time in milliseconds. We
                         * add 0.5 to the result so that when we cast to long we
                         * actually get a rounded result.  Saves the math.h depend.
                         */
                        value = (long) (900.0 * ((double) time) / 1000.0 / 1000.0 / 1000.0 + 0.5);
                        successful = true;

                        logger(LOG_LEVEL_DEBUG, "%s [%u]: IORegistryEntryCreateCFProperty: %li.\n",
                                __FUNCTION__, __LINE__, value);
                    }
                }

                CFRelease(cf_type);
            }
        }
    }
    #endif

    #ifdef USE_APPLICATION_SERVICES
    if (!successful) {
        Float32 clicktime;
        CFTypeRef pref_val = CFPreferencesCopyValue(CFSTR("com.apple.mouse.doubleClickThreshold"), kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
        if (pref_val != NULL) {
            if (CFGetTypeID(pref_val) == CFNumberGetTypeID() && CFNumberGetValue((CFNumberRef) pref_val, kCFNumberFloat32Type, &clicktime)) {
                /* This is in some undefined unit of time that if we happen
                 * to multiply by 900 gives us the time in milliseconds.  It is
                 * completely possible that this value is in seconds and should be
                 * multiplied by 1000 but because IOKit values are undocumented and
                 * I have no idea what a Carbon 'tick' is so there really is no way
                 * to confirm this.
                 */
                value = (long) (clicktime * 900);

                logger(LOG_LEVEL_DEBUG, "%s [%u]: CFPreferencesCopyValue: %li.\n",
                        __FUNCTION__, __LINE__, value);
            }

            CFRelease(pref_val);
        }
    }
    #endif

    #ifdef USE_CARBON_LEGACY
    if (!successful) {
        // Apple documentation states that value is in 'ticks'. I am not sure
        // what that means, but it looks a lot like the arbitrary slider value.
        time = GetDblTime();
        if (time > -1) {
            // This is the slider value, we must multiply by 15 to convert to
            // milliseconds.
            value = (long) time * 15;
            successful = true;

            logger(LOG_LEVEL_DEBUG, "%s [%u]: GetDblTime: %li.\n",
                    __FUNCTION__, __LINE__, value);
        }
    }
    #endif

    return value;
}


// Create a shared object constructor.
__attribute__ ((constructor))
void on_library_load() {
    #ifdef USE_IOKIT
    io_service_t service = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching(kIOHIDSystemClass));
    if (service) {
        kern_return_t kren_ret = IOServiceOpen(service, mach_task_self(), kIOHIDParamConnectType, &connection);
        if (kren_ret != kIOReturnSuccess) {
            logger(LOG_LEVEL_INFO, "%s [%u]: IOServiceOpen failure (%#X)!\n",
                    __FUNCTION__, __LINE__, kren_ret);
        }
    }
    #endif

    // Initialize Native Input Functions.
    load_input_helper();
}

// Create a shared object destructor.
__attribute__ ((destructor))
void on_library_unload() {
    // Disable the event hook.
    //hook_stop();

    // Cleanup native input functions.
    unload_input_helper();

    #ifdef USE_IOKIT
    if (connection) {
        kern_return_t kren_ret = IOServiceClose(connection);
        if (kren_ret != kIOReturnSuccess) {
            logger(LOG_LEVEL_INFO, "%s [%u]: IOServiceClose failure (%#X) %#X!\n",
                    __FUNCTION__, __LINE__, kren_ret, kIOReturnError);
        }
    }
    #endif
}
