/* libUIOHook: Cross-platfrom userland keyboard and mouse hooking.
 * Copyright (C) 2006-2014 Alexander Barker.  All Rights Received.
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
#include <uiohook.h>

#ifdef _WIN32
#include <windows.h>

static HANDLE hook_control_handle = NULL;
#else
#if defined(__APPLE__) && defined(__MACH__)
#include <CoreFoundation/CoreFoundation.h>
#else
#include <pthread.h>

static pthread_cond_t hook_control_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t hook_control_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif
#endif

#include "logger.h"

// Program's is running sentinel.
static bool running = false;

// NOTE: This function executes on the hook thread!  If you need to block
// please do so on another thread with your own event dispatcher implementation.
void dispatch_proc(virtual_event * const event) {
	#if defined(_WIN32) && !defined(_WIN64)
	logger(LOG_LEVEL_INFO, "id=%i,when=%I64u,mask=0x%X",
			event->type, event->time, event->mask);
	#else
	logger(LOG_LEVEL_INFO, "id=%i,when=%llu,mask=0x%X",
			event->type, event->time, event->mask);
	#endif

	switch (event->type) {
		case EVENT_KEY_PRESSED:
			// If the escape key is pressed, naturally terminate the program.
			if (event->data.keyboard.keycode == VC_ESCAPE) {
				#ifdef _WIN32
				running = false;
				SetEvent(hook_control_handle);
				#else
				#if defined(__APPLE__) && defined(__MACH__)
				running = false;
				CFRunLoopStop(CFRunLoopGetMain());
				#else
				pthread_mutex_lock(&hook_control_mutex);
				running = false;
				pthread_cond_signal(&hook_control_cond);
				pthread_mutex_unlock(&hook_control_mutex);
				#endif
				#endif
			}
		case EVENT_KEY_RELEASED:
			logger(LOG_LEVEL_INFO, ",keycode=%u,rawcode=0x%X",
					event->data.keyboard.keycode, event->data.keyboard.rawcode);
			break;

		case EVENT_KEY_TYPED:
			logger(LOG_LEVEL_INFO, ",keychar=%lc,rawcode=%u",
					(wint_t) event->data.keyboard.keychar, event->data.keyboard.rawcode);
			break;

		case EVENT_MOUSE_PRESSED:
		case EVENT_MOUSE_RELEASED:
		case EVENT_MOUSE_CLICKED:
		case EVENT_MOUSE_MOVED:
		case EVENT_MOUSE_DRAGGED:
			logger(LOG_LEVEL_INFO, ",x=%i,y=%i,button=%i,clicks=%i",
					event->data.mouse.x, event->data.mouse.y,
					event->data.mouse.button, event->data.mouse.clicks);
			break;

		case EVENT_MOUSE_WHEEL:
			logger(LOG_LEVEL_INFO, ",type=%i,amount=%i,rotation=%i",
							event->data.wheel.type, event->data.wheel.amount,
							event->data.wheel.rotation);
			break;
	}

	logger(LOG_LEVEL_INFO, "\n");
}

int main() {
	hook_set_dispatch_proc(&dispatch_proc);

	#ifdef _WIN32
	hook_control_handle = CreateEvent(NULL, TRUE, FALSE, TEXT("hook_control_handle"));
	#endif

	int status = hook_enable();
	running = (status == UIOHOOK_SUCCESS);
	while (running) {
		#ifdef _WIN32
		WaitForSingleObject(hook_control_handle, INFINITE);
		#else
		#if defined(__APPLE__) && defined(__MACH__)
		//CFRunLoopRun();
		SInt32 result = 0;
		do {
			result = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true);
		} while (running && result != kCFRunLoopRunStopped);
		#else
		pthread_mutex_lock(&hook_control_mutex);
		pthread_cond_wait(&hook_control_cond, &hook_control_mutex);
		pthread_mutex_unlock(&hook_control_mutex);
		#endif
		#endif
	}

	if (hook_is_enabled()) {
		hook_disable();
	}

	#ifdef _WIN32
	CloseHandle(hook_control_handle);
	#endif

	return status;
}
