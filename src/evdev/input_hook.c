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
#include <fcntl.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <libudev.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <uiohook.h>
#include <unistd.h>

#include "dispatch_event.h"
#include "input_helper.h"
#include "logger.h"


struct input_hook {
    struct libevdev *evdev;
    struct libevdev_uinput *uinput;
    char *devnode;

    // Device DPI from the MOUSE_DPI udev property (0 = unknown / no normalization).  The
    // uinput clone carries no MOUSE_DPI, so libinput cannot normalize its motion to the
    // 1000-DPI reference; we pre-scale reinjected REL_X/REL_Y by 1000/dpi to compensate.
    // carry_x/carry_y hold the sub-unit remainder between events so nothing is lost.
    int dpi;
    int64_t carry_x, carry_y;

    struct input_hook *next;
};

static struct input_hook *hook_list = NULL;

static int rel_x = 0, rel_y = 0;
static int wheel_h = 0, wheel_v = 0;

// Event file descriptor used by hook_stop() to wake hook_run() and exit its event loop.
static int stop_fd = -1;

// udev context and hotplug monitor, live for the duration of hook_run().
static struct udev *udev_context = NULL;
static struct udev_monitor *udev_mon = NULL;

// Singleton epoll markers, identified by address; everything else in the epoll set
// is a struct input_hook validated against hook_list before use.
static char stop_tag;
static char monitor_tag;


// Handle an EV_KEY event.  Mouse buttons route through the pointer dispatch; every
// other code is treated as a keyboard key.
static bool hook_key_proc(struct input_event *ev) {
    uint16_t button;
    uint16_t mask;

    switch (ev->code) {
        case BTN_LEFT:    button = MOUSE_BUTTON1; mask = MASK_BUTTON1; break;
        case BTN_RIGHT:   button = MOUSE_BUTTON2; mask = MASK_BUTTON2; break;
        case BTN_MIDDLE:  button = MOUSE_BUTTON3; mask = MASK_BUTTON3; break;

        case BTN_SIDE:
        case BTN_BACK:    button = MOUSE_BUTTON4; mask = MASK_BUTTON4; break;

        case BTN_EXTRA:
        case BTN_FORWARD: button = MOUSE_BUTTON5; mask = MASK_BUTTON5; break;

        default:
            // Not a pointer button; treat as a keyboard key.
            // value 1 (press) and 2 (autorepeat) both dispatch as a press.
            // TODO If we time the delta between 1 and 2 we can get the key repeat interval.
            if (ev->value > 0) {
                return dispatch_key_press(ev);
            }

            return dispatch_key_release(ev);
    }

    if (ev->value > 0) {
        set_modifier_mask(mask);
        return dispatch_mouse_press(ev, button);
    }

    unset_modifier_mask(mask);
    return dispatch_mouse_release(ev, button);
}

// Handle an EV_REL event: accumulate motion and wheel deltas until the next EV_SYN
// flushes them.  Nothing is dispatched here, so the event is never consumed.
static bool hook_rel_proc(struct input_event *ev) {
    switch (ev->code) {
        case REL_X:
            rel_x += ev->value;
            break;

        case REL_Y:
            rel_y += ev->value;
            break;

        case REL_WHEEL_HI_RES:
            wheel_v += ev->value;
            break;

        case REL_HWHEEL_HI_RES:
            wheel_h += ev->value;
            break;
    }

    return false;
}

static bool hook_sync_proc(struct input_event *ev) {
    bool consumed = false;

    if (rel_x != 0 || rel_y != 0) {
        track_pointer_delta(rel_x, rel_y);
        consumed |= dispatch_mouse_move(ev);
        rel_x = rel_y = 0;
    }

    if (wheel_v != 0) {
        consumed |= dispatch_mouse_wheel(ev, wheel_v, WHEEL_VERTICAL_DIRECTION);
        wheel_v = 0;
    }

    if (wheel_h != 0) {
        consumed |= dispatch_mouse_wheel(ev, wheel_h, WHEEL_HORIZONTAL_DIRECTION);
        wheel_h = 0;
    }

    return consumed;
}

static bool hook_event_proc(struct input_event *ev) {
    switch (ev->type) {
        case EV_KEY:
            return hook_key_proc(ev);

        case EV_REL:
            return hook_rel_proc(ev);

        case EV_SYN:
            return hook_sync_proc(ev);
    }

    return false;
}

static int create_hook(const char *path, int dpi, struct input_hook **hook) {
    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to open input device: %s! (%d)\n",
                __FUNCTION__, __LINE__,
                path, errno);
        return UIOHOOK_FAILURE;
    }

    // Zero initialized so destroy_hook() can safely be called after any failure below.
    *hook = calloc(1, sizeof(struct input_hook));
    if (*hook == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to allocate memory for evdev buffer!\n",
                __FUNCTION__, __LINE__);

        close(fd);
        return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }

    (*hook)->devnode = strdup(path);
    (*hook)->dpi = dpi;

    int err = libevdev_new_from_fd(fd, &(*hook)->evdev);
    if (err < 0) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to create evdev from file descriptor! (%d)\n",
                __FUNCTION__, __LINE__,
                err);

        // No evdev handle owns the descriptor yet, close it here.
        (*hook)->evdev = NULL;
        close(fd);
        return UIOHOOK_FAILURE;
    }

    // We need to wait until no key is being pressed because we are going 
    // to grab below and that will cause keys to stick.
    struct input_event ev;
    for (unsigned int i = 0; i < KEY_MAX; i++) {
        while (libevdev_get_event_value((*hook)->evdev, EV_KEY, i)) {
            int status = -EAGAIN;
            unsigned int flags = LIBEVDEV_READ_FLAG_NORMAL;
            do {
                status = libevdev_next_event((*hook)->evdev, flags, &ev);
                if (status == LIBEVDEV_READ_STATUS_SYNC) {
                    flags = LIBEVDEV_READ_FLAG_SYNC;
                }
            } while (status == LIBEVDEV_READ_STATUS_SYNC);
        }
    }

    err = libevdev_uinput_create_from_device((*hook)->evdev, LIBEVDEV_UINPUT_OPEN_MANAGED, &(*hook)->uinput);
    if (err < 0) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Failed to create uinput from device! (%d)\n",
                __FUNCTION__, __LINE__,
                err);

        (*hook)->uinput = NULL;
    } else {
        // On success, grab the device so we can consume events.
        err = libevdev_grab((*hook)->evdev, LIBEVDEV_GRAB);
        if (err != 0) {
            logger(LOG_LEVEL_WARN, "%s [%u]: Failed to grab evdev! (%d)\n",
                    __FUNCTION__, __LINE__,
                    err);

            libevdev_uinput_destroy((*hook)->uinput);
            (*hook)->uinput = NULL;
        }
    }

    return UIOHOOK_SUCCESS;
}

static void destroy_hook(struct input_hook **hook) {
    if (*hook != NULL) {
        int fd = -1;

        if ((*hook)->evdev != NULL) {
            fd = libevdev_get_fd((*hook)->evdev);

            libevdev_grab((*hook)->evdev, LIBEVDEV_UNGRAB);
            libevdev_free((*hook)->evdev);
            (*hook)->evdev = NULL;
        }

        if ((*hook)->uinput != NULL) {
            libevdev_uinput_destroy((*hook)->uinput);
            (*hook)->uinput = NULL;
        }

        if ((*hook)->devnode != NULL) {
            free((*hook)->devnode);
            (*hook)->devnode = NULL;
        }

        free(*hook);
        *hook = NULL;

        if (fd >= 0) {
            close(fd);
        }
    }
}

static const char * get_device_label(struct udev_device *device) {
    if (udev_device_get_property_value(device, "ID_INPUT_KEYBOARD") != NULL) {
        return "keyboard";
    }

    if (udev_device_get_property_value(device, "ID_INPUT_MOUSE") != NULL) {
        return "mouse";
    }

    if (udev_device_get_property_value(device, "ID_INPUT_POINTINGSTICK") != NULL) {
        return "pointing stick";
    }

    // TODO ID_INPUT_TOUCHPAD devices use EV_ABS + BTN_TOUCH which will require touch-to-pointer
    // processing in hook_event_proc before they can be enumerated here.

    return NULL;
}

/* Returns true if the device node is already hooked or is a uinput clone we created.
 * Our clones advertise the same capabilities as the device they mirror, so udev stamps
 * them with the same ID_INPUT_* properties. Without this check the udev monitor will
 * hook each clone and clone it again, leading to an infinite feedback loop. */
static bool is_device_hooked(const char *devnode) {
    for (struct input_hook *hook = hook_list; hook != NULL; hook = hook->next) {
        if (hook->devnode != NULL && strcmp(hook->devnode, devnode) == 0) {
            return true;
        }

        if (hook->uinput != NULL) {
            const char *clone_devnode = libevdev_uinput_get_devnode(hook->uinput);
            if (clone_devnode != NULL && strcmp(clone_devnode, devnode) == 0) {
                return true;
            }
        }
    }

    return false;
}

// Parse the active DPI from a device's MOUSE_DPI udev property, e.g.
// "1200@1000 *2400@1000 3200@1000" -> 2400 (the entry marked active with '*', else the
// first entry).  Returns 0 when absent/unparseable, in which case reinjected motion is
// passed through unscaled.
static int get_device_dpi(struct udev_device *device) {
    const char *prop = udev_device_get_property_value(device, "MOUSE_DPI");
    if (prop == NULL) {
        return 0;
    }

    int first_dpi = 0;
    for (const char *p = prop; *p != '\0'; ) {
        while (*p == ' ') { p++; }
        if (*p == '\0') { break; }

        bool active = *p == '*';
        if (active) { p++; }

        int dpi = atoi(p);      // reads the leading digits, stops at '@'
        if (first_dpi == 0) { first_dpi = dpi; }
        if (active && dpi > 0) { return dpi; }

        while (*p != ' ' && *p != '\0') { p++; }
    }

    return first_dpi;
}

// Hook a device node, register it with epoll and link it into hook_list.
static int attach_device(int epoll_fd, const char *devnode, int dpi, const char *label) {
    struct input_hook *hook = NULL;
    if (create_hook(devnode, dpi, &hook) != UIOHOOK_SUCCESS) {
        destroy_hook(&hook);
        return UIOHOOK_FAILURE;
    }

    // The kernel copies the epoll_event at registration, it does not need to outlive this call.
    struct epoll_event event = {
        .events = EPOLLIN,
        .data.ptr = hook
    };

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, libevdev_get_fd(hook->evdev), &event) < 0) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to add file descriptor to epoll! (%d)\n",
                __FUNCTION__, __LINE__, errno);

        destroy_hook(&hook);
        return UIOHOOK_FAILURE;
    }

    hook->next = hook_list;
    hook_list = hook;

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Attached %s device: %s.\n",
            __FUNCTION__, __LINE__,
            label, devnode);

    return UIOHOOK_SUCCESS;
}

// Unregister a device from epoll, unlink it from hook_list and destroy it.
static void detach_device(int epoll_fd, struct input_hook *hook) {
    struct input_hook **indirect = &hook_list;
    while (*indirect != NULL && *indirect != hook) {
        indirect = &(*indirect)->next;
    }

    if (*indirect != NULL) {
        *indirect = hook->next;
    }

    if (hook->evdev != NULL) {
        int fd = libevdev_get_fd(hook->evdev);
        if (fd >= 0) {
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
        }
    }

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Detached device: %s.\n",
            __FUNCTION__, __LINE__,
            hook->devnode != NULL ? hook->devnode : "(unknown)");

    destroy_hook(&hook);
}

static int create_event_listeners(int epoll_fd) {
    struct udev_enumerate *enumerate = udev_enumerate_new(udev_context);
    if (enumerate == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to create udev enumeration context!\n",
                __FUNCTION__, __LINE__);
        return UIOHOOK_FAILURE;
    }

    udev_enumerate_add_match_subsystem(enumerate, "input");
    // Only evdev nodes; excludes legacy mousedev/joydev nodes like /dev/input/mouse0.
    udev_enumerate_add_match_sysname(enumerate, "event*");
    udev_enumerate_scan_devices(enumerate);

    struct udev_list_entry *entry;
    udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(enumerate)) {
        struct udev_device *device = udev_device_new_from_syspath(udev_context, udev_list_entry_get_name(entry));
        if (device == NULL) {
            continue;
        }

        // Skip parent devices without a /dev/input/event* node, unsupported device
        // types and uinput clones we created ourselves.
        const char *devnode = udev_device_get_devnode(device);
        const char *label = get_device_label(device);
        if (devnode != NULL && label != NULL && !is_device_hooked(devnode)) {
            attach_device(epoll_fd, devnode, get_device_dpi(device), label);
        }

        udev_device_unref(device);
    }

    udev_enumerate_unref(enumerate);

    return UIOHOOK_SUCCESS;
}

static void destroy_event_listeners(int epoll_fd) {
    while (hook_list != NULL) {
        detach_device(epoll_fd, hook_list);
    }
}

// Process pending hotplug events from the udev monitor.
static void hook_monitor_proc(int epoll_fd) {
    struct udev_device *device;
    while ((device = udev_monitor_receive_device(udev_mon)) != NULL) {
        const char *action = udev_device_get_action(device);
        const char *devnode = udev_device_get_devnode(device);
        const char *sysname = udev_device_get_sysname(device);

        // Filter out parent devices and legacy character nodes leaving, only the evdev nodes matching 
        // the filter applied by create_event_listeners().
        if (action == NULL || devnode == NULL || sysname == NULL || strncmp(sysname, "event", 5) != 0) {
            udev_device_unref(device);
            continue;
        }

        if (strcmp(action, "add") == 0) {
            // Never attach our own uinput clones; doing so would grab them and clone
            // them again, cascading into an infinite chain of grabbed virtual devices.
            const char *label = get_device_label(device);
            if (label != NULL && !is_device_hooked(devnode)) {
                attach_device(epoll_fd, devnode, get_device_dpi(device), label);
            }
        } else if (strcmp(action, "remove") == 0) {
            for (struct input_hook *hook = hook_list; hook != NULL; hook = hook->next) {
                if (hook->devnode != NULL && strcmp(hook->devnode, devnode) == 0) {
                    detach_device(epoll_fd, hook);
                    break;
                }
            }
        }

        udev_device_unref(device);
    }
}

// Drain all pending events from a device epoll flagged readable, re-emitting each
// through the uinput clone unless it was consumed.  Detaches the device on read failure.
static void hook_device_proc(int epoll_fd, void *ptr) {
    // Validate the hook is still attached; a hotplug remove processed earlier in this
    // same batch may have already detached and freed it.
    struct input_hook *hook = hook_list;
    while (hook != NULL && hook != ptr) {
        hook = hook->next;
    }
    if (hook == NULL) {
        return;
    }

    struct input_event ev;
    int read_status;
    unsigned int flags = LIBEVDEV_READ_FLAG_NORMAL;
    do {
        read_status = libevdev_next_event(hook->evdev, flags, &ev);
        if (read_status == LIBEVDEV_READ_STATUS_SYNC) {
            flags = LIBEVDEV_READ_FLAG_SYNC;
        }

        if (read_status == LIBEVDEV_READ_STATUS_SUCCESS || read_status == LIBEVDEV_READ_STATUS_SYNC) {
            bool consumed = hook_event_proc(&ev);

            if (hook->uinput != NULL && !consumed) {
                int value = ev.value;
                bool emit = true;

                // Normalize relative motion to libinput's 1000-DPI reference.  The clone
                // has no MOUSE_DPI, so libinput would pass its raw counts through unscaled
                // and a high-DPI mouse would feel over-accelerated only while hooked.
                if (hook->dpi > 0 && hook->dpi != 1000
                        && ev.type == EV_REL && (ev.code == REL_X || ev.code == REL_Y)) {
                    int64_t *carry = ev.code == REL_X ? &hook->carry_x : &hook->carry_y;
                    int64_t numerator = (int64_t) ev.value * 1000 + *carry;

                    value = (int) (numerator / hook->dpi);
                    *carry = numerator - (int64_t) value * hook->dpi;
                    emit = value != 0;
                }

                if (emit) {
                    libevdev_uinput_write_event(hook->uinput, ev.type, ev.code, value);
                }
            }
        }
    } while (read_status == LIBEVDEV_READ_STATUS_SUCCESS || read_status == LIBEVDEV_READ_STATUS_SYNC);

    // The device is gone or otherwise unreadable; drop it.  Unplugged devices are
    // usually caught here before the udev remove event arrives.
    if (read_status != -EAGAIN) {
        logger(LOG_LEVEL_INFO, "%s [%u]: Device read failed, detaching: %s! (%d)\n",
                __FUNCTION__, __LINE__,
                hook->devnode != NULL ? hook->devnode : "(unknown)", read_status);

        detach_device(epoll_fd, hook);
    }
}

static void hook_teardown(int epoll_fd) {
    destroy_event_listeners(epoll_fd);

    if (udev_mon != NULL) {
        udev_monitor_unref(udev_mon);
        udev_mon = NULL;
    }

    if (udev_context != NULL) {
        udev_unref(udev_context);
        udev_context = NULL;
    }

    if (stop_fd >= 0) {
        close(stop_fd);
        stop_fd = -1;
    }

    if (epoll_fd >= 0 && close(epoll_fd)) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to close epoll file descriptor! (%d)\n",
                __FUNCTION__, __LINE__, errno);
    }
}

UIOHOOK_API int hook_run() {
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to call epoll_create1!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_ERROR_EPOLL_CREATE;
    }

    // Create the event file descriptor hook_stop() uses to signal shutdown.
    stop_fd = eventfd(0, EFD_NONBLOCK);
    if (stop_fd == -1) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to call eventfd! (%d)\n",
                __FUNCTION__, __LINE__, errno);

        hook_teardown(epoll_fd);
        return UIOHOOK_FAILURE;
    }

    struct epoll_event stop_event = {
        .events = EPOLLIN,
        .data.ptr = &stop_tag
    };
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, stop_fd, &stop_event) < 0) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to add stop file descriptor to epoll! (%d)\n",
                __FUNCTION__, __LINE__, errno);

        hook_teardown(epoll_fd);
        return UIOHOOK_FAILURE;
    }

    udev_context = udev_new();
    if (udev_context == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to create udev context!\n",
                __FUNCTION__, __LINE__);

        hook_teardown(epoll_fd);
        return UIOHOOK_FAILURE;
    }

    // Start the hotplug monitor before enumeration so devices plugged in while we
    // enumerate are not missed.
    udev_mon = udev_monitor_new_from_netlink(udev_context, "udev");
    if (udev_mon != NULL) {
        udev_monitor_filter_add_match_subsystem_devtype(udev_mon, "input", NULL);
        udev_monitor_enable_receiving(udev_mon);

        struct epoll_event monitor_event = {
            .events = EPOLLIN,
            .data.ptr = &monitor_tag
        };
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, udev_monitor_get_fd(udev_mon), &monitor_event) < 0) {
            logger(LOG_LEVEL_WARN, "%s [%u]: Failed to add udev monitor to epoll, hotplug disabled! (%d)\n",
                    __FUNCTION__, __LINE__, errno);

            udev_monitor_unref(udev_mon);
            udev_mon = NULL;
        }
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: Failed to create udev monitor, hotplug disabled!\n",
                __FUNCTION__, __LINE__);
    }

    int status = create_event_listeners(epoll_fd);
    if (status != UIOHOOK_SUCCESS) {
        hook_teardown(epoll_fd);
        return status;
    }

    // The device set changes at runtime with hotplug, so use a fixed batch size.
    // epoll_wait() delivers anything beyond it on the next iteration; nothing is lost.
    #define EVENT_BUFFER_SIZE 16
    #define EVENT_BUFFER_WAIT 30

    struct epoll_event event_buffer[EVENT_BUFFER_SIZE];

    // Initialize the input helper and fire the hook enabled event.
    dispatch_hook_enabled();

    bool is_running = true;
    while (is_running) {
        int event_count = epoll_wait(epoll_fd, event_buffer, EVENT_BUFFER_SIZE, EVENT_BUFFER_WAIT * 1000);
        if (event_count < 0) {
            if (errno == EINTR) {
                continue;
            }

            logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to call epoll_wait! (%d)\n",
                    __FUNCTION__, __LINE__, errno);

            status = UIOHOOK_FAILURE;
            break;
        }

        for (int i = 0; i < event_count && is_running; i++) {
            void *ptr = event_buffer[i].data.ptr;

            if (ptr == &stop_tag) {
                // hook_stop() signaled shutdown.
                is_running = false;
            } else if (ptr == &monitor_tag) {
                hook_monitor_proc(epoll_fd);
            } else {
                hook_device_proc(epoll_fd, ptr);
            }
        }
    }

    // Fire the hook disabled event and unload the input helper.
    dispatch_hook_disabled();

    hook_teardown(epoll_fd);

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Status: %#X.\n",
            __FUNCTION__, __LINE__, status);

    return status;
}


UIOHOOK_API int hook_stop() {
    int status = UIOHOOK_FAILURE;

    if (stop_fd >= 0) {
        uint64_t signal = 1;
        if (write(stop_fd, &signal, sizeof(signal)) == sizeof(signal)) {
            status = UIOHOOK_SUCCESS;
        } else {
            logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to write to stop file descriptor! (%d)\n",
                    __FUNCTION__, __LINE__, errno);
        }
    } else {
        // FIXME This message should be normalized across platforms.
        logger(LOG_LEVEL_WARN, "%s [%u]: The hook is not currently running!\n",
                __FUNCTION__, __LINE__);
    }

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Status: %#X.\n",
            __FUNCTION__, __LINE__, status);

    return status;
}
