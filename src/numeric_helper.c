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

#include "numeric_helper.h"

int8_t clamp_int8(int64_t value) {
    if (value < INT8_MIN) {
        return INT8_MIN;
    }
    if (value > INT8_MAX) {
        return INT8_MAX;
    }
    return (int8_t) value;
}

uint8_t clamp_uint8(int64_t value) {
    if (value < 0) {
        return 0;
    }
    if (value > UINT8_MAX) {
        return UINT8_MAX;
    }
    return (uint8_t) value;
}

int16_t clamp_int16(int64_t value) {
    if (value < INT16_MIN) {
        return INT16_MIN;
    }
    if (value > INT16_MAX) {
        return INT16_MAX;
    }
    return (int16_t) value;
}

uint16_t clamp_uint16(int64_t value) {
    if (value < 0) {
        return 0;
    }
    if (value > UINT16_MAX) {
        return UINT16_MAX;
    }
    return (uint16_t) value;
}

int32_t clamp_int32(int64_t value) {
    if (value < INT32_MIN) {
        return INT32_MIN;
    }
    if (value > INT32_MAX) {
        return INT32_MAX;
    }
    return (int32_t) value;
}

uint32_t clamp_uint32(int64_t value) {
    if (value < 0) {
        return 0;
    }
    if (value > UINT32_MAX) {
        return UINT32_MAX;
    }
    return (uint32_t) value;
}

int16_t clamp_int16_from_double(double value) {
    if (value != value) {
        return 0;
    }
    if (value < INT16_MIN) {
        return INT16_MIN;
    }
    if (value > INT16_MAX) {
        return INT16_MAX;
    }
    return (int16_t) value;
}

uint16_t clamp_uint16_from_double(double value) {
    if (value != value) {
        return 0;
    }
    if (value < 0) {
        return 0;
    }
    if (value > UINT16_MAX) {
        return UINT16_MAX;
    }
    return (uint16_t) value;
}

