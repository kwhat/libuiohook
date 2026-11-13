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

#ifndef _included_runloop_helper
#define _included_runloop_helper

#include "runloop_queue.h"

/* Register a main-runloop source in common modes; return UIOHOOK_SUCCESS or an error code. */
extern int main_runloop_start(void);

/* Stop accepting requests and cancel pending work without removing the runloop source. */
extern void main_runloop_stop(void);

/* Cancel pending work and release the runloop source; repeated cleanup is safe. */
extern void main_runloop_destroy(void);

/* Run proc on the main thread; return false if unavailable, cancelled, or not started within 250 ms. */
extern bool main_runloop_call(runloop_proc proc, void *context);

#endif
