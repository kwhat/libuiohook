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

#include <errno.h>
#include <limits.h>
#include <linux/input.h>
#include <poll.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "input_activity.h"

#define BIT_WORDS(count) (((count) + sizeof(unsigned long) * 8 - 1) / (sizeof(unsigned long) * 8))

static bool bit_is_set(const unsigned long *bits, unsigned int bit) {
    const unsigned int width = sizeof(*bits) * 8;

    return (bits[bit / width] & (1UL << (bit % width))) != 0;
}

int input_activity_init(struct input_activity *activity, int fd) {
    *activity = (struct input_activity) { .fd = fd };
    unsigned long types[BIT_WORDS(EV_CNT)] = { 0 };
    if (ioctl(fd, EVIOCGBIT(0, sizeof(types)), types) < 0) {
        return -errno;
    }

    activity->has_keys = bit_is_set(types, EV_KEY);

    if (bit_is_set(types, EV_ABS)) {
        unsigned long axes[BIT_WORDS(ABS_CNT)] = { 0 };
        if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(axes)), axes) < 0) {
            return -errno;
        }

        if (bit_is_set(axes, ABS_MT_SLOT)) {
            struct input_absinfo slots;
            if (ioctl(fd, EVIOCGABS(ABS_MT_SLOT), &slots) < 0) {
                return -errno;
            }

            if (slots.maximum < 0 || slots.maximum == INT_MAX) {
                return -EINVAL;
            }

            activity->slot_count = slots.maximum + 1;
        }
    }

    return 0;
}

int input_activity_is_idle(const struct input_activity *activity) {
    if (activity->has_keys) {
        unsigned long keys[BIT_WORDS(KEY_CNT)] = { 0 };
        if (ioctl(activity->fd, EVIOCGKEY(sizeof(keys)), keys) < 0) {
            return -errno;
        }

        for (unsigned int code = 0; code <= KEY_MAX; code++) {
            if (bit_is_set(keys, code)) {
                return 0;
            }
        }
    }

    if (activity->slot_count > 0) {
        size_t count = (size_t) activity->slot_count + 1;
        int32_t *values = calloc(count, sizeof(*values));
        if (values == NULL) {
            return -ENOMEM;
        }

        values[0] = ABS_MT_TRACKING_ID;
        int status = ioctl(activity->fd, EVIOCGMTSLOTS(count * sizeof(*values)), values);
        if (status < 0) {
            int error = errno;
            free(values);
            return -error;
        }

        bool active = false;
        for (size_t i = 1; i < count; i++) {
            active |= values[i] >= 0;
        }

        free(values);

        if (active) {
            return 0;
        }
    }

    return 1;
}

int input_activity_wait(const struct input_activity *activity) {
    for (;;) {
        // Drain pre-grab events before testing state so they cannot be replayed
        // through the clone after the desktop has already received them.
        struct input_event events[32];
        ssize_t size;
        do {
            size = read(activity->fd, events, sizeof(events));
        } while (size > 0 || (size < 0 && errno == EINTR));

        if (size == 0) {
            return -ENODEV;
        }

        if (errno != EAGAIN) {
            return -errno;
        }

        int idle = input_activity_is_idle(activity);
        if (idle != 0) {
            return idle < 0 ? idle : 0;
        }

        struct pollfd event = { .fd = activity->fd, .events = POLLIN };
        int status = poll(&event, 1, -1);
        if (status < 0 && errno != EINTR) {
            return -errno;
        }

        if (status > 0 && (event.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            return -ENODEV;
        }
    }
}
