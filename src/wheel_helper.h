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

#ifndef _included_wheel_helper
#define _included_wheel_helper

#include <stdbool.h>
#include <stdint.h>
#include <uiohook.h>

// Capture keeps sub-unit movement until it can fit the public rotation field.
extern int16_t wheel_capture_rotation(double units, uint16_t delta, double *remainder);

// Convert rotation/delta to native units. Amount describes settings, not motion.
// Callers commit the updated remainder only after successful native posting.
extern bool wheel_post_rotation(const mouse_wheel_event_data *wheel,
        uint16_t native_delta, double *remainder, int32_t *rotation);

#endif
