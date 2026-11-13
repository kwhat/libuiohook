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

#include <pthread.h>
#include <string.h>
#include "darwin/runloop_queue.h"
#include "test_assert.h"

static pthread_mutex_t test_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t test_cond = PTHREAD_COND_INITIALIZER;
static unsigned int wakes;
static bool entered, release_callback;

struct job {
    uint64_t generation;
    unsigned int timeout;
    int value, calls;
    bool result, returned, block;
};

static void wake_scheduler(void *context) {
    TEST_ASSERT(context == &wakes);
    pthread_mutex_lock(&test_mutex);
    wakes++;
    pthread_cond_broadcast(&test_cond);
    pthread_mutex_unlock(&test_mutex);
}

static void wait_for_wakes(unsigned int count) {
    pthread_mutex_lock(&test_mutex);
    while (wakes < count) {
        pthread_cond_wait(&test_cond, &test_mutex);
    }
    pthread_mutex_unlock(&test_mutex);
}

static void convert(void *context) {
    struct job *job = context;
    job->calls++;
    job->value *= 2;
    if (job->block) {
        pthread_mutex_lock(&test_mutex);
        entered = true;
        pthread_cond_broadcast(&test_cond);
        while (!release_callback) {
            pthread_cond_wait(&test_cond, &test_mutex);
        }
        pthread_mutex_unlock(&test_mutex);
    }
}

static void *caller(void *context) {
    struct job *job = context;
    bool result = runloop_queue_call(job->generation, convert, job, false, job->timeout);
    pthread_mutex_lock(&test_mutex);
    job->result = result;
    job->returned = true;
    pthread_cond_broadcast(&test_cond);
    pthread_mutex_unlock(&test_mutex);
    return NULL;
}

static void *scheduler(void *context) {
    TEST_ASSERT(runloop_queue_perform(*(uint64_t *) context));
    return NULL;
}

static uint64_t start_queue(void) {
    wakes = 0;
    uint64_t generation = runloop_queue_start(wake_scheduler, &wakes);
    TEST_ASSERT(generation != 0);
    return generation;
}

static void test_completion(void) {
    uint64_t generation = start_queue();
    TEST_ASSERT(runloop_queue_start(wake_scheduler, &wakes) == 0);
    struct job direct = { .value = 7 };
    TEST_ASSERT(runloop_queue_call(generation, convert, &direct, true, 0));
    TEST_ASSERT(direct.value == 14 && direct.calls == 1 && wakes == 0);

    struct job jobs[4];
    pthread_t threads[4];
    for (int i = 0; i < 4; i++) {
        jobs[i] = (struct job) { .generation = generation, .timeout = 5000, .value = i + 1 };
        TEST_ASSERT(pthread_create(&threads[i], NULL, caller, &jobs[i]) == 0);
        wait_for_wakes(i + 1);
    }
    // Each completion broadcasts to all waiters. Only that request may finish.
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT(runloop_queue_perform(generation));
        TEST_ASSERT(pthread_join(threads[i], NULL) == 0);
        TEST_ASSERT(jobs[i].result && jobs[i].calls == 1 && jobs[i].value == (i + 1) * 2);
        pthread_mutex_lock(&test_mutex);
        for (int j = i + 1; j < 4; j++) {
            TEST_ASSERT(!jobs[j].returned);
        }
        pthread_mutex_unlock(&test_mutex);
    }
    TEST_ASSERT(!runloop_queue_perform(generation));
    runloop_queue_stop(generation);
}

static void test_cancellation_and_restart(void) {
    uint64_t generation = start_queue();
    struct job jobs[3];
    pthread_t threads[3];
    for (int i = 0; i < 3; i++) {
        jobs[i] = (struct job) { .generation = generation, .timeout = 5000, .value = 10 + i };
        TEST_ASSERT(pthread_create(&threads[i], NULL, caller, &jobs[i]) == 0);
    }
    wait_for_wakes(3);
    // The scheduler never runs: shutdown must still release every caller.
    runloop_queue_stop(generation);
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT(pthread_join(threads[i], NULL) == 0);
        TEST_ASSERT(!jobs[i].result && jobs[i].calls == 0 && jobs[i].value == 10 + i);
    }
    TEST_ASSERT(!runloop_queue_call(generation, convert, &jobs[0], true, 0));
    uint64_t next = start_queue();
    TEST_ASSERT(next != generation);
    runloop_queue_stop(generation); // A stale observer/source cannot stop a new session.
    TEST_ASSERT(!runloop_queue_perform(generation));
    TEST_ASSERT(!runloop_queue_call(generation, convert, &jobs[0], false, 0));
    TEST_ASSERT(runloop_queue_call(next, convert, &jobs[0], true, 0));
    runloop_queue_stop(next);
}

static void test_running_request_lifetime(void) {
    uint64_t generation = start_queue();
    entered = release_callback = false;
    struct job running = { .generation = generation, .timeout = 5000, .value = 4, .block = true };
    struct job pending = { .generation = generation, .timeout = 5000, .value = 5 };
    pthread_t first, second, executor;
    TEST_ASSERT(pthread_create(&first, NULL, caller, &running) == 0);
    wait_for_wakes(1);
    TEST_ASSERT(pthread_create(&executor, NULL, scheduler, &generation) == 0);
    pthread_mutex_lock(&test_mutex);
    while (!entered) {
        pthread_cond_wait(&test_cond, &test_mutex);
    }
    pthread_mutex_unlock(&test_mutex);
    TEST_ASSERT(pthread_create(&second, NULL, caller, &pending) == 0);
    wait_for_wakes(2);
    runloop_queue_stop(generation);
    TEST_ASSERT(pthread_join(second, NULL) == 0 && !pending.result && pending.calls == 0);
    TEST_ASSERT(runloop_queue_start(wake_scheduler, &wakes) == 0);
    pthread_mutex_lock(&test_mutex);
    TEST_ASSERT(!running.returned);
    release_callback = true;
    pthread_cond_broadcast(&test_cond);
    pthread_mutex_unlock(&test_mutex);
    TEST_ASSERT(pthread_join(executor, NULL) == 0);
    TEST_ASSERT(pthread_join(first, NULL) == 0);
    TEST_ASSERT(running.result && running.calls == 1 && running.value == 8);
}

static void test_timeout(void) {
    uint64_t generation = start_queue();
    // Withdraw a middle request without losing either neighbor.
    struct job jobs[3] = {
        { .generation = generation, .timeout = 5000 },
        { .generation = generation, .timeout = 20 },
        { .generation = generation, .timeout = 5000 }
    };
    pthread_t threads[3];
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT(pthread_create(&threads[i], NULL, caller, &jobs[i]) == 0);
        wait_for_wakes(i + 1);
    }
    TEST_ASSERT(pthread_join(threads[1], NULL) == 0);
    TEST_ASSERT(!jobs[1].result && jobs[1].calls == 0);
    TEST_ASSERT(runloop_queue_perform(generation));
    TEST_ASSERT(runloop_queue_perform(generation));
    TEST_ASSERT(!runloop_queue_perform(generation));
    TEST_ASSERT(pthread_join(threads[0], NULL) == 0 && jobs[0].result);
    TEST_ASSERT(pthread_join(threads[2], NULL) == 0 && jobs[2].result);
    // Timeout also unlinks the last/only request, whose storage is then gone.
    jobs[1].returned = false;
    TEST_ASSERT(pthread_create(&threads[1], NULL, caller, &jobs[1]) == 0);
    TEST_ASSERT(pthread_join(threads[1], NULL) == 0 && !jobs[1].result);
    TEST_ASSERT(!runloop_queue_perform(generation));
    runloop_queue_stop(generation);
}

int main(void) {
    TEST_ASSERT(runloop_queue_start(NULL, NULL) == 0);
    test_completion();
    test_cancellation_and_restart();
    test_running_request_lifetime();
    test_timeout();
    return EXIT_SUCCESS;
}
