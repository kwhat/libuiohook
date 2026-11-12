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

#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <uiohook.h>
#include <unistd.h>
#include <wayland-client-core.h>

#include "logger.h"
#include "wayland_helper.h"

// libwayland-client is dlopen'd, so only its functions need resolving.  Every
// wl_*_interface data symbol is instead compiled in from the generated protocol code
// (wayland-protocol.c / xdg-output-protocol.c), which sidesteps the extern-symbol
// conflict a data-symbol macro redirect would create.
static void *lib_wayland = NULL;

// Serializes the one-time dlopen + symbol resolution in load_wayland() so concurrent callers
// (e.g. the hook thread seeding the pointer while a property query runs) cannot race it.
static pthread_mutex_t wayland_load_mutex = PTHREAD_MUTEX_INITIALIZER;

static struct wl_display *(*p_wl_display_connect)(const char *);
static void               (*p_wl_display_disconnect)(struct wl_display *);
static int                (*p_wl_display_roundtrip)(struct wl_display *);
static struct wl_proxy   *(*p_wl_proxy_marshal_flags)(struct wl_proxy *, uint32_t, const struct wl_interface *, uint32_t, uint32_t, ...);
static int                (*p_wl_proxy_add_listener)(struct wl_proxy *, void (**)(void), void *);
static void               (*p_wl_proxy_destroy)(struct wl_proxy *);
static uint32_t           (*p_wl_proxy_get_version)(struct wl_proxy *);
static void              *(*p_wl_proxy_get_user_data)(struct wl_proxy *);
static void               (*p_wl_proxy_set_user_data)(struct wl_proxy *, void *);

// Route the inline protocol wrappers (included just below) through the resolved pointers.
#define wl_display_connect      p_wl_display_connect
#define wl_display_disconnect   p_wl_display_disconnect
#define wl_display_roundtrip    p_wl_display_roundtrip
#define wl_proxy_marshal_flags  p_wl_proxy_marshal_flags
#define wl_proxy_add_listener   p_wl_proxy_add_listener
#define wl_proxy_destroy        p_wl_proxy_destroy
#define wl_proxy_get_version    p_wl_proxy_get_version
#define wl_proxy_get_user_data  p_wl_proxy_get_user_data
#define wl_proxy_set_user_data  p_wl_proxy_set_user_data

#include <wayland-client-protocol.h>
#include "xdg-output-client-protocol.h"
#include "wlr-layer-shell-client-protocol.h"

static void unload_wayland(void);

// Resolve one libwayland-client symbol into its function pointer.  target is the address of the
// p_wl_* pointer cast to void **, matching how dlsym returns an untyped object pointer.
static bool wl_resolve(void **target, const char *name) {
    *target = dlsym(lib_wayland, name);
    if (*target == NULL) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Failed to resolve %s: %s\n",
                __FUNCTION__, __LINE__, name, dlerror());
        return false;
    }

    return true;
}

static int load_wayland(void) {
    pthread_mutex_lock(&wayland_load_mutex);

    if (lib_wayland != NULL) {
        pthread_mutex_unlock(&wayland_load_mutex);
        return UIOHOOK_SUCCESS;
    }

    lib_wayland = dlopen("libwayland-client.so.0", RTLD_NOW | RTLD_LOCAL);
    if (lib_wayland == NULL) {
        logger(LOG_LEVEL_WARN, "%s [%u]: libwayland-client is unavailable: %s\n",
                __FUNCTION__, __LINE__, dlerror());
        pthread_mutex_unlock(&wayland_load_mutex);
        return UIOHOOK_FAILURE;
    }

    static const struct {
        void **target;
        const char *name;
    } symbols[] = {
        { (void **) &p_wl_display_connect,     "wl_display_connect"     },
        { (void **) &p_wl_display_disconnect,  "wl_display_disconnect"  },
        { (void **) &p_wl_display_roundtrip,   "wl_display_roundtrip"   },
        { (void **) &p_wl_proxy_marshal_flags, "wl_proxy_marshal_flags" },
        { (void **) &p_wl_proxy_add_listener,  "wl_proxy_add_listener"  },
        { (void **) &p_wl_proxy_destroy,       "wl_proxy_destroy"       },
        { (void **) &p_wl_proxy_get_version,   "wl_proxy_get_version"   },
        { (void **) &p_wl_proxy_get_user_data, "wl_proxy_get_user_data" },
        { (void **) &p_wl_proxy_set_user_data, "wl_proxy_set_user_data" }
    };

    for (size_t i = 0; i < sizeof(symbols) / sizeof(symbols[0]); i++) {
        if (!wl_resolve(symbols[i].target, symbols[i].name)) {
            unload_wayland();
            pthread_mutex_unlock(&wayland_load_mutex);
            return UIOHOOK_FAILURE;
        }
    }

    pthread_mutex_unlock(&wayland_load_mutex);
    return UIOHOOK_SUCCESS;
}

static void unload_wayland(void) {
    if (lib_wayland != NULL) {
        dlclose(lib_wayland);
        lib_wayland = NULL;
    }
}


// One monitor discovered during enumeration.
struct wl_monitor {
    struct wl_output *output;
    struct zxdg_output_v1 *xdg_output;

    int32_t x, y;
    int32_t width, height;
    bool have_position;
    bool have_size;

    struct wl_monitor *next;
};

// Enumeration state threaded through the listeners as their user data.
struct wl_enum {
    struct zxdg_output_manager_v1 *manager;
    struct wl_monitor *monitors;
};


static void xdg_output_logical_position(void *data, struct zxdg_output_v1 *xdg_output, int32_t x, int32_t y) {
    struct wl_monitor *monitor = data;
    monitor->x = x;
    monitor->y = y;
    monitor->have_position = true;
}

static void xdg_output_logical_size(void *data, struct zxdg_output_v1 *xdg_output, int32_t width, int32_t height) {
    struct wl_monitor *monitor = data;
    monitor->width = width;
    monitor->height = height;
    monitor->have_size = true;
}

static void xdg_output_done(void *data, struct zxdg_output_v1 *xdg_output) {
    // Deprecated since xdg-output v3; geometry is applied as it arrives above.
}

static void xdg_output_name(void *data, struct zxdg_output_v1 *xdg_output, const char *name) {
    // Unused; monitor identity is not needed for the bounding box.
}

static void xdg_output_description(void *data, struct zxdg_output_v1 *xdg_output, const char *description) {
    // Unused.
}

static const struct zxdg_output_v1_listener xdg_output_listener = {
    .logical_position = xdg_output_logical_position,
    .logical_size = xdg_output_logical_size,
    .done = xdg_output_done,
    .name = xdg_output_name,
    .description = xdg_output_description
};


static void registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    struct wl_enum *state = data;

    if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0) {
        // Logical position/size require xdg-output version 3 or newer.
        if (version >= 3) {
            state->manager = wl_registry_bind(registry, name, &zxdg_output_manager_v1_interface, 3);
        }
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        struct wl_monitor *monitor = calloc(1, sizeof(struct wl_monitor));
        if (monitor != NULL) {
            uint32_t bind_version = version < 2 ? version : 2;
            monitor->output = wl_registry_bind(registry, name, &wl_output_interface, bind_version);
            if (monitor->output == NULL) {
                free(monitor);
                return;
            }

            monitor->next = state->monitors;
            state->monitors = monitor;
        }
    }
}

static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
    // One-shot enumeration; hotplug is not tracked here.
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove
};


screen_data *wayland_create_screen_info(unsigned char *count) {
    *count = 0;

    if (load_wayland() != UIOHOOK_SUCCESS) {
        return NULL;
    }

    struct wl_display *display = wl_display_connect(NULL);
    if (display == NULL) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Failed to connect to the Wayland display!\n",
                __FUNCTION__, __LINE__);
        return NULL;
    }

    struct wl_enum state = { 0 };
    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, &state);

    // First roundtrip: receive the globals (output manager and each wl_output).
    if (wl_display_roundtrip(display) < 0) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Wayland roundtrip failed during screen enumeration!\n",
                __FUNCTION__, __LINE__);
    }

    screen_data *screens = NULL;

    if (state.manager != NULL) {
        // Now that the manager is known, request an xdg_output per monitor.  Binding
        // order is not guaranteed, so this is done after the globals settle.
        for (struct wl_monitor *monitor = state.monitors; monitor != NULL; monitor = monitor->next) {
            monitor->xdg_output = zxdg_output_manager_v1_get_xdg_output(state.manager, monitor->output);
            zxdg_output_v1_add_listener(monitor->xdg_output, &xdg_output_listener, monitor);
        }

        // Second roundtrip: receive logical_position / logical_size for each output.
        if (wl_display_roundtrip(display) < 0) {
            logger(LOG_LEVEL_WARN, "%s [%u]: Wayland roundtrip failed reading output geometry!\n",
                    __FUNCTION__, __LINE__);
        }

        unsigned int found = 0;
        for (struct wl_monitor *monitor = state.monitors; monitor != NULL; monitor = monitor->next) {
            if (monitor->have_position && monitor->have_size) {
                found++;
            }
        }

        if (found > UINT8_MAX) {
            found = UINT8_MAX;
            logger(LOG_LEVEL_WARN, "%s [%u]: Screen count overflow detected!\n",
                    __FUNCTION__, __LINE__);
        }

        if (found > 0) {
            screens = malloc(sizeof(screen_data) * found);
            if (screens != NULL) {
                unsigned int i = 0;
                for (struct wl_monitor *monitor = state.monitors; monitor != NULL && i < found; monitor = monitor->next) {
                    if (monitor->have_position && monitor->have_size) {
                        screens[i] = (screen_data) {
                            .number = (uint8_t) (i + 1),
                            .x = (int16_t) monitor->x,
                            .y = (int16_t) monitor->y,
                            .width = (uint16_t) monitor->width,
                            .height = (uint16_t) monitor->height
                        };
                        i++;
                    }
                }

                *count = (uint8_t) found;
            }
        }
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: Wayland compositor does not support xdg-output!\n",
                __FUNCTION__, __LINE__);
    }

    // Tear down every proxy created above before disconnecting.
    struct wl_monitor *monitor = state.monitors;
    while (monitor != NULL) {
        struct wl_monitor *next = monitor->next;

        if (monitor->xdg_output != NULL) {
            zxdg_output_v1_destroy(monitor->xdg_output);
        }

        if (monitor->output != NULL) {
            wl_output_destroy(monitor->output);
        }

        free(monitor);
        monitor = next;
    }

    if (state.manager != NULL) {
        zxdg_output_manager_v1_destroy(state.manager);
    }

    wl_registry_destroy(registry);
    wl_display_disconnect(display);

    return screens;
}


// State threaded through the seat/keyboard listeners while reading the keymap and repeat
// settings from the compositor.
struct wl_seat_capture {
    bool want_keymap;

    struct wl_seat *seat;
    struct wl_keyboard *keyboard;

    char *keymap;          // malloc'd XKB text, NULL when unread or not requested.
    int32_t repeat_rate;   // keys per second, -1 when not reported.
    int32_t repeat_delay;  // milliseconds, -1 when not reported.
};

static void keyboard_keymap(void *data, struct wl_keyboard *keyboard, uint32_t format, int32_t fd, uint32_t size) {
    struct wl_seat_capture *capture = data;

    if (capture->want_keymap && capture->keymap == NULL && format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        // The fd content is NUL terminated and size includes the terminator, so the copy
        // is a ready-to-use string for xkb_keymap_new_from_string().
        void *map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (map != MAP_FAILED) {
            capture->keymap = malloc(size);
            if (capture->keymap != NULL) {
                memcpy(capture->keymap, map, size);
            }

            munmap(map, size);
        }
    }

    close(fd);
}

static void keyboard_repeat_info(void *data, struct wl_keyboard *keyboard, int32_t rate, int32_t delay) {
    struct wl_seat_capture *capture = data;
    capture->repeat_rate = rate;
    capture->repeat_delay = delay;
}

static void keyboard_enter(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys) { }
static void keyboard_leave(void *data, struct wl_keyboard *keyboard, uint32_t serial, struct wl_surface *surface) { }
static void keyboard_key(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state) { }
static void keyboard_modifiers(void *data, struct wl_keyboard *keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group) { }

static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = keyboard_keymap,
    .enter = keyboard_enter,
    .leave = keyboard_leave,
    .key = keyboard_key,
    .modifiers = keyboard_modifiers,
    .repeat_info = keyboard_repeat_info
};

static void seat_capabilities(void *data, struct wl_seat *seat, uint32_t capabilities) {
    struct wl_seat_capture *capture = data;

    if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && capture->keyboard == NULL) {
        capture->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(capture->keyboard, &keyboard_listener, capture);
    }
}

static void seat_name(void *data, struct wl_seat *seat, const char *name) { }

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities,
    .name = seat_name
};

static void seat_registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    struct wl_seat_capture *capture = data;

    if (capture->seat == NULL && strcmp(interface, wl_seat_interface.name) == 0) {
        // repeat_info requires wl_seat version 4; keymap works from version 1.
        uint32_t bind_version = version < 7 ? version : 7;
        capture->seat = wl_registry_bind(registry, name, &wl_seat_interface, bind_version);
        if (capture->seat != NULL) {
            wl_seat_add_listener(capture->seat, &seat_listener, capture);
        }
    }
}

static void seat_registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) { }

static const struct wl_registry_listener seat_registry_listener = {
    .global = seat_registry_global,
    .global_remove = seat_registry_global_remove
};

// Connect, bind the first seat's keyboard and pump enough roundtrips to receive the
// keymap and repeat_info events, then tear everything down.
static void capture_seat_info(struct wl_seat_capture *capture) {
    capture->seat = NULL;
    capture->keyboard = NULL;
    capture->keymap = NULL;
    capture->repeat_rate = -1;
    capture->repeat_delay = -1;

    if (load_wayland() != UIOHOOK_SUCCESS) {
        return;
    }

    struct wl_display *display = wl_display_connect(NULL);
    if (display == NULL) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Failed to connect to the Wayland display!\n",
                __FUNCTION__, __LINE__);
        return;
    }

    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &seat_registry_listener, capture);

    // Roundtrip 1 binds the seat, 2 delivers its capabilities (and binds the keyboard),
    // 3 delivers the keyboard's keymap / repeat_info events.
    bool success = wl_display_roundtrip(display) >= 0;
    if (success && capture->seat != NULL) {
        success = wl_display_roundtrip(display) >= 0 && wl_display_roundtrip(display) >= 0;
    }

    if (!success) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Wayland roundtrip failed while reading seat info!\n",
                __FUNCTION__, __LINE__);
    }

    if (capture->keyboard != NULL) {
        wl_keyboard_destroy(capture->keyboard);
    }

    if (capture->seat != NULL) {
        wl_seat_destroy(capture->seat);
    }

    wl_registry_destroy(registry);
    wl_display_disconnect(display);
}

char *wayland_get_keymap(int32_t *repeat_rate, int32_t *repeat_delay) {
    struct wl_seat_capture capture = { .want_keymap = true };
    capture_seat_info(&capture);

    if (repeat_rate != NULL) {
        *repeat_rate = capture.repeat_rate;
    }

    if (repeat_delay != NULL) {
        *repeat_delay = capture.repeat_delay;
    }

    return capture.keymap;
}

int wayland_get_repeat_info(int32_t *repeat_rate, int32_t *repeat_delay) {
    struct wl_seat_capture capture = { .want_keymap = false };
    capture_seat_info(&capture);

    if (capture.repeat_rate < 0 && capture.repeat_delay < 0) {
        return UIOHOOK_FAILURE;
    }

    if (repeat_rate != NULL) {
        *repeat_rate = capture.repeat_rate;
    }

    if (repeat_delay != NULL) {
        *repeat_delay = capture.repeat_delay;
    }

    return UIOHOOK_SUCCESS;
}


// One output during a pointer probe: its layer-shell overlay and its layout origin.
struct probe_output {
    struct wl_output *output;
    struct zxdg_output_v1 *xdg_output;
    int32_t origin_x, origin_y;

    struct wl_surface *surface;
    struct zwlr_layer_surface_v1 *layer_surface;
    struct wl_buffer *buffer;

    struct probe_output *next;
};

struct probe_state {
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct zwlr_layer_shell_v1 *layer_shell;
    struct zxdg_output_manager_v1 *xdg_output_manager;
    struct wl_seat *seat;
    struct wl_pointer *pointer;

    struct probe_output *outputs;

    bool have_position;
    int16_t global_x, global_y;
};

// Create a fully transparent ARGB8888 shm buffer.  The overlay only needs to be mapped to
// receive pointer focus; it is never meant to be seen.
static struct wl_buffer *probe_create_buffer(struct probe_state *state, int32_t width, int32_t height) {
    if (width <= 0 || height <= 0) {
        return NULL;
    }

    int32_t stride = width * 4;
    size_t size = (size_t) stride * height;

    // shm_open (POSIX) is used instead of memfd_create so the file does not depend on _GNU_SOURCE.
    // The object is created O_EXCL under a name unique to this process+time and unlinked right away,
    // leaving an anonymous fd that stays valid until close().
    int fd = -1;
    for (int attempt = 0; attempt < 16; attempt++) {
        char name[64];
        snprintf(name, sizeof(name), "/uiohook-cursor-probe-%d-%ld-%d",
                 (int) getpid(), (long) time(NULL), attempt);

        fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
        if (fd >= 0) {
            shm_unlink(name);
            break;
        }
    }

    if (fd < 0) {
        return NULL;
    }

    struct wl_buffer *buffer = NULL;
    if (ftruncate(fd, size) == 0) {
        void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (data != MAP_FAILED) {
            memset(data, 0, size);
            munmap(data, size);

            struct wl_shm_pool *pool = wl_shm_create_pool(state->shm, fd, size);
            buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);
            wl_shm_pool_destroy(pool);
        }
    }

    close(fd);
    return buffer;
}

static void probe_xdg_output_logical_position(void *data, struct zxdg_output_v1 *xdg_output, int32_t x, int32_t y) {
    struct probe_output *output = data;
    output->origin_x = x;
    output->origin_y = y;
}

static void probe_xdg_output_logical_size(void *data, struct zxdg_output_v1 *xdg_output, int32_t width, int32_t height) { }
static void probe_xdg_output_done(void *data, struct zxdg_output_v1 *xdg_output) { }
static void probe_xdg_output_name(void *data, struct zxdg_output_v1 *xdg_output, const char *name) { }
static void probe_xdg_output_description(void *data, struct zxdg_output_v1 *xdg_output, const char *description) { }

static const struct zxdg_output_v1_listener probe_xdg_output_listener = {
    .logical_position = probe_xdg_output_logical_position,
    .logical_size = probe_xdg_output_logical_size,
    .done = probe_xdg_output_done,
    .name = probe_xdg_output_name,
    .description = probe_xdg_output_description
};

static void layer_surface_configure(void *data, struct zwlr_layer_surface_v1 *layer_surface, uint32_t serial, uint32_t width, uint32_t height) {
    struct probe_output *output = data;
    // Need the probe_state to reach the shm; stashed in the surface user data below.
    struct probe_state *state = wl_surface_get_user_data(output->surface);

    zwlr_layer_surface_v1_ack_configure(layer_surface, serial);

    if (output->buffer == NULL) {
        output->buffer = probe_create_buffer(state, (int32_t) width, (int32_t) height);
        if (output->buffer != NULL) {
            wl_surface_attach(output->surface, output->buffer, 0, 0);
            wl_surface_commit(output->surface);
        }
    }
}

static void layer_surface_closed(void *data, struct zwlr_layer_surface_v1 *layer_surface) { }

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure,
    .closed = layer_surface_closed
};

// Narrow a global coordinate to the int16_t the public API uses, clamping instead of wrapping.
// A layout wider/taller than +-32767px is unlikely, but pinning to the edge beats a sign-flip.
static int16_t clamp_coordinate(int32_t value) {
    if (value > INT16_MAX) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Pointer coordinate overflow (%d) clamped to %d!\n",
                __FUNCTION__, __LINE__, value, INT16_MAX);
        return INT16_MAX;
    }

    if (value < INT16_MIN) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Pointer coordinate underflow (%d) clamped to %d!\n",
                __FUNCTION__, __LINE__, value, INT16_MIN);
        return INT16_MIN;
    }

    return (int16_t) value;
}

static void probe_pointer_enter(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t sx, wl_fixed_t sy) {
    struct probe_state *state = data;
    if (state->have_position) {
        return;
    }

    // Match the entered surface to its output and add the output's layout origin.
    for (struct probe_output *output = state->outputs; output != NULL; output = output->next) {
        if (output->surface == surface) {
            state->global_x = clamp_coordinate(output->origin_x + wl_fixed_to_int(sx));
            state->global_y = clamp_coordinate(output->origin_y + wl_fixed_to_int(sy));
            state->have_position = true;
            break;
        }
    }
}

static void probe_pointer_leave(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface) {
    // The pointer left one of our overlays (fires as we tear them down).  Intentionally
    // blank: the probe samples the cursor once from the enter event, nothing to do here.
}

static void probe_pointer_motion(void *data, struct wl_pointer *pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy) {
    // The pointer moved within an overlay.  Intentionally blank: the position comes from
    // the enter event, so ongoing motion is irrelevant to a one-shot probe.
}

static void probe_pointer_button(void *data, struct wl_pointer *pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
    // A button was pressed/released over an overlay.  Intentionally blank: the probe never
    // consumes or forwards input, it only reads the cursor position.
}

static void probe_pointer_axis(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis, wl_fixed_t value) {
    // A scroll/axis event occurred over an overlay.  Intentionally blank for the same
    // reason: the probe cares only about where the pointer is.
}

// wl_pointer is bound at version 1, so only these five classic events are ever delivered;
// the newer members of the listener struct stay NULL and are never called.
static const struct wl_pointer_listener probe_pointer_listener = {
    .enter = probe_pointer_enter,
    .leave = probe_pointer_leave,
    .motion = probe_pointer_motion,
    .button = probe_pointer_button,
    .axis = probe_pointer_axis
};

static void probe_seat_capabilities(void *data, struct wl_seat *seat, uint32_t capabilities) {
    struct probe_state *state = data;
    if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && state->pointer == NULL) {
        state->pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(state->pointer, &probe_pointer_listener, state);
    }
}

static void probe_seat_name(void *data, struct wl_seat *seat, const char *name) { }

static const struct wl_seat_listener probe_seat_listener = {
    .capabilities = probe_seat_capabilities,
    .name = probe_seat_name
};

static void probe_registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    struct probe_state *state = data;

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        state->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 1);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        state->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        state->layer_shell = wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1);
    } else if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0 && version >= 3) {
        state->xdg_output_manager = wl_registry_bind(registry, name, &zxdg_output_manager_v1_interface, 3);
    } else if (strcmp(interface, wl_seat_interface.name) == 0 && state->seat == NULL) {
        state->seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
        if (state->seat != NULL) {
            wl_seat_add_listener(state->seat, &probe_seat_listener, state);
        }
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        struct probe_output *output = calloc(1, sizeof(struct probe_output));
        if (output != NULL) {
            output->output = wl_registry_bind(registry, name, &wl_output_interface, version < 2 ? version : 2);
            if (output->output == NULL) {
                free(output);
                return;
            }

            output->next = state->outputs;
            state->outputs = output;
        }
    }
}

static void probe_registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) { }

static const struct wl_registry_listener probe_registry_listener = {
    .global = probe_registry_global,
    .global_remove = probe_registry_global_remove
};

int wayland_query_pointer_position(int16_t *x, int16_t *y) {
    if (load_wayland() != UIOHOOK_SUCCESS) {
        return UIOHOOK_FAILURE;
    }

    struct wl_display *display = wl_display_connect(NULL);
    if (display == NULL) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Failed to connect to the Wayland display!\n",
                __FUNCTION__, __LINE__);
        return UIOHOOK_FAILURE;
    }

    struct probe_state state = { 0 };
    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &probe_registry_listener, &state);
    wl_display_roundtrip(display);   // bind globals, add seat listener

    int status = UIOHOOK_FAILURE;

    if (state.compositor == NULL || state.shm == NULL || state.layer_shell == NULL
            || state.xdg_output_manager == NULL || state.outputs == NULL) {
        logger(LOG_LEVEL_WARN, "%s [%u]: Wayland pointer probe needs layer-shell + xdg-output; unavailable.\n",
                __FUNCTION__, __LINE__);
    } else {
        // Resolve each output's layout origin, and deliver the seat's pointer capability.
        for (struct probe_output *output = state.outputs; output != NULL; output = output->next) {
            output->xdg_output = zxdg_output_manager_v1_get_xdg_output(state.xdg_output_manager, output->output);
            zxdg_output_v1_add_listener(output->xdg_output, &probe_xdg_output_listener, output);
        }
        wl_display_roundtrip(display);   // origins + seat capabilities (binds pointer)

        // Put a transparent full-output overlay on every output so the cursor is over one.
        for (struct probe_output *output = state.outputs; output != NULL; output = output->next) {
            output->surface = wl_compositor_create_surface(state.compositor);
            wl_surface_set_user_data(output->surface, &state);

            output->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
                    state.layer_shell, output->surface, output->output,
                    ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "uiohook-cursor-probe");

            zwlr_layer_surface_v1_add_listener(output->layer_surface, &layer_surface_listener, output);
            zwlr_layer_surface_v1_set_anchor(output->layer_surface,
                    ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | 
                    ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | 
                    ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | 
                    ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT
                );
            zwlr_layer_surface_v1_set_exclusive_zone(output->layer_surface, -1);
            zwlr_layer_surface_v1_set_size(output->layer_surface, 0, 0);
            wl_surface_commit(output->surface);
        }

        // Pump events: the configure handler attaches buffers (mapping the overlays), then
        // the compositor delivers pointer.enter on whichever output holds the cursor.
        for (int i = 0; i < 50 && !state.have_position; i++) {
            if (wl_display_roundtrip(display) < 0) {
                break;
            }
        }

        if (state.have_position) {
            *x = state.global_x;
            *y = state.global_y;
            status = UIOHOOK_SUCCESS;
        } else {
            logger(LOG_LEVEL_WARN, "%s [%u]: No pointer enter received during position probe!\n",
                    __FUNCTION__, __LINE__);
        }
    }

    // Teardown.
    struct probe_output *output = state.outputs;
    while (output != NULL) {
        struct probe_output *next = output->next;

        if (output->layer_surface != NULL) { zwlr_layer_surface_v1_destroy(output->layer_surface); }
        if (output->surface       != NULL) { wl_surface_destroy(output->surface);                  }
        if (output->buffer        != NULL) { wl_buffer_destroy(output->buffer);                    }
        if (output->xdg_output    != NULL) { zxdg_output_v1_destroy(output->xdg_output);           }
        if (output->output        != NULL) { wl_output_destroy(output->output);                    }

        free(output);
        output = next;
    }

    if (state.pointer            != NULL) { wl_pointer_destroy(state.pointer);                        }
    if (state.seat               != NULL) { wl_seat_destroy(state.seat);                              }
    if (state.layer_shell        != NULL) { zwlr_layer_shell_v1_destroy(state.layer_shell);           }
    if (state.xdg_output_manager != NULL) { zxdg_output_manager_v1_destroy(state.xdg_output_manager); }
    if (state.shm                != NULL) { wl_shm_destroy(state.shm);                                }
    if (state.compositor         != NULL) { wl_compositor_destroy(state.compositor);                  }

    wl_registry_destroy(registry);
    wl_display_disconnect(display);

    return status;
}
