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

#include <CoreFoundation/CoreFoundation.h>
#include <pthread.h>
#include <uiohook.h>

#include "darwin/main_runloop_helper.h"
#include "test_assert.h"

static pthread_mutex_t test_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool returned;
static int calls;

struct conversion {
    int value;
    bool result;
};

static void convert(void *context) {
    TEST_ASSERT(CFRunLoopGetCurrent() == CFRunLoopGetMain());
    struct conversion *message = context;
    message->value *= 2;
    calls++;
}

static void *caller(void *context) {
    struct conversion *message = context;
    message->result = main_runloop_call(convert, message);
    pthread_mutex_lock(&test_mutex);
    returned = true;
    pthread_mutex_unlock(&test_mutex);
    // End the main-thread test loop once completion has been published.
    CFRunLoopStop(CFRunLoopGetMain());
    return NULL;
}

static bool is_returned(void) {
    pthread_mutex_lock(&test_mutex);
    bool result = returned;
    pthread_mutex_unlock(&test_mutex);
    return result;
}

int main(void) {
    struct conversion message = { .value = 7 };
    TEST_ASSERT(!main_runloop_call(convert, &message));
    TEST_ASSERT(main_runloop_start() == UIOHOOK_SUCCESS);
    TEST_ASSERT(main_runloop_start() == UIOHOOK_FAILURE);
    TEST_ASSERT(main_runloop_call(convert, &message));
    TEST_ASSERT(message.value == 14 && calls == 1);

    // A worker conversion runs on the actual main runloop, without a hook.
    pthread_t thread;
    TEST_ASSERT(pthread_create(&thread, NULL, caller, &message) == 0);
    while (!is_returned()) {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, true);
    }
    TEST_ASSERT(pthread_join(thread, NULL) == 0);
    TEST_ASSERT(message.result && message.value == 28 && calls == 2);

    // Leave the scheduler unserviced: timeout withdraws the stack-backed message.
    returned = false;
    TEST_ASSERT(pthread_create(&thread, NULL, caller, &message) == 0);
    TEST_ASSERT(pthread_join(thread, NULL) == 0);
    TEST_ASSERT(!message.result && message.value == 28 && calls == 2);
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, true);
    TEST_ASSERT(calls == 2);

    main_runloop_stop();
    TEST_ASSERT(!main_runloop_call(convert, &message));
    main_runloop_destroy();
    main_runloop_destroy();
    TEST_ASSERT(main_runloop_start() == UIOHOOK_SUCCESS);
    TEST_ASSERT(main_runloop_call(convert, &message));
    TEST_ASSERT(message.value == 56 && calls == 3);
    main_runloop_destroy();
    return EXIT_SUCCESS;
}
