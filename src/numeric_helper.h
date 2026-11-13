/* libUIOHook: Cross-platform keyboard and mouse hooking from userland.
 * Copyright (C) 2006-2023 Alexander Barker.  All Rights Reserved.
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

#ifndef _included_numeric_helper
#define _included_numeric_helper

#include <stdint.h>

// Saturating integer conversions. Negative unsigned inputs become zero.
int8_t clamp_int8(int64_t value);
uint8_t clamp_uint8(int64_t value);
int16_t clamp_int16(int64_t value);
uint16_t clamp_uint16(int64_t value);
int32_t clamp_int32(int64_t value);
uint32_t clamp_uint32(int64_t value);

// Floating-point conversions truncate toward zero; NaN becomes zero.
int16_t clamp_int16_from_double(double value);
uint16_t clamp_uint16_from_double(double value);

#endif
