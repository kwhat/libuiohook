/* libUIOHook: Cross-platfrom userland keyboard and mouse hooking.
 * Copyright (C) 2006-2015 Alexander Barker.  All Rights Received.
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

#include <uiohook.h>
#include <windows.h>

#include "copyright.h"
#include "logger.h"
#include "input_helper.h"

// Global Variables.
HINSTANCE hInst = NULL;
//TODO: structure should have the count as well to avoid the global. s_info[ s_array, s_count ]
screen_data *screens = NULL;


//http://msdn.microsoft.com/en-us/library/windows/desktop/dd162610(v=vs.85).aspx
//http://msdn.microsoft.com/en-us/library/dd162610%28VS.85%29.aspx
//http://msdn.microsoft.com/en-us/library/dd145061%28VS.85%29.aspx
//http://msdn.microsoft.com/en-us/library/dd144901(v=vs.85).aspx
// callback function called by EnumDisplayMonitors for each enabled monitor
static BOOL CALLBACK monitor_enum_proc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
	int width  = lprcMonitor->right - lprcMonitor->left;
	int height = lprcMonitor->bottom - lprcMonitor->top;
	int origin_x = lprcMonitor->left;
	int origin_y = lprcMonitor->top;
	
	if (width > 0 && height > 0) {
		uint8_t *screen_count = (uint8_t *) dwData;
		
		// FIXME Figure out memory management strategy.
		if (screens == NULL) {
			screens = malloc(sizeof(screen_data));				
		}
		else {
			screens = realloc(screens, sizeof(screen_data) * (*(uint8_t *) dwData));
		}

		screens[(*screen_count)++] = (screen_data) {
				// Should monitor count start @ zero? Currently it starts at 1.
				.number = *screen_count,
				.x = origin_x,
				.y = origin_y,
				.width = width,
				.height = height
			};

			logger(LOG_LEVEL_INFO,	"%s [%u]: Monitor %d: %ldx%ld (%ld, %ld)\n",
					__FUNCTION__, __LINE__, *screen_count, width, height, origin_x, origin_y);
	}
	
	return TRUE;
}

/* The following function was contributed by Anthony Liguori Jan 14, 2015.
 * https://github.com/kwhat/libuiohook/pull/17
 */
UIOHOOK_API screen_data* hook_create_screen_info(uint8_t *count) {
	// Initialize count to zero.
	*count = 0;
	
	// TODO: probably should check whether screens is NULL, and free otherwise
	// or who will take responsibility to free that?
	// screen_data *screens = NULL;
	BOOL status = EnumDisplayMonitors(NULL, NULL, monitor_enum_proc, (LPARAM) count);
	
	if (!status) {
		// Fallback in case EnumDisplayMonitors fails.
		logger(LOG_LEVEL_INFO,	"%s [%u]: EnumDisplayMonitors failed. Fallback.\n",
				__FUNCTION__, __LINE__);

		int width  = GetSystemMetrics(SM_CXSCREEN);
		int height = GetSystemMetrics(SM_CYSCREEN);

		if (width > 0 && height > 0) {
			screens = malloc(sizeof(screen_data));

			if (screens != NULL) {
				*count = 1;
				screens[0] = (screen_data) {
					.number = 1,
					.x = 0,
					.y = 0,
					.width = width,
					.height = height
				};
			}
		}
	}

	return screens;
}

UIOHOOK_API long int hook_get_auto_repeat_rate() {
	long int value = -1;
	long int rate;

	if (SystemParametersInfo(SPI_GETKEYBOARDSPEED, 0, &rate, 0)) {
		logger(LOG_LEVEL_INFO,	"%s [%u]: SPI_GETKEYBOARDSPEED: %li.\n",
			__FUNCTION__, __LINE__, rate);

		value = rate;
	}

	return value;
}

UIOHOOK_API long int hook_get_auto_repeat_delay() {
	long int value = -1;
	long int delay;

	if (SystemParametersInfo(SPI_GETKEYBOARDDELAY, 0, &delay, 0)) {
		logger(LOG_LEVEL_INFO,	"%s [%u]: SPI_GETKEYBOARDDELAY: %li.\n",
			__FUNCTION__, __LINE__, delay);

		value = delay;
	}

	return value;
}

UIOHOOK_API long int hook_get_pointer_acceleration_multiplier() {
	long int value = -1;
	int mouse[3]; // 0-Threshold X, 1-Threshold Y and 2-Speed.

	if (SystemParametersInfo(SPI_GETMOUSE, 0, &mouse, 0)) {
		logger(LOG_LEVEL_INFO,	"%s [%u]: SPI_GETMOUSE[2]: %i.\n",
			__FUNCTION__, __LINE__, mouse[2]);

		value = mouse[2];
	}

	return value;
}

UIOHOOK_API long int hook_get_pointer_acceleration_threshold() {
	long int value = -1;
	int mouse[3]; // 0-Threshold X, 1-Threshold Y and 2-Speed.

	if (SystemParametersInfo(SPI_GETMOUSE, 0, &mouse, 0)) {
		logger(LOG_LEVEL_INFO,	"%s [%u]: SPI_GETMOUSE[0]: %i.\n",
			__FUNCTION__, __LINE__, mouse[0]);
		logger(LOG_LEVEL_INFO,	"%s [%u]: SPI_GETMOUSE[1]: %i.\n",
			__FUNCTION__, __LINE__, mouse[1]);

		// Average the x and y thresholds.
		value = (mouse[0] + mouse[1]) / 2;
	}

	return value;
}

UIOHOOK_API long int hook_get_pointer_sensitivity() {
	long int value = -1;
	int sensitivity;

	if (SystemParametersInfo(SPI_GETMOUSESPEED, 0, &sensitivity, 0)) {
		logger(LOG_LEVEL_INFO,	"%s [%u]: SPI_GETMOUSESPEED: %i.\n",
			__FUNCTION__, __LINE__, sensitivity);

		value = sensitivity;
	}

	return value;
}

UIOHOOK_API long int hook_get_multi_click_time() {
	long int value = -1;
	UINT clicktime;

	clicktime = GetDoubleClickTime();
	logger(LOG_LEVEL_INFO,	"%s [%u]: GetDoubleClickTime: %u.\n",
			__FUNCTION__, __LINE__, (unsigned int) clicktime);

	value = (long int) clicktime;

	return value;
}

// DLL Entry point.
BOOL WINAPI DllMain(HINSTANCE hInstDLL, DWORD fdwReason, LPVOID lpReserved) {
	switch (fdwReason) {
		case DLL_PROCESS_ATTACH:
			// Display the copyright on library load.
			COPYRIGHT();

			// Save the DLL address.
			hInst = hInstDLL;

			// Initialize native input helper functions.
			load_input_helper();
			break;

		case DLL_PROCESS_DETACH:
			// Deinitialize native input helper functions.
			unload_input_helper();
			break;

		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
			// Do Nothing.
			break;
	}

	return TRUE;
}
