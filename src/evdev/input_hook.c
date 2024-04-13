/* libUIOHook: Cross-platform keyboard and mouse hooking from userland.
 * Copyright (C) 2006-2024 Alexander Barker.  All Rights Reserved.
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
#include <glob.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <uiohook.h>
#include <unistd.h>

#include "dispatch_event.h"
#include "input_helper.h"
#include "logger.h"

#define EVENT_GLOB_PATTERN "/dev/input/event*"


struct input_hook {
    struct libevdev *evdev;
    struct libevdev_uinput *uinput;
};

static int rel_x = 0, rel_y = 0;
static int wheel_h = 0, wheel_v = 0;


static bool hook_key_proc(struct input_event *ev) {
    bool consumed = false;

    #ifdef USE_EPOCH_TIME
    uint64_t timestamp = get_unix_timestamp(&ev->time);
    #else
    uint64_t timestamp = get_seq_timestamp();
    #endif

    xkb_keycode_t keycode = event_to_keycode(ev->code);
    xkb_keysym_t keysym = event_to_keysym(keycode, ev->value);

    if (ev->value > 0) {
      consumed = dispatch_key_press(timestamp, keycode, keysym);
    } else {
      consumed = dispatch_key_release(timestamp, keycode, keysym);
    }

    return consumed;
}

static bool hook_button_proc(struct input_event *ev) {
    bool consumed = false;

    if (ev->value > 0) {
        consumed = dispatch_mouse_press(ev);
    } else {
        consumed = dispatch_mouse_release(ev);
    }

    return consumed;
}

static bool hook_sync_proc(struct input_event *ev) {
    bool consumed = false;

    #ifdef USE_EPOCH_TIME
    uint64_t timestamp = get_unix_timestamp(&ev->time);
    #else
    uint64_t timestamp = get_seq_timestamp();
    #endif

    if (rel_x != 0 || rel_y != 0) {
        consumed = dispatch_mouse_move(timestamp, rel_x, rel_y);
        rel_x = rel_y = 0;
    }

    if (wheel_v != 0) {
        consumed = dispatch_mouse_wheel(timestamp, wheel_v, WHEEL_VERTICAL_DIRECTION);
        wheel_v = 0;
    }

    if (wheel_h != 0) {
        consumed = dispatch_mouse_wheel(timestamp, wheel_v, WHEEL_HORIZONTAL_DIRECTION);
        wheel_h = 0;
    }

    return consumed;
}

static bool hook_event_proc(struct input_event *ev) {
    bool consumed = false;

    switch (ev->type) {
        case EV_KEY:
            switch (ev->code) {
                case BTN_LEFT:
                case BTN_RIGHT:
                case BTN_MIDDLE:
                case BTN_SIDE:
                case BTN_EXTRA:
                case BTN_FORWARD:
                case BTN_BACK:
                    consumed = hook_button_proc(ev);
                    break;

                default:
                    consumed = hook_key_proc(ev);
            }
            break;

        case EV_REL:
            switch (ev->code) {
                case REL_X:
                    rel_x += ev->value;
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
            break;

        case EV_SYN:
            consumed = hook_sync_proc(ev);
            break;
    }

    return consumed;
}

static int create_hook(char *path, struct input_hook **hook) {
    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to open input device: %s! (%d)\n",
                __FUNCTION__, __LINE__,
                path, errno);
        return UIOHOOK_FAILURE;
    }

    *hook = malloc(sizeof(struct input_hook));
    if (hook == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to allocate memory for evdev buffer!\n",
                __FUNCTION__, __LINE__);
        return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }

    int err = libevdev_new_from_fd(fd, &(*hook)->evdev);
    if (err < 0) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to create evdev from file descriptor! (%d)\n",
                __FUNCTION__, __LINE__,
                err);
        return UIOHOOK_FAILURE;
    }

    // We need to wait until no key is being pressed becasue we are going to grab below and that will cause keys to stick.
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

    char *label;
    if (libevdev_has_event_type((*hook)->evdev, EV_REP) && libevdev_has_event_code((*hook)->evdev, EV_KEY, KEY_ESC)) {
        label = "keyboard";
    } else if (libevdev_has_event_type((*hook)->evdev, EV_REL) && libevdev_has_event_code((*hook)->evdev, EV_KEY, BTN_LEFT)) {
        label = "pointing";
    } else {
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Unsupported input device: %s.\n",
                __FUNCTION__, __LINE__,
                path);
        return UIOHOOK_FAILURE;
    }

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Found %s device: %s.\n",
            __FUNCTION__, __LINE__,
            label, path);

    return UIOHOOK_SUCCESS;
}

static void destroy_hook(struct input_hook **hook) {
    if (*hook != NULL) {
        int fd = libevdev_get_fd((*hook)->evdev);

        if ((*hook)->evdev != NULL) {
            libevdev_grab((*hook)->evdev, LIBEVDEV_UNGRAB);
            libevdev_free((*hook)->evdev);
            (*hook)->evdev = NULL;
        }

        if ((*hook)->uinput != NULL) {
            libevdev_uinput_destroy((*hook)->uinput);
            (*hook)->uinput = NULL;
        }

        free(*hook);
        *hook = NULL;

        if (fd >= 0) {
            close(fd);
        }
    }
}


static int create_glob_buffer(glob_t *glob_buffer) {
    int status = glob(EVENT_GLOB_PATTERN,  GLOB_ERR | GLOB_NOSORT | GLOB_NOESCAPE, NULL, glob_buffer);
    switch (status) {
        case GLOB_NOSPACE:
            logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to allocate memory for glob!\n",
                    __FUNCTION__, __LINE__);
            return UIOHOOK_ERROR_OUT_OF_MEMORY;

        default:
            logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to call glob()! (%d)\n",
                    __FUNCTION__, __LINE__,
                    status);
            return UIOHOOK_FAILURE;

        case 0:
            // Success
    }

   return UIOHOOK_SUCCESS;
}

static void destroy_glob(glob_t *glob_buffer) {
    globfree(glob_buffer);
}


static int create_event_listeners(int epoll_fd, struct epoll_event **listeners) {
    glob_t glob_buffer;
    int status = create_glob_buffer(&glob_buffer);
    if (status != UIOHOOK_SUCCESS) {
        destroy_glob(&glob_buffer);
        return status;
    }

    struct epoll_event *event_buffer = *listeners = malloc(sizeof(struct epoll_event) * glob_buffer.gl_pathc);
    if (event_buffer == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to allocate memory for epoll event devices!\n",
                __FUNCTION__, __LINE__);

        destroy_glob(&glob_buffer);
        return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }

    int found = 0;
    for (int i = 0; i < glob_buffer.gl_pathc; i++) {
        struct input_hook *hook = NULL;
        if (create_hook(glob_buffer.gl_pathv[i], &hook) != UIOHOOK_SUCCESS) {
            destroy_hook(&hook);
            continue;
        }

        event_buffer[found].events = EPOLLIN;
        event_buffer[found].data.ptr = hook;

        int fd = libevdev_get_fd(hook->evdev);
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event_buffer[found]) < 0) {
            logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to add file descriptor to epoll! (%d)\n",
                    __FUNCTION__, __LINE__);
            
            destroy_hook(&hook);
            event_buffer[found].data.ptr = NULL;
            continue;
        }

        found++;
    }

    destroy_glob(&glob_buffer);

    *listeners = realloc(event_buffer, sizeof(struct epoll_event *) * found);
    if (*listeners == NULL) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Failed to realloc event listeners! (%d)\n",
                __FUNCTION__, __LINE__);

        *listeners = event_buffer;
        event_buffer = NULL;
    }

    return UIOHOOK_SUCCESS;
}

static int destroy_event_listeners(int epoll_fd, struct epoll_event **listeners) {
    // FIXME Implement
}


UIOHOOK_API int hook_run() {
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to call epoll_create1!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_ERROR_EPOLL_CREATE;
    }

    struct epoll_event *listeners = NULL;
    int status = create_event_listeners(epoll_fd, &listeners);
    if (status != UIOHOOK_SUCCESS) {
        close(epoll_fd);
        return status;
    }

    // FIXME EVENT_BUFFER_SIZE should really just be the number of listeners we got.
    #define EVENT_BUFFER_SIZE 8
    #define EVENT_BUFFER_WAIT 30

    struct epoll_event *event_buffer = malloc(sizeof(struct epoll_event) * EVENT_BUFFER_SIZE);
    if (event_buffer == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to allocate memory for device buffer!\n",
                __FUNCTION__, __LINE__);

        close(epoll_fd);
        return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }

    // FIXME This should happen on hook start event.
    load_input_helper();


    int err;
    struct input_event ev;

    // FIXME We need some way of turning this off.
    while (1) {
		int event_count = epoll_wait(epoll_fd, event_buffer, EVENT_BUFFER_SIZE, EVENT_BUFFER_WAIT * 1000);

        for (int i = 0; i < event_count; i++) {
            struct input_hook *hook = event_buffer[i].data.ptr;

            int status = -EAGAIN;
            unsigned int flags = LIBEVDEV_READ_FLAG_NORMAL;
            do {
                status = libevdev_next_event(hook->evdev, flags, &ev);
                if (status == LIBEVDEV_READ_STATUS_SYNC) {
                    flags = LIBEVDEV_READ_FLAG_SYNC;
                }

                if (status != -EAGAIN) {
                    bool consumed = hook_event_proc(&ev);

                    if (hook->uinput != NULL && !consumed) {
                        libevdev_uinput_write_event(hook->uinput, ev.type, ev.code, ev.value);
                    }
                }
            } while (status != -EAGAIN);
        }
	}

	if (close(epoll_fd)) {
	    logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to close epoll file descriptor!\n",
                __FUNCTION__, __LINE__);
        // FIXME What exactly should we do here? Return error, set epoll_fd to null?
	}

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Something, something, something, complete.\n",
            __FUNCTION__, __LINE__);

    return UIOHOOK_SUCCESS;
}


UIOHOOK_API int hook_stop() {
    int status = UIOHOOK_FAILURE;

    // FIXME Implement

    logger(LOG_LEVEL_DEBUG, "%s [%u]: Status: %#X.\n",
            __FUNCTION__, __LINE__, status);

    return status;
}
