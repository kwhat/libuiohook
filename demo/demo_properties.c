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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <uiohook.h>


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

int main() {
    // Disable the logger.
    hook_set_logger_proc(&logger_proc, NULL);

    // Retrieves current monitor layout and size.
    unsigned char count;
    screen_data* monitors = hook_create_screen_info(&count);
    logger(LOG_LEVEL_INFO, "Monitors Found:\t%u\n", count);
    for (int i = 0; i < count; i++) {
        logger(LOG_LEVEL_INFO, "\t%3u) %4u x %-4u (%5d, %-5d)\n",
            monitors[i].number,
            monitors[i].width, monitors[i].height,
            monitors[i].x, monitors[i].y);
    }
    logger(LOG_LEVEL_INFO, "\n");

    // Retrieves the keyboard auto repeat rate.
    long int repeat_rate = hook_get_auto_repeat_rate();
    if (repeat_rate >= 0) {
        logger(LOG_LEVEL_INFO, "Auto Repeat Rate:\t%ld\n", repeat_rate);
    } else {
        logger(LOG_LEVEL_WARN, "Failed to acquire keyboard auto repeat rate!\n");
    }

    // Retrieves the keyboard auto repeat delay.
    long int repeat_delay = hook_get_auto_repeat_delay();
    if (repeat_delay >= 0) {
        logger(LOG_LEVEL_INFO, "Auto Repeat Delay:\t%ld\n", repeat_delay);
    } else {
        logger(LOG_LEVEL_WARN, "Failed to acquire keyboard auto repeat delay!\n");
    }

    // Retrieves the mouse acceleration multiplier.
    long int acceleration_multiplier = hook_get_pointer_acceleration_multiplier();
    if (acceleration_multiplier >= 0) {
        logger(LOG_LEVEL_INFO, "Mouse Acceleration Multiplier:\t%ld\n", acceleration_multiplier);
    } else {
        logger(LOG_LEVEL_WARN, "Failed to acquire mouse acceleration multiplier!\n");
    }

    // Retrieves the mouse acceleration threshold.
    long int acceleration_threshold = hook_get_pointer_acceleration_threshold();
    if (acceleration_threshold >= 0) {
        logger(LOG_LEVEL_INFO, "Mouse Acceleration Threshold:\t%ld\n", acceleration_threshold);
    } else {
        logger(LOG_LEVEL_WARN, "Failed to acquire mouse acceleration threshold!\n");
    }

    // Retrieves the mouse sensitivity.
    long int sensitivity = hook_get_pointer_sensitivity();
    if (sensitivity >= 0) {
        logger(LOG_LEVEL_INFO, "Mouse Sensitivity:\t%ld\n", sensitivity);
    } else {
        logger(LOG_LEVEL_WARN, "Failed to acquire mouse sensitivity value!\n");
    }

    // Retrieves the double/triple click interval.
    long int click_time = hook_get_multi_click_time();
    if (click_time >= 0) {
        logger(LOG_LEVEL_INFO, "Multi-Click Time:\t%ld\n", click_time);
    } else {
        logger(LOG_LEVEL_WARN, "Failed to acquire mouse multi-click time!\n");
    }

    return EXIT_SUCCESS;
}
