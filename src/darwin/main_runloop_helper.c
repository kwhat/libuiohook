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
#include <stdint.h>
#include <stdlib.h>
#include <uiohook.h>

#include "main_runloop_helper.h"

// Pending requests expire if the host main runloop is not servicing this source.
#define MAIN_RUNLOOP_TIMEOUT_MS 250

struct main_runloop_info {
    CFRunLoopSourceRef source;
    uint64_t generation;
};

static pthread_mutex_t helper_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct main_runloop_info *main_info;

static void wake_main_runloop(void *context) {
    struct main_runloop_info *info = context;
    CFRunLoopSourceSignal(info->source);
    CFRunLoopWakeUp(CFRunLoopGetMain());
}

static void perform_main_runloop(void *context) {
    uint64_t generation = (uint64_t) (uintptr_t) context;
    while (runloop_queue_perform(generation)) {
    }
}

// helper_mutex is held; partial allocation is safe because fields start at zero.
static void destroy_main_info(void) {
    if (main_info != NULL) {
        runloop_queue_stop(main_info->generation);
        CFRunLoopRef loop = CFRunLoopGetMain();
        if (main_info->source != NULL) {
            CFRunLoopSourceInvalidate(main_info->source);
            CFRunLoopRemoveSource(loop, main_info->source, kCFRunLoopCommonModes);
            CFRelease(main_info->source);
        }
        free(main_info);
        main_info = NULL;
    }
}

int main_runloop_start(void) {
    pthread_mutex_lock(&helper_mutex);
    if (main_info != NULL) {
        pthread_mutex_unlock(&helper_mutex);
        return UIOHOOK_FAILURE;
    }
    main_info = calloc(1, sizeof(*main_info));
    if (main_info == NULL) {
        pthread_mutex_unlock(&helper_mutex);
        return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }

    main_info->generation = runloop_queue_start(wake_main_runloop, main_info);
    int status = UIOHOOK_SUCCESS;
    if (main_info->generation == 0) {
        status = UIOHOOK_FAILURE;
    } else {
        // Immutable callbacks and generation tokens; no source-context mutation.
        CFRunLoopSourceContext source_context = { 0 };
        source_context.info = (void *) (uintptr_t) main_info->generation;
        source_context.perform = perform_main_runloop;
        main_info->source = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &source_context);
        if (main_info->source == NULL) {
            status = UIOHOOK_ERROR_CREATE_RUN_LOOP_SOURCE;
        }
    }
    if (status == UIOHOOK_SUCCESS) {
        CFRunLoopAddSource(CFRunLoopGetMain(), main_info->source, kCFRunLoopCommonModes);
    } else {
        destroy_main_info();
    }
    pthread_mutex_unlock(&helper_mutex);
    return status;
}

void main_runloop_stop(void) {
    pthread_mutex_lock(&helper_mutex);
    if (main_info != NULL) {
        runloop_queue_stop(main_info->generation);
    }
    pthread_mutex_unlock(&helper_mutex);
}

void main_runloop_destroy(void) {
    pthread_mutex_lock(&helper_mutex);
    destroy_main_info();
    pthread_mutex_unlock(&helper_mutex);
}

bool main_runloop_call(runloop_proc proc, void *context) {
    pthread_mutex_lock(&helper_mutex);
    uint64_t generation = main_info != NULL ? main_info->generation : 0;
    pthread_mutex_unlock(&helper_mutex);
    return runloop_queue_call(generation, proc, context,
            CFRunLoopGetCurrent() == CFRunLoopGetMain(), MAIN_RUNLOOP_TIMEOUT_MS);
}
