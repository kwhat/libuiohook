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

#ifndef _included_runloop_queue
#define _included_runloop_queue

#include <stdbool.h>
#include <stdint.h>

typedef void (*runloop_proc)(void *);

/* Start the singleton queue; return its generation or zero on failure. Wake must not block or reenter. */
extern uint64_t runloop_queue_start(runloop_proc wake, void *context);

/* Cancel pending work for this generation; running callbacks finish before their callers return. */
extern void runloop_queue_stop(uint64_t generation);

/* Run directly on the main thread or queue and wait; return false for invalid, cancelled, or timed-out work.
 * Timeout applies only before execution starts; context must remain valid until this call returns. */
extern bool runloop_queue_call(uint64_t generation, runloop_proc proc, void *context, unsigned int timeout_ms);

/* Execute one queued request for this generation on the main-thread scheduler; return whether one ran. */
extern bool runloop_queue_perform(uint64_t generation);

#endif
