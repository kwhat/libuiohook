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

#include <pthread.h>
#include <stdlib.h>
#ifdef USE_XRECORD_ASYNC
#include <sys/time.h>
#endif
#include <uiohook.h>
#ifdef USE_XKB
#include <X11/XKBlib.h>
#endif
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/record.h>

#include "hook_callback.h"
#include "logger.h"
#include "input_helper.h"

// The pointer to the X11 display accessed by the callback.
static Display *disp_ctrl;
static XRecordContext context;

// Thread and hook handles.
#ifdef USE_XRECORD_ASYNC
static bool running;

static pthread_cond_t hook_xrecord_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t hook_xrecord_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

pthread_mutex_t hook_running_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t hook_control_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t hook_control_cond = PTHREAD_COND_INITIALIZER;

static void *hook_thread_proc(void *arg) {
	// Lock the thread control mutex.  This will be unlocked when the
	// thread has finished starting, or when it has terminated due to error.
	// This is unlocked in the hook_callback.c hook_event_proc().
	pthread_mutex_lock(&hook_control_mutex);

	int *status = (int *) arg;
	*status = UIOHOOK_FAILURE;

	// XRecord context for use later.
	context = 0;

	Display *disp_data = XOpenDisplay(NULL);
	if (disp_data != NULL) {
		logger(LOG_LEVEL_DEBUG,	"%s [%u]: XOpenDisplay successful.\n",
				__FUNCTION__, __LINE__);

		// Make sure the data display is synchronized to prevent late event delivery!
		// See Bug 42356 for more information.
		// https://bugs.freedesktop.org/show_bug.cgi?id=42356#c4
		XSynchronize(disp_data, True);

		// Setup XRecord range.
		XRecordClientSpec clients = XRecordAllClients;
		XRecordRange *range = XRecordAllocRange();
		if (range != NULL) {
			logger(LOG_LEVEL_DEBUG,	"%s [%u]: XRecordAllocRange successful.\n",
					__FUNCTION__, __LINE__);

			// Create XRecord Context.
			range->device_events.first = KeyPress;
			range->device_events.last = MotionNotify;

			/* Note that the documentation for this function is incorrect,
			 * disp_data should be used!
			 * See: http://www.x.org/releases/X11R7.6/doc/libXtst/recordlib.txt
			 */
			context = XRecordCreateContext(disp_data, 0, &clients, 1, &range, 1);
			if (context != 0) {
				logger(LOG_LEVEL_DEBUG,	"%s [%u]: XRecordCreateContext successful.\n",
						__FUNCTION__, __LINE__);

				// Initialize Native Input Functions.
				load_input_helper(disp_ctrl);

				#ifdef USE_XRECORD_ASYNC
				// Async requires that we loop so that our thread does not return.
				if (XRecordEnableContextAsync(disp_data, context, hook_event_proc, NULL) != 0) {
					// Time in MS to sleep the runloop.
					int timesleep = 100;

					// Allow the thread loop to block.
					pthread_mutex_lock(&hook_xrecord_mutex);
					running = true;

					do {
						// Unlock the mutex from the previous iteration.
						pthread_mutex_unlock(&hook_xrecord_mutex);

						XRecordProcessReplies(disp_data);

						// Prevent 100% CPU utilization.
						struct timeval tv;
						gettimeofday(&tv, NULL);

						struct timespec ts;
						ts.tv_sec = time(NULL) + timesleep / 1000;
						ts.tv_nsec = tv.tv_usec * 1000 + 1000 * 1000 * (timesleep % 1000);
						ts.tv_sec += ts.tv_nsec / (1000 * 1000 * 1000);
						ts.tv_nsec %= (1000 * 1000 * 1000);

						pthread_mutex_lock(&hook_xrecord_mutex);
						pthread_cond_timedwait(&hook_xrecord_cond, &hook_xrecord_mutex, &ts);
					} while (running);

					// Unlock after loop exit.
					pthread_mutex_unlock(&hook_xrecord_mutex);

					// Set the exit status.
					status = NULL;
				}
				#else
				// Sync blocks until XRecordDisableContext() is called.
				if (XRecordEnableContext(disp_data, context, hook_event_proc, NULL) != 0) {
					// Set the exit status.
					status = NULL;
				}
				#endif
				else {
					logger(LOG_LEVEL_ERROR,	"%s [%u]: XRecordEnableContext failure!\n",
						__FUNCTION__, __LINE__);

					#ifdef USE_XRECORD_ASYNC
					// Reset the running state.
					pthread_mutex_lock(&hook_xrecord_mutex);
					running = false;
					pthread_mutex_unlock(&hook_xrecord_mutex);
					#endif

					// Set the exit status.
					*status = UIOHOOK_ERROR_X_RECORD_ENABLE_CONTEXT;
				}

				// Free up the context after the run loop terminates.
				XRecordFreeContext(disp_data, context);
			}
			else {
				logger(LOG_LEVEL_ERROR,	"%s [%u]: XRecordCreateContext failure!\n",
						__FUNCTION__, __LINE__);

				// Set the exit status.
				*status = UIOHOOK_ERROR_X_RECORD_CREATE_CONTEXT;
			}

			// Free the XRecordRange.
			XFree(range);
		}
		else {
			logger(LOG_LEVEL_ERROR,	"%s [%u]: XRecordAllocRange failure!\n",
					__FUNCTION__, __LINE__);

			// Set the exit status.
			*status = UIOHOOK_ERROR_X_RECORD_ALLOC_RANGE;
		}

		XCloseDisplay(disp_data);
		disp_data = NULL;
	}
	else {
		logger(LOG_LEVEL_ERROR,	"%s [%u]: XOpenDisplay failure!\n",
				__FUNCTION__, __LINE__);

		// Set the exit status.
		*status = UIOHOOK_ERROR_X_OPEN_DISPLAY;
	}

	logger(LOG_LEVEL_DEBUG,	"%s [%u]: Something, something, something, complete.\n",
			__FUNCTION__, __LINE__);

	// Make sure we signal that we have passed any exception throwing code.
	pthread_cond_signal(&hook_control_cond);
	pthread_mutex_unlock(&hook_control_mutex);

	return status;
}

UIOHOOK_API int hook_enable() {
	int status = UIOHOOK_FAILURE;

	// Lock the thread control mutex.  This will be unlocked when the
	// thread has finished starting, or when it has fully stopped.
	pthread_mutex_lock(&hook_control_mutex);

	// Make sure the native thread is not already running.
	if (hook_is_enabled() != true) {
		// Open the control and data displays.
		if (disp_ctrl == NULL) {
			disp_ctrl = XOpenDisplay(NULL);
		}

		if (disp_ctrl != NULL) {
			// Attempt to setup detectable autorepeat.
			// NOTE: is_auto_repeat is NOT stdbool!
			Bool is_auto_repeat = False;
			#ifdef USE_XKB
			// Enable detectable autorepeat.
			XkbSetDetectableAutoRepeat(disp_ctrl, True, &is_auto_repeat);
			#else
			XAutoRepeatOn(disp_ctrl);

			XKeyboardState kb_state;
			XGetKeyboardControl(disp_ctrl, &kb_state);

			is_auto_repeat = (kb_state.global_auto_repeat == AutoRepeatModeOn);
			#endif

			if (is_auto_repeat == False) {
				logger(LOG_LEVEL_WARN,	"%s [%u]: %s\n",
						__FUNCTION__, __LINE__, "Could not enable detectable auto-repeat!\n");
			}
			else {
				logger(LOG_LEVEL_DEBUG,	"%s [%u]: Successfully enabled detectable autorepeat.\n",
						__FUNCTION__, __LINE__);
			}

			// Check to make sure XRecord is installed and enabled.
			int major, minor;
			if (XRecordQueryVersion(disp_ctrl, &major, &minor) != 0) {
				logger(LOG_LEVEL_INFO,	"%s [%u]: XRecord version: %i.%i.\n",
						__FUNCTION__, __LINE__, major, minor);

				// Create the thread attribute.
				pthread_attr_t hook_thread_attr;
				pthread_attr_init(&hook_thread_attr);

				// Get the policy and priority for the thread attr.
				int policy = 0;
				pthread_attr_getschedpolicy(&hook_thread_attr, &policy);
				int priority = sched_get_priority_max(policy);

				pthread_t hook_thread_id;
				void *hook_thread_status = malloc(sizeof(int));
				if (pthread_create(&hook_thread_id, &hook_thread_attr, hook_thread_proc, hook_thread_status) == 0) {
					logger(LOG_LEVEL_DEBUG,	"%s [%u]: Start successful\n",
							__FUNCTION__, __LINE__);

					#if _POSIX_C_SOURCE >= 200112L
					// POSIX does not support pthread_setschedprio so we will use
					// pthread_setschedparam instead.
					struct sched_param param = { .sched_priority = priority };
					if (pthread_setschedparam(hook_thread_id, SCHED_OTHER, &param) != 0) {
						logger(LOG_LEVEL_WARN,	"%s [%u]: Could not set thread priority %i for thread 0x%lX!\n",
								__FUNCTION__, __LINE__, priority, (unsigned long) hook_thread_id);
					}
					#else
					// Raise the thread priority using glibc pthread_setschedprio.
					if (pthread_setschedprio(hook_thread_id, priority) != 0) {
						logger(LOG_LEVEL_WARN,	"%s [%u]: Could not set thread priority %i for thread 0x%lX!\n",
								__FUNCTION__, __LINE__, priority, (unsigned long) hook_thread_id);
					}
					#endif

					// Wait for the thread to indicate that it has passed
					// the initialization portion.
					pthread_cond_wait(&hook_control_cond, &hook_control_mutex);

					// Handle any possible JNI issue that may have occurred.
					if (hook_is_enabled()) {
						logger(LOG_LEVEL_DEBUG,	"%s [%u]: Initialization successful.\n",
								__FUNCTION__, __LINE__);

						status = UIOHOOK_SUCCESS;
					}
					else {
						logger(LOG_LEVEL_ERROR,	"%s [%u]: Initialization failure!\n",
								__FUNCTION__, __LINE__);

						// Wait for the thread to die.
						void *hook_thread_status;
						pthread_join(hook_thread_id, (void *) &hook_thread_status);
						status = *(int *) hook_thread_status;

						logger(LOG_LEVEL_ERROR,	"%s [%u]: Thread Result: (%#X)!\n",
								__FUNCTION__, __LINE__, status);
					}
				}
				else {
					logger(LOG_LEVEL_ERROR,	"%s [%u]: Thread create failure!\n",
							__FUNCTION__, __LINE__);

					status = UIOHOOK_ERROR_THREAD_CREATE;
				}

				// Make sure the thread attribute is removed.
				pthread_attr_destroy(&hook_thread_attr);

				// At this point we have either copied the error or success.
				free(hook_thread_status);
			}
			else {
				logger(LOG_LEVEL_ERROR,	"%s [%u]: XRecord is not currently available!\n",
							__FUNCTION__, __LINE__);

				status = UIOHOOK_ERROR_X_RECORD_NOT_FOUND;
			}
		}
		else {
			logger(LOG_LEVEL_ERROR,	"%s [%u]: XOpenDisplay failure!\n",
					__FUNCTION__, __LINE__);

			// Close down any open displays.
			if (disp_ctrl != NULL) {
				XCloseDisplay(disp_ctrl);
				disp_ctrl = NULL;
			}

			status = UIOHOOK_ERROR_X_OPEN_DISPLAY;
		}
	}

	// Make sure the control mutex is unlocked.
	pthread_mutex_unlock(&hook_control_mutex);

	logger(LOG_LEVEL_DEBUG,	"%s [%u]: Status: %#X.\n",
			__FUNCTION__, __LINE__, status);

	return status;
}

UIOHOOK_API int hook_disable() {
	int status = UIOHOOK_FAILURE;

	// Lock the thread control mutex.  This will be unlocked when the
	// thread has fully stopped.
	pthread_mutex_lock(&hook_control_mutex);

	if (hook_is_enabled() == true) {
		// Try to exit the thread naturally.
		if (XRecordDisableContext(disp_ctrl, context) != 0) {
			#ifdef USE_XRECORD_ASYNC
			pthread_mutex_lock(&hook_xrecord_mutex);
			running = false;
			pthread_cond_signal(&hook_xrecord_cond);
			pthread_mutex_unlock(&hook_xrecord_mutex);
			#endif

			// See Bug 42356 for more information.
			// https://bugs.freedesktop.org/show_bug.cgi?id=42356#c4
			XFlush(disp_ctrl);
			//XSync(disp_ctrl, True);

			// Wait for the thread to die.
			pthread_cond_wait(&hook_control_cond, &hook_control_mutex);

			// Close down any open displays.
			if (disp_ctrl != NULL) {
				XCloseDisplay(disp_ctrl);
				disp_ctrl = NULL;
			}

			status = UIOHOOK_SUCCESS;
		}
	}

	// Make sure the mutex gets unlocked.
	pthread_mutex_unlock(&hook_control_mutex);

	logger(LOG_LEVEL_DEBUG,	"%s [%u]: Status: %#X.\n",
			__FUNCTION__, __LINE__, status);

	return status;
}

UIOHOOK_API bool hook_is_enabled() {
	bool is_running = false;

	// Try to aquire a lock on the running mutex.
	if (pthread_mutex_trylock(&hook_running_mutex) == 0) {
		// Lock Successful, thread is not running.
		pthread_mutex_unlock(&hook_running_mutex);
	}
	else {
		is_running = true;
	}

	logger(LOG_LEVEL_DEBUG,	"%s [%u]: State (%i).\n",
			__FUNCTION__, __LINE__, is_running);

	return is_running;
}
