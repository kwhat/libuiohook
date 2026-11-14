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

#ifndef _included_input_helper
#define _included_input_helper

#include <stdint.h>

/* Event timestamp source: real Unix epoch time, or a monotonic sequence counter. */
#ifdef USE_EPOCH_TIME

/* Convert event seconds and microseconds to Unix epoch milliseconds. */
extern uint64_t get_unix_timestamp(uint64_t seconds, uint64_t microseconds);
#else

/* Return the next sequence timestamp, wrapping to zero when the counter reaches UINT64_MAX. */
extern uint64_t get_seq_timestamp();
#endif

/* Initialize keyboard translation, then compute pointer bounds and seed the tracked pointer
 * position. */
extern void load_input_helper(void);

/* Release the keyboard translation resources owned by the keycode helper. */
extern void unload_input_helper(void);

#endif
