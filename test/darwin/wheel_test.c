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

#include <ApplicationServices/ApplicationServices.h>
#include <uiohook.h>

#include "dispatch_event.h"
#include "input_helper.h"
#include "test_assert.h"

static CGEventRef posted;
static unsigned int posts, captures;
static uiohook_event captured[8];
static bool consume_horizontal;

// Intercept the final native post. Event construction and field access use
// local Core Graphics objects; no event reaches the desktop or needs a hook.
void CGEventPost(CGEventTapLocation tap, CGEventRef event) {
    TEST_ASSERT(tap == kCGHIDEventTap);
    if (posted != NULL) {
        CFRelease(posted);
    }
    posted = (CGEventRef) CFRetain(event);
    posts++;
}

UniCharCount event_to_unicode(CGEventRef event, UniChar *buffer, UniCharCount size) {
    (void) event; (void) buffer; (void) size;
    TEST_ASSERT(false);
    return 0;
}

void event_to_objc(CGEventRef event, UInt32 *subtype, UInt32 *data) {
    (void) event; (void) subtype; (void) data;
    TEST_ASSERT(false);
}

int load_input_helper(void) { TEST_ASSERT(false); return UIOHOOK_FAILURE; }
void unload_input_helper(void) { TEST_ASSERT(false); }
UIOHOOK_API long int hook_get_multi_click_time(void) { return 500; }

static void capture(uiohook_event *const event, void *data) {
    (void) data;
    TEST_ASSERT(captures < sizeof(captured) / sizeof(captured[0]));
    captured[captures++] = *event;
    if (consume_horizontal && event->data.wheel.direction == WHEEL_HORIZONTAL_DIRECTION) {
        event->mask |= MASK_CONSUMED;
    }
}

int main(void) {
    hook_set_dispatch_proc(capture, NULL);
    CGEventRef native = CGEventCreateScrollWheelEvent(NULL, kCGScrollEventUnitLine,
            (CGWheelCount) 2, 1, -2);
    TEST_ASSERT(native != NULL);
    TEST_ASSERT(!dispatch_mouse_wheel(1, native));
    TEST_ASSERT(captures == 2);
    TEST_ASSERT(captured[0].data.wheel.rotation == 120 && captured[0].data.wheel.delta == 120);
    TEST_ASSERT(captured[1].data.wheel.rotation == -240);
    TEST_ASSERT(captured[1].data.wheel.direction == WHEEL_HORIZONTAL_DIRECTION);
    TEST_ASSERT(captured[0].data.wheel.amount == 1);
    TEST_ASSERT(hook_post_event(&captured[1]) == UIOHOOK_SUCCESS);
    TEST_ASSERT(CGEventGetIntegerValueField(posted, kCGScrollWheelEventDeltaAxis1) == 0);
    TEST_ASSERT(CGEventGetIntegerValueField(posted, kCGScrollWheelEventDeltaAxis2) == -2);
    CFRelease(native);

    native = CGEventCreateScrollWheelEvent(NULL, kCGScrollEventUnitPixel,
            (CGWheelCount) 2, 1, -1);
    TEST_ASSERT(native != NULL);
    // Precision fields must be honored even when rounded line deltas are zero.
    CGEventSetIntegerValueField(native, kCGScrollWheelEventDeltaAxis1, 0);
    CGEventSetIntegerValueField(native, kCGScrollWheelEventDeltaAxis2, 0);
    CGEventSetIntegerValueField(native, kCGScrollWheelEventPointDeltaAxis1, 2);
    CGEventSetIntegerValueField(native, kCGScrollWheelEventPointDeltaAxis2, -3);
    consume_horizontal = true;
    TEST_ASSERT(dispatch_mouse_wheel(2, native));
    TEST_ASSERT(captures == 4);
    TEST_ASSERT(captured[2].data.wheel.type == WHEEL_BLOCK_SCROLL);
    TEST_ASSERT(captured[2].data.wheel.delta == 1 && captured[2].data.wheel.rotation == 2);
    TEST_ASSERT(captured[3].data.wheel.rotation == -3);
    TEST_ASSERT(hook_post_event(&captured[3]) == UIOHOOK_SUCCESS);
    TEST_ASSERT(CGEventGetIntegerValueField(posted, kCGScrollWheelEventIsContinuous) != 0);
    TEST_ASSERT(CGEventGetIntegerValueField(posted, kCGScrollWheelEventPointDeltaAxis2) == -3);
    CFRelease(native);

    // Fixed-point line movement can also survive a zero rounded line delta.
    native = CGEventCreateScrollWheelEvent(NULL, kCGScrollEventUnitLine,
            (CGWheelCount) 2, 0, 0);
    TEST_ASSERT(native != NULL);
    CGEventSetDoubleValueField(native, kCGScrollWheelEventFixedPtDeltaAxis1, 0.125);
    TEST_ASSERT(!dispatch_mouse_wheel(3, native));
    TEST_ASSERT(captures == 5 && captured[4].data.wheel.rotation == 15);
    CFRelease(native);

    uiohook_event event = captured[2];
    event.data.wheel.amount = 0;
    event.data.wheel.rotation = 1;
    event.data.wheel.delta = 2;
    unsigned int previous = posts;
    TEST_ASSERT(hook_post_event(&event) == UIOHOOK_SUCCESS && posts == previous);
    TEST_ASSERT(hook_post_event(&event) == UIOHOOK_SUCCESS && posts == previous + 1);
    event.data.wheel.delta = 0;
    TEST_ASSERT(hook_post_event(&event) == UIOHOOK_FAILURE);
    CFRelease(posted);
    return EXIT_SUCCESS;
}
