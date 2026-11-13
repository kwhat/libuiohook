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

#include <limits.h>
#include "wheel_helper.h"

// Floating division may leave a whole step just below an integer (e.g. ten
// tenths). The tolerance is far smaller than a public delta's smallest fraction.
static int32_t whole_units(double value) {
    int32_t units = (int32_t) value;
    if (value > 0 && units + 1.0 - value < 1e-9) {
        units++;
    } else if (value < 0 && value - (units - 1.0) < 1e-9) {
        units--;
    }
    return units;
}

static double fractional_units(double value, int32_t units) {
    double remainder = value - units;
    return remainder > -1e-9 && remainder < 1e-9 ? 0 : remainder;
}

int16_t wheel_capture_rotation(double units, uint16_t delta, double *remainder) {
    double value = units * delta + *remainder;
    if (value != value) {
        *remainder = 0;
        return 0;
    }
    if (value > INT16_MAX) {
        *remainder = 0;
        return INT16_MAX;
    }
    if (value < INT16_MIN) {
        *remainder = 0;
        return INT16_MIN;
    }

    int16_t rotation = (int16_t) whole_units(value);
    *remainder = fractional_units(value, rotation);
    return rotation;
}

bool wheel_post_rotation(const mouse_wheel_event_data *wheel,
        uint16_t native_delta, double *remainder, int32_t *rotation) {
    if (wheel->delta == 0 || native_delta == 0
            || (wheel->type != WHEEL_UNIT_SCROLL && wheel->type != WHEEL_BLOCK_SCROLL)
            || (wheel->direction != WHEEL_VERTICAL_DIRECTION
                && wheel->direction != WHEEL_HORIZONTAL_DIRECTION)) {
        return false;
    }

    // Public 16-bit fields bound this product below INT32_MAX.
    double value = (double) wheel->rotation * native_delta / wheel->delta + *remainder;
    *rotation = whole_units(value);
    *remainder = fractional_units(value, *rotation);
    return true;
}
