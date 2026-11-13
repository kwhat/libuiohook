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
#include "test_assert.h"

int main(void) {
    mouse_wheel_event_data wheel = {
        .type = WHEEL_UNIT_SCROLL, .amount = 3, .rotation = 360,
        .delta = 120, .direction = WHEEL_VERTICAL_DIRECTION
    };
    double remainder = 0;
    int32_t rotation;
    TEST_ASSERT(wheel_post_rotation(&wheel, 1, &remainder, &rotation) && rotation == 3);
    TEST_ASSERT(wheel_post_rotation(&wheel, 120, &remainder, &rotation) && rotation == 360);
    wheel.amount = 0;
    wheel.rotation = -300;
    wheel.delta = 100;
    TEST_ASSERT(wheel_post_rotation(&wheel, 120, &remainder, &rotation) && rotation == -360);
    wheel.rotation = 25;
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT(wheel_post_rotation(&wheel, 1, &remainder, &rotation));
        TEST_ASSERT(rotation == (i == 3 ? 1 : 0));
    }
    wheel.rotation = -25;
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT(wheel_post_rotation(&wheel, 1, &remainder, &rotation));
        TEST_ASSERT(rotation == (i == 3 ? -1 : 0));
    }
    wheel.rotation = 1;
    wheel.delta = 10;
    int32_t total = 0;
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT(wheel_post_rotation(&wheel, 1, &remainder, &rotation));
        total += rotation;
    }
    TEST_ASSERT(total == 1);
    wheel.delta = 0;
    TEST_ASSERT(!wheel_post_rotation(&wheel, 1, &remainder, &rotation));
    wheel.delta = 1;
    wheel.direction = 0;
    TEST_ASSERT(!wheel_post_rotation(&wheel, 1, &remainder, &rotation));
    wheel.direction = WHEEL_HORIZONTAL_DIRECTION;
    wheel.type = 0;
    TEST_ASSERT(!wheel_post_rotation(&wheel, 1, &remainder, &rotation));
    wheel.type = WHEEL_BLOCK_SCROLL;
    wheel.rotation = INT16_MIN;
    TEST_ASSERT(wheel_post_rotation(&wheel, 120, &remainder, &rotation));
    TEST_ASSERT(rotation == -3932160);
    TEST_ASSERT(!wheel_post_rotation(&wheel, 0, &remainder, &rotation));

    TEST_ASSERT(wheel_capture_rotation(0.125, 120, &remainder) == 15);
    TEST_ASSERT(wheel_capture_rotation(-1.5, 120, &remainder) == -180);
    TEST_ASSERT(wheel_capture_rotation(0.5, 1, &remainder) == 0);
    TEST_ASSERT(wheel_capture_rotation(0.5, 1, &remainder) == 1);
    TEST_ASSERT(wheel_capture_rotation(1000, 120, &remainder) == INT16_MAX);
    TEST_ASSERT(wheel_capture_rotation(-1000, 120, &remainder) == INT16_MIN);
    TEST_ASSERT(remainder == 0);
    return EXIT_SUCCESS;
}
