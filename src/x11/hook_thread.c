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
#include "input_helper.h"
#include "logger.h"

typedef struct _hook_data {
	Display *display;
	XRecordRange *range;
} hook_data;

// The pointer to the X11 display accessed by the callback.
static Display *ctrl_display;
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


static void hook_cleanup_proc(void *arg) {
	// Make sure the control mutex is locked because the hook_event_proc
	// may not get called with XRecordEndOfData if the thread is canceled.
	if (pthread_mutex_trylock(&hook_control_mutex) == 0) {
		logger(LOG_LEVEL_WARN,	"%s [%u]: Improper hook shutdown detected!\n",
				__FUNCTION__, __LINE__);
	}

	hook_data *data = (hook_data *) arg;

	// Free the XRecord range if it was set.
	if (data->range != NULL) {
		XFree(data->range);
	}

	if (data->display != NULL) {
		// Free up the context if it was set.
		if (context != 0) {
			XRecordFreeContext(data->display, context);
			context = 0;
		}

		// Close down the XRecord data display.
		XCloseDisplay(data->display);
	}

	// Cleanup native input functions.
	unload_input_helper();

	// Close down any open displays.
	if (ctrl_display != NULL) {
		XCloseDisplay(ctrl_display);
		ctrl_display = NULL;
	}

	// Cleanup the structure.
	free(arg);

	// Make sure we signal that the thread has terminated.
	pthread_mutex_unlock(&hook_running_mutex);

	// Make sure we signal that we have passed any exception throwing code for
	// the waiting hook_enable().
	pthread_cond_signal(&hook_control_cond);
	pthread_mutex_unlock(&hook_control_mutex);
}

static void *hook_thread_proc(void *arg) {
	// Lock the thread control mutex.  This will be unlocked when the
	// thread has finished starting, or when it has terminated due to error.
	// This is unlocked in the hook_callback.c hook_event_proc().
	pthread_mutex_lock(&hook_control_mutex);

	// Hook data for future cleanup.
	hook_data *data = malloc(sizeof(hook_data));
	pthread_cleanup_push(hook_cleanup_proc, data);

	// Cast for convenience and initialize.
	int *status = (int *) arg;
	*status = UIOHOOK_FAILURE;

	// Set the context to an invalid value (zero).
	context = 0;

	// Try and open a data display for XRecord.
	// NOTE This display must be opened on the same thread as XRecord.
	data->display = XOpenDisplay(NULL);
	if (data->display != NULL) {
		logger(LOG_LEVEL_DEBUG,	"%s [%u]: XOpenDisplay successful.\n",
				__FUNCTION__, __LINE__);

		// Make sure the data display is synchronized to prevent late event delivery!
		// See Bug 42356 for more information.
		// https://bugs.freedesktop.org/show_bug.cgi?id=42356#c4
		XSynchronize(data->display, True);

		// Setup XRecord range.
		XRecordClientSpec clients = XRecordAllClients;
		data->range = XRecordAllocRange();
		if (data->range != NULL) {
			logger(LOG_LEVEL_DEBUG,	"%s [%u]: XRecordAllocRange successful.\n",
					__FUNCTION__, __LINE__);

			// Create XRecord Context.
			data->range->device_events.first = KeyPress;
			data->range->device_events.last = MotionNotify;

			/* Note that the documentation for this function is incorrect,
			 * disp_data should be used!
			 * See: http://www.x.org/releases/X11R7.6/doc/libXtst/recordlib.txt
			 */
			context = XRecordCreateContext(data->display, XRecordFromServerTime, &clients, 1, &(data->range), 1);
			if (context != 0) {
				logger(LOG_LEVEL_DEBUG,	"%s [%u]: XRecordCreateContext successful.\n",
						__FUNCTION__, __LINE__);

				// Initialize native input helper functions.
				load_input_helper(ctrl_display);

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
				if (XRecordEnableContext(data->display, context, hook_event_proc, NULL) != 0) {
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
			}
			else {
				logger(LOG_LEVEL_ERROR,	"%s [%u]: XRecordCreateContext failure!\n",
						__FUNCTION__, __LINE__);

				// Set the exit status.
				*status = UIOHOOK_ERROR_X_RECORD_CREATE_CONTEXT;
			}
		}
		else {
			logger(LOG_LEVEL_ERROR,	"%s [%u]: XRecordAllocRange failure!\n",
					__FUNCTION__, __LINE__);

			// Set the exit status.
			*status = UIOHOOK_ERROR_X_RECORD_ALLOC_RANGE;
		}
	}
	else {
		logger(LOG_LEVEL_ERROR,	"%s [%u]: XOpenDisplay failure!\n",
				__FUNCTION__, __LINE__);

		// Set the exit status.
		*status = UIOHOOK_ERROR_X_OPEN_DISPLAY;
	}

	// Execute the thread cleanup handler.
	pthread_cleanup_pop(1);

	logger(LOG_LEVEL_DEBUG,	"%s [%u]: Something, something, something, complete.\n",
			__FUNCTION__, __LINE__);

	return arg;
}

UIOHOOK_API int hook_enable() {
	int status = UIOHOOK_FAILURE;

	// Lock the thread control mutex.  This will be unlocked when the
	// thread has finished starting, or when it has fully stopped.
	pthread_mutex_lock(&hook_control_mutex);

	// Make sure the native thread is not already running.
	if (hook_is_enabled() != true) {
		// Open the control and data displays.
		if (ctrl_display == NULL) {
			ctrl_display = XOpenDisplay(NULL);
		}

		if (ctrl_display != NULL) {
			// Attempt to setup detectable autorepeat.
			// NOTE: is_auto_repeat is NOT stdbool!
			Bool is_auto_repeat = False;
			#ifdef USE_XKB
			// Enable detectable autorepeat.
			XkbSetDetectableAutoRepeat(ctrl_display, True, &is_auto_repeat);
			#else
			XAutoRepeatOn(ctrl_display);

			XKeyboardState kb_state;
			XGetKeyboardControl(ctrl_display, &kb_state);

			is_auto_repeat = (kb_state.global_auto_repeat == AutoRepeatModeOn);
			#endif

			if (is_auto_repeat == False) {
				logger(LOG_LEVEL_WARN,	"%s [%u]: Could not enable detectable auto-repeat!\n",
						__FUNCTION__, __LINE__);
			}
			else {
				logger(LOG_LEVEL_DEBUG,	"%s [%u]: Successfully enabled detectable autorepeat.\n",
						__FUNCTION__, __LINE__);
			}

			// Check to make sure XRecord is installed and enabled.
			int major, minor;
			if (XRecordQueryVersion(ctrl_display, &major, &minor) != 0) {
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
						int *hook_thread_status;
						pthread_join(hook_thread_id, (void **) &hook_thread_status);
						status = *hook_thread_status;

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
			if (ctrl_display != NULL) {
				XCloseDisplay(ctrl_display);
				ctrl_display = NULL;
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
		// We need to make sure the context is still valid.
		XRecordState *state = malloc(sizeof(XRecordState));
		if (XRecordGetContext(ctrl_display, context, &state) != 0) {
			// Try to exit the thread naturally.
			if (state->enabled && XRecordDisableContext(ctrl_display, context) != 0) {
				#ifdef USE_XRECORD_ASYNC
				pthread_mutex_lock(&hook_xrecord_mutex);
				running = false;
				pthread_cond_signal(&hook_xrecord_cond);
				pthread_mutex_unlock(&hook_xrecord_mutex);
				#endif

				// See Bug 42356 for more information.
				// https://bugs.freedesktop.org/show_bug.cgi?id=42356#c4
				XFlush(ctrl_display);
				//XSync(ctrl_display, True);

				// If we want method to behave synchronically, we must wait
				// for the thread to die.
				// NOTE This will prevent function calls from the callback!
				// pthread_cond_wait(&hook_control_cond, &hook_control_mutex);

				status = UIOHOOK_SUCCESS;
			}
		}

		free(state);
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
