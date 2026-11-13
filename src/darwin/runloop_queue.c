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

#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <sys/time.h>
#include "runloop_queue.h"

struct runloop_request {
    runloop_proc proc;
    void *context;
    bool started, finished, completed;
    struct runloop_request *next;
};

static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;
static struct runloop_request *head, *tail;
static runloop_proc wake_proc;
static void *wake_context;
static uint64_t queue_generation;
static unsigned int waiting, running;
static bool accepting;

uint64_t runloop_queue_start(runloop_proc wake, void *context) {
    pthread_mutex_lock(&queue_mutex);
    if (accepting || waiting != 0 || running != 0 || wake == NULL) {
        pthread_mutex_unlock(&queue_mutex);
        return 0;
    }
    if (++queue_generation == 0) {
        queue_generation++;
    }
    wake_proc = wake;
    wake_context = context;
    accepting = true;
    uint64_t generation = queue_generation;
    pthread_mutex_unlock(&queue_mutex);
    return generation;
}

void runloop_queue_stop(uint64_t generation) {
    pthread_mutex_lock(&queue_mutex);
    if (generation == queue_generation) {
        accepting = false;
        while (head != NULL) {
            struct runloop_request *request = head;
            head = request->next;
            request->finished = true;
        }
        tail = NULL;
        wake_proc = NULL;
        wake_context = NULL;
        pthread_cond_broadcast(&queue_cond);
    }
    pthread_mutex_unlock(&queue_mutex);
}

bool runloop_queue_call(uint64_t generation, runloop_proc proc, void *context, bool on_main_thread, unsigned int timeout_ms) {
    pthread_mutex_lock(&queue_mutex);
    if (!accepting || generation != queue_generation || proc == NULL) {
        pthread_mutex_unlock(&queue_mutex);
        return false;
    }

    if (on_main_thread) {
        running++;
        pthread_mutex_unlock(&queue_mutex);
        proc(context);
        pthread_mutex_lock(&queue_mutex);
        running--;
        pthread_cond_broadcast(&queue_cond);
        pthread_mutex_unlock(&queue_mutex);
        return true;
    }

    struct timeval now;
    gettimeofday(&now, NULL);
    struct timespec deadline = {
        .tv_sec = now.tv_sec + timeout_ms / 1000,
        .tv_nsec = now.tv_usec * 1000L + (timeout_ms % 1000) * 1000000L
    };
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000L;
    }
    struct runloop_request request = {
        .proc = proc, .context = context, .started = false, .finished = false,
        .completed = false, .next = NULL
    };
    if (tail == NULL) {
        head = &request;
    } else {
        tail->next = &request;
    }
    tail = &request;
    waiting++;
    // Stop takes the same mutex, so the scheduler context stays alive here.
    wake_proc(wake_context);
    while (!request.finished) {
        if (request.started) {
            // Running code may still access the caller's event and output buffer.
            pthread_cond_wait(&queue_cond, &queue_mutex);
        } else if (pthread_cond_timedwait(&queue_cond, &queue_mutex, &deadline) == ETIMEDOUT
                && !request.started && !request.finished) {
            // Withdraw this request before its stack storage can go out of scope.
            struct runloop_request *previous = NULL;
            struct runloop_request *current = head;
            while (current != &request) {
                previous = current;
                current = current->next;
            }
            if (previous == NULL) {
                head = request.next;
            } else {
                previous->next = request.next;
            }
            if (tail == &request) {
                tail = previous;
            }
            request.finished = true;
        }
    }
    waiting--;
    bool completed = request.completed;
    pthread_mutex_unlock(&queue_mutex);
    return completed;
}

bool runloop_queue_perform(uint64_t generation) {
    pthread_mutex_lock(&queue_mutex);
    if (!accepting || generation != queue_generation || head == NULL) {
        pthread_mutex_unlock(&queue_mutex);
        return false;
    }
    struct runloop_request *request = head;
    head = request->next;
    if (head == NULL) {
        tail = NULL;
    }
    request->started = true;
    running++;
    pthread_mutex_unlock(&queue_mutex);

    // Never hold the queue lock across AppKit, TIS, or user logger callbacks.
    request->proc(request->context);

    pthread_mutex_lock(&queue_mutex);
    request->completed = true;
    request->finished = true;
    running--;
    pthread_cond_broadcast(&queue_cond);
    pthread_mutex_unlock(&queue_mutex);
    return true;
}
