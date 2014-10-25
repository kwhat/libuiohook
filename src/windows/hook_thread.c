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

#include <uiohook.h>
#include <windows.h>

#include "hook_callback.h"
#include "input_helper.h"
#include "logger.h"

// The handle to the DLL module pulled in DllMain on DLL_PROCESS_ATTACH.
extern HINSTANCE hInst;

// Thread and hook handles.
static DWORD hook_thread_id = 0;
static HANDLE hook_thread_handle = NULL;

static HANDLE hook_running_mutex = NULL;
static HANDLE hook_control_mutex = NULL;

HHOOK keyboard_event_hhook = NULL, mouse_event_hhook = NULL;


static DWORD WINAPI hook_thread_proc(LPVOID lpParameter) {
	DWORD status = UIOHOOK_FAILURE;

	// Spot check the hInst incase the library was statically linked and DllMain
	// did not receieve a pointer on load.
	if (hInst == NULL) {
		logger(LOG_LEVEL_WARN,	"%s [%u]: hInst was not set by DllMain()!\n",
				__FUNCTION__, __LINE__);

		hInst = GetModuleHandle(NULL);

		if (hInst == NULL) {
			logger(LOG_LEVEL_ERROR,	"%s [%u]: Could not determine hInst for SetWindowsHookEx()! (%#lX)\n",
					__FUNCTION__, __LINE__, (unsigned long) GetLastError());
		}
	}

	// Create the native hooks.
	keyboard_event_hhook = SetWindowsHookEx(WH_KEYBOARD_LL, hook_event_proc, hInst, 0);
	mouse_event_hhook = SetWindowsHookEx(WH_MOUSE_LL, hook_event_proc, hInst, 0);

	// If we did not encounter a problem, start processing events.
	if (keyboard_event_hhook != NULL && mouse_event_hhook != NULL) {
		logger(LOG_LEVEL_DEBUG,	"%s [%u]: SetWindowsHookEx() successful.\n",
				__FUNCTION__, __LINE__);

		// Initialize native input helper functions.
		load_input_helper();

		// Check and setup modifiers.
		initialize_modifiers();

		// Set the exit status.
		status = UIOHOOK_SUCCESS;

		// Windows does not have a hook start event or callback so we need to
		// manually fake it.
		hook_start_proc();

		// Signal that we have passed the thread initialization.
		SetEvent(hook_running_mutex);
		SetEvent(hook_control_mutex);

		// Block until the thread receives an WM_QUIT request.
		MSG message;
		while (GetMessage(&message, (HWND) -1, 0, 0) > 0) {
			TranslateMessage(&message);
			DispatchMessage(&message);
		}

		// Deinitialize native input helper functions.
		unload_input_helper();
	}
	else {
		logger(LOG_LEVEL_ERROR,	"%s [%u]: SetWindowsHookEx() failed! (%#lX)\n",
				__FUNCTION__, __LINE__, (unsigned long) GetLastError());

		status = UIOHOOK_ERROR_SET_WINDOWS_HOOK_EX;
	}

	// Destroy the native hooks.
	if (keyboard_event_hhook != NULL) {
		UnhookWindowsHookEx(keyboard_event_hhook);
		keyboard_event_hhook = NULL;
	}

	if (mouse_event_hhook != NULL) {
		UnhookWindowsHookEx(mouse_event_hhook);
		mouse_event_hhook = NULL;
	}

	// We must explicitly call the cleanup handler because Windows does not
	// provide a thread cleanup method like POSIX pthread_cleanup_push/pop.
	hook_stop_proc();

	// Close any handle that is still open.
	if (hook_thread_handle != NULL) {
		CloseHandle(hook_thread_handle);
		hook_thread_handle = NULL;
	}

	if (hook_control_mutex != NULL) {
		CloseHandle(hook_control_mutex);
		hook_control_mutex = NULL;
	}

	if (hook_running_mutex != NULL) {
		CloseHandle(hook_running_mutex);
		hook_running_mutex = NULL;
	}

	logger(LOG_LEVEL_DEBUG,	"%s [%u]: Something, something, something, complete.\n",
			__FUNCTION__, __LINE__);

	return status;
}

UIOHOOK_API int hook_enable() {
	int status = UIOHOOK_FAILURE;

	// Make sure the native thread is not already running.
	if (hook_is_enabled() != true) {
		// Create event handles for the thread hook.
		hook_running_mutex = CreateEvent(NULL, TRUE, FALSE, TEXT("hook_running_mutex"));
		hook_control_mutex = CreateEvent(NULL, TRUE, FALSE, TEXT("hook_control_mutex"));

		LPTHREAD_START_ROUTINE lpStartAddress = &hook_thread_proc;
		hook_thread_handle = CreateThread(NULL, 0, lpStartAddress, NULL, 0, &hook_thread_id);
		if (hook_thread_handle != INVALID_HANDLE_VALUE) {
			logger(LOG_LEVEL_DEBUG,	"%s [%u]: Start successful\n",
							__FUNCTION__, __LINE__);

			// Attempt to set the thread priority to time critical.
			// TODO This maybe a little overkill, re-evaluate.
			if (SetThreadPriority(hook_thread_handle, THREAD_PRIORITY_TIME_CRITICAL) == 0) {
				logger(LOG_LEVEL_WARN, "%s [%u]: Could not set thread priority %li for thread %#p! (%#lX)\n",
						__FUNCTION__, __LINE__, (long) THREAD_PRIORITY_TIME_CRITICAL,
						hook_thread_handle, (unsigned long) GetLastError());
			}

			// Wait for any possible thread exceptions to get thrown into
			// the queue
			WaitForSingleObject(hook_control_mutex, INFINITE);

			// TODO Set the return status to the thread exit code.
			if (hook_is_enabled()) {
				logger(LOG_LEVEL_DEBUG,	"%s [%u]: Start successful\n",
						__FUNCTION__, __LINE__);

				status = UIOHOOK_SUCCESS;
			}
			else {
				logger(LOG_LEVEL_ERROR,	"%s [%u]: Initialization failure!\n",
						__FUNCTION__, __LINE__);

				// Wait for the thread to die.
				WaitForSingleObject(hook_thread_handle,  INFINITE);

				DWORD thread_status;
				GetExitCodeThread(hook_thread_handle, &thread_status);
				status = (int) thread_status;

				logger(LOG_LEVEL_ERROR,	"%s [%u]: Thread Result: %#X!\n",
						__FUNCTION__, __LINE__, status);
			}
		}
		else {
			logger(LOG_LEVEL_ERROR,	"%s [%u]: Thread create failure!\n",
					__FUNCTION__, __LINE__);

			status = UIOHOOK_ERROR_THREAD_CREATE;
		}
	}

	return status;
}

UIOHOOK_API int hook_disable() {
	int status = UIOHOOK_FAILURE;

	if (hook_is_enabled() == true) {
		// Try to exit the thread naturally.
		PostThreadMessage(hook_thread_id, WM_QUIT, (WPARAM) NULL, (LPARAM) NULL);

		// If we want method to behave synchronically, we must wait
		// for the thread to die.
		// NOTE This will prevent function calls from the callback!
		// WaitForSingleObject(hook_thread_handle,  INFINITE);

		status = UIOHOOK_SUCCESS;
	}

	logger(LOG_LEVEL_DEBUG,	"%s [%u]: Status: %#X.\n",
			__FUNCTION__, __LINE__, status);

	return status;
}

UIOHOOK_API bool hook_is_enabled() {
	bool is_running = false;

	if (hook_running_mutex != NULL) {
		DWORD status = WaitForSingleObject(hook_running_mutex, 0);
		switch (status)	{
			case WAIT_OBJECT_0:
				logger(LOG_LEVEL_DEBUG,
						"%s [%u]: Running event signaled.\n",
						__FUNCTION__, __LINE__);

				GetExitCodeThread(hook_thread_handle, &status);
				if (status == STILL_ACTIVE) {
					is_running = true;
				}
				else {
					logger(LOG_LEVEL_WARN,	"%s [%u]: Late thread cleanup detected!\n",
							__FUNCTION__, __LINE__);

					// NOTE Windows does not provide reliable thread cleanup!
					// Instead we rely on auto-magic cleanup from the OS for
					// thread resources and we need to detect and cleanup the
					// leftovers so we can restart the hook in the event a
					// developer calls ExitThread or TerminateThread from the
					// uiohook event dispatch callback.
					if (hook_thread_handle != NULL) {
						CloseHandle(hook_thread_handle);
						hook_thread_handle = NULL;
					}

					if (hook_control_mutex != NULL) {
						CloseHandle(hook_control_mutex);
						hook_control_mutex = NULL;
					}

					if (hook_running_mutex != NULL) {
						CloseHandle(hook_running_mutex);
						hook_running_mutex = NULL;
					}
				}

				break;

			case WAIT_TIMEOUT:
				logger(LOG_LEVEL_DEBUG,
						"%s [%u]: Running event not signaled yet...\n",
						__FUNCTION__, __LINE__);
				break;

			case WAIT_ABANDONED:
				logger(LOG_LEVEL_WARN,
						"%s [%u]: Running event abandoned and reclaimed!\n",
						__FUNCTION__, __LINE__);
				break;

			case WAIT_FAILED:
				logger(LOG_LEVEL_ERROR,
						"%s [%u]: Failed to wait for running event! (%#lX)\n",
						__FUNCTION__, __LINE__,
						(unsigned long) GetLastError());
				break;
		}
	}

	logger(LOG_LEVEL_DEBUG,	"%s [%u]: State: %i.\n",
			__FUNCTION__, __LINE__, is_running);

	return is_running;
}


static HANDLE control_handle = NULL;

UIOHOOK_API void hook_wait(){
	control_handle = CreateEvent(NULL, TRUE, FALSE, TEXT("control_handle"));
	WaitForSingleObject(control_handle, INFINITE);
}

UIOHOOK_API void hook_continue(){
	SetEvent(control_handle);
	CloseHandle(control_handle);
}