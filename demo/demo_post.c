/* libUIOHook: Cross-platform keyboard and mouse hooking from userland.
 * Copyright (C) 2006-2022 Alexander Barker.  All Rights Reserved.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <uiohook.h>

#ifdef _WIN32
#include <windows.h>
#define sleep(x) Sleep(1000 * (x))
#else
#include <unistd.h>
#endif

// Virtual event pointers
static uiohook_event *event = NULL;


static void logger_proc(unsigned int level, void *user_data, const char *format, va_list args) {
    switch (level) {
        case LOG_LEVEL_INFO:
            vfprintf(stdout, format, args);
            break;

        case LOG_LEVEL_WARN:
        case LOG_LEVEL_ERROR:
            vfprintf(stderr, format, args);
            break;
    }
}

static void logger(unsigned int level, const char *format, ...) {
    va_list args;

    va_start(args, format);
    logger_proc(level, NULL, format, args);
    va_end(args);
}

// TODO Implement CLI options.
//int main(int argc, char *argv[]) {
int main() {
    // Set the logger callback for library output.
    hook_set_logger_proc(&logger_proc, NULL);

    // Allocate memory for the virtual events only once.
    event = (uiohook_event *) malloc(sizeof(uiohook_event));
    if (event == NULL) {
        return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }

    sleep(1);

    //* Click drag example
    event->type = EVENT_MOUSE_PRESSED;
    event->data.mouse.button = MOUSE_BUTTON1;
    #ifdef _WIN32
    event->data.mouse.x = 4;
    event->data.mouse.y = 4;
    #else
    event->data.mouse.x = 10;
    event->data.mouse.y = 35;
    #endif
    hook_post_event(event);

    sleep(1);

    for (int i = 0; i < 275; i++) {
        event->type = EVENT_MOUSE_MOVED;
        event->data.mouse.button = MOUSE_NOBUTTON;
        event->data.mouse.x = i;
        event->data.mouse.y = i;
        hook_post_event(event);
    }

    sleep(1);

    event->type = EVENT_MOUSE_RELEASED;
    event->data.mouse.button = MOUSE_BUTTON1;
    hook_post_event(event);

    sleep(1);

    event->type = EVENT_KEY_PRESSED;
    event->mask = 0x00;
    event->data.keyboard.keychar = CHAR_UNDEFINED;
    event->data.keyboard.keycode = VC_ESCAPE;
    hook_post_event(event);
    //*/

    //* Key press with modifier example
    event->type = EVENT_KEY_PRESSED;
    event->mask = 0x00;
    event->data.keyboard.keychar = CHAR_UNDEFINED;

    event->data.keyboard.keycode = VC_SHIFT_L;
    hook_post_event(event);

    event->data.keyboard.keycode = VC_A;
    hook_post_event(event);


    event->type = EVENT_KEY_RELEASED;

    event->data.keyboard.keycode = VC_A;
    hook_post_event(event);

    event->data.keyboard.keycode = VC_SHIFT_L;
    hook_post_event(event);
    //*/

    //* Mouse Wheel Event
    event->type = EVENT_MOUSE_WHEEL;

    event->data.wheel.x = 675;
    event->data.wheel.y = 675;
    event->data.wheel.amount = 3;
    event->data.wheel.rotation = 1;
    hook_post_event(event);
    //*/

    free(event);

    return UIOHOOK_SUCCESS;
}
