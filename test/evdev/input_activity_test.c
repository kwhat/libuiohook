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
#include <linux/input.h>
#include <poll.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "input_activity.h"
#include "test_assert.h"

static bool has_keys, has_slots;
static unsigned int key1, key2;
static bool contact;
static int query_error, poll_error, read_error, polls;
static bool disconnected;

static void set_bit(unsigned long *bits, unsigned int bit) {
    unsigned int width = sizeof(*bits) * 8;
    bits[bit / width] |= 1UL << (bit % width);
}

int ioctl(int fd, unsigned long request, ...) {
    TEST_ASSERT(fd == 42);
    va_list args;
    va_start(args, request);
    void *value = va_arg(args, void *);
    va_end(args);
    if (query_error) {
        errno = query_error;
        return -1;
    }
    if (_IOC_NR(request) == _IOC_NR(EVIOCGBIT(0, 0))) {
        if (has_keys) set_bit(value, EV_KEY);
        if (has_slots) set_bit(value, EV_ABS);
    } else if (_IOC_NR(request) == _IOC_NR(EVIOCGBIT(EV_ABS, 0))) {
        set_bit(value, ABS_MT_SLOT);
    } else if (request == EVIOCGABS(ABS_MT_SLOT)) {
        ((struct input_absinfo *) value)->maximum = 1;
    } else if (_IOC_NR(request) == _IOC_NR(EVIOCGKEY(0))) {
        if (key1) set_bit(value, key1);
        if (key2) set_bit(value, key2);
    } else if (_IOC_NR(request) == _IOC_NR(EVIOCGMTSLOTS(0))) {
        int32_t *slots = value;
        TEST_ASSERT(slots[0] == ABS_MT_TRACKING_ID);
        slots[1] = -1;
        slots[2] = contact ? 123 : -1;
    } else {
        TEST_ASSERT(false);
    }
    return 0;
}

ssize_t read(int fd, void *buffer, size_t size) {
    (void) buffer;
    TEST_ASSERT(fd == 42 && size >= sizeof(struct input_event));
    if (disconnected) return 0;
    errno = read_error ? read_error : EAGAIN;
    return -1;
}

int poll(struct pollfd *fds, nfds_t count, int timeout) {
    TEST_ASSERT(count == 1 && fds[0].fd == 42 && timeout == -1);
    if (poll_error) {
        errno = poll_error;
        return -1;
    }
    polls++;
    TEST_ASSERT(polls <= 3);
    // Releasing an earlier key must not hide another held key or contact.
    if (polls == 1) key1 = 0;
    if (polls == 2) key2 = 0;
    if (polls == 3) contact = false;
    fds[0].revents = POLLIN;
    return 1;
}

int main(void) {
    struct input_activity activity;
    has_keys = true;
    TEST_ASSERT(input_activity_init(&activity, 42) == 0);
    TEST_ASSERT(input_activity_is_idle(&activity) == 1);
    const unsigned int keys[] = { KEY_A, KEY_LEFTCTRL, BTN_LEFT, BTN_RIGHT, BTN_TOUCH, KEY_MAX };
    for (unsigned int i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        key1 = keys[i];
        TEST_ASSERT(input_activity_is_idle(&activity) == 0);
        key1 = 0;
        TEST_ASSERT(input_activity_is_idle(&activity) == 1);
    }

    has_slots = true;
    TEST_ASSERT(input_activity_init(&activity, 42) == 0);
    contact = true;
    TEST_ASSERT(input_activity_is_idle(&activity) == 0);
    key1 = KEY_A;
    key2 = KEY_MAX;
    TEST_ASSERT(input_activity_wait(&activity) == 0);
    TEST_ASSERT(polls == 3 && input_activity_is_idle(&activity) == 1);

    // State-query errors never count as idle, before either grab or ungrab.
    query_error = EIO;
    TEST_ASSERT(input_activity_is_idle(&activity) == -EIO);
    TEST_ASSERT(input_activity_wait(&activity) == -EIO);
    TEST_ASSERT(input_activity_init(&activity, 42) == -EIO);
    query_error = 0;
    TEST_ASSERT(input_activity_init(&activity, 42) == 0);
    key1 = BTN_LEFT;
    poll_error = ENODEV;
    TEST_ASSERT(input_activity_wait(&activity) == -ENODEV);
    poll_error = 0;
    read_error = EIO;
    TEST_ASSERT(input_activity_wait(&activity) == -EIO);
    read_error = 0;
    disconnected = true;
    TEST_ASSERT(input_activity_wait(&activity) == -ENODEV);
    disconnected = false;

    // Slot-only devices must wait even without EV_KEY / BTN_TOUCH support.
    has_keys = false;
    TEST_ASSERT(input_activity_init(&activity, 42) == 0);
    contact = true;
    TEST_ASSERT(input_activity_is_idle(&activity) == 0);
    contact = false;
    TEST_ASSERT(input_activity_is_idle(&activity) == 1);
    puts("Generic input activity tests passed");
    return 0;
}
