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

#include <stdint.h>

#include "input_helper.h"
#include "keycode_helper.h"
#include "mouse_helper.h"

#ifdef USE_EPOCH_TIME
/* Get the current timestamp in unix epoch time. */
uint64_t get_unix_timestamp(uint64_t seconds, uint64_t microseconds) {
    // Convert the event time to a Unix epoch in MS.
    uint64_t timestamp = (seconds * 1000) + (microseconds / 1000);

    return timestamp;
}
#else
static uint64_t seq_timestamp = 0;
uint64_t get_seq_timestamp() {
    if (seq_timestamp == UINT64_MAX) {
        // TODO Warning
        seq_timestamp = 0;
    }
    return seq_timestamp++;
}
#endif

void load_input_helper(void) {
    load_keycode_helper();
    init_pointer_tracking();
}

void unload_input_helper(void) {
    unload_keycode_helper();
}
