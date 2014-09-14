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

#include <ApplicationServices/ApplicationServices.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/time.h>
#include <uiohook.h>

#include "hook_callback.h"
#include "input_helper.h"
#include "logger.h"

typedef struct _hook_data {
	CFMachPortRef port;
	CFRunLoopSourceRef source;
	CFRunLoopObserverRef observer;
} hook_data;

// Thread and hook handles.
CFRunLoopRef event_loop;

pthread_mutex_t hook_running_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t hook_control_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t hook_control_cond = PTHREAD_COND_INITIALIZER;

// Flag to restart the event tap incase of timeout.
Boolean restart_tap = false;


static void hook_cleanup_proc(void *arg) {
	pthread_mutex_lock(&hook_control_mutex);
	hook_data *data = (hook_data *) arg;

	// Stop the CFMachPort from receiving any more messages.
	if (data->port != NULL) {
		CFMachPortInvalidate(data->port);
		CFRelease(data->port);
	}

	if (event_loop != NULL && data->observer != NULL) {
		// Lock back up until we are done processing the exit.
		if (CFRunLoopContainsObserver(event_loop, data->observer, kCFRunLoopDefaultMode)) {
			CFRunLoopRemoveObserver(event_loop, data->observer, kCFRunLoopDefaultMode);
		}

		if (CFRunLoopContainsSource(event_loop, data->source, kCFRunLoopDefaultMode)) {
			CFRunLoopRemoveSource(event_loop, data->source, kCFRunLoopDefaultMode);
		}
		
		CFRunLoopObserverInvalidate(data->observer);
	}
	
	// Clean up the event source.
	if (data->source) {
		CFRelease(data->source);
	}

	stop_message_port_runloop();
	
	// Free the data structure.
	free(arg);
}

static void hook_cancel_proc(void *arg) {
	// Make sure the control mutex is locked because the hook_cleanup_proc
	// may not get popped under some circumstances.
	pthread_mutex_trylock(&hook_control_mutex);
	
	// Make sure we signal that the thread has terminated.
	pthread_mutex_unlock(&hook_running_mutex);

	// Make sure we signal that we have passed any exception throwing code for
	// the waiting hook_enable().
	pthread_cond_signal(&hook_control_cond);
	pthread_mutex_unlock(&hook_control_mutex);
}

static void *hook_thread_proc(void *arg) {
	// Lock the thread control mutex.  This will be unlocked when the
	// thread has started the runloop, or when it has terminated due to error.
	// This is unlocked in the hook_callback.c hook_status_proc().
	pthread_mutex_lock(&hook_control_mutex);

	int *status = (int *) arg;
	*status = UIOHOOK_FAILURE;

	// Push hook cancel proc on the thread stack.
	pthread_cleanup_push(hook_cancel_proc, NULL);
	
	do {
		// Reset the restart flag...
		restart_tap = false;

		// Check for accessibility each time we start the loop.
		if (is_accessibility_enabled()) {
			// Push hook cleanup proc on the thread stack.
			hook_data *data = malloc(sizeof(hook_data));
			pthread_cleanup_push(hook_cleanup_proc, data);
			
			logger(LOG_LEVEL_DEBUG,	"%s [%u]: Accessibility API is enabled.\n",
					__FUNCTION__, __LINE__);


			// Setup the event mask to listen for.
			#ifdef USE_DEBUG
			CGEventMask event_mask =	kCGEventMaskForAllEvents;
			#else
			// This includes everything except:
			//	kCGEventNull
			//	kCGEventTapDisabledByTimeout
			//	kCGEventTapDisabledByTimeout
			CGEventMask event_mask =	CGEventMaskBit(kCGEventKeyDown) |
										CGEventMaskBit(kCGEventKeyUp) |
										CGEventMaskBit(kCGEventFlagsChanged) |

										CGEventMaskBit(kCGEventLeftMouseDown) |
										CGEventMaskBit(kCGEventLeftMouseUp) |
										CGEventMaskBit(kCGEventLeftMouseDragged) |

										CGEventMaskBit(kCGEventRightMouseDown) |
										CGEventMaskBit(kCGEventRightMouseUp) |
										CGEventMaskBit(kCGEventRightMouseDragged) |

										CGEventMaskBit(kCGEventOtherMouseDown) |
										CGEventMaskBit(kCGEventOtherMouseUp) |
										CGEventMaskBit(kCGEventOtherMouseDragged) |

										CGEventMaskBit(kCGEventMouseMoved) |
										CGEventMaskBit(kCGEventScrollWheel) |
										CGEventMaskBit(kCGEventTapDisabledByTimeout);
			#endif

			// Create the event tap.
			data->port = CGEventTapCreate(
											kCGSessionEventTap,			// kCGHIDEventTap
											kCGHeadInsertEventTap,		// kCGTailAppendEventTap
											kCGEventTapOptionDefault,	// kCGEventTapOptionListenOnly See Bug #22
											event_mask,
											hook_event_proc,
											NULL
										);


			if (data->port != NULL) {
				logger(LOG_LEVEL_DEBUG,	"%s [%u]: CGEventTapCreate Successful.\n",
						__FUNCTION__, __LINE__);

				// Create the runloop event source from the event tap.
				data->source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, data->port, 0);
				if (data->source != NULL) {
					logger(LOG_LEVEL_DEBUG,	"%s [%u]: CFMachPortCreateRunLoopSource successful.\n",
							__FUNCTION__, __LINE__);

					event_loop = CFRunLoopGetCurrent();
					if (event_loop != NULL) {
						logger(LOG_LEVEL_DEBUG,	"%s [%u]: CFRunLoopGetCurrent successful.\n",
								__FUNCTION__, __LINE__);

						// Initialize Native Input Functions.
						load_input_helper();

						// Create run loop observers.
						data->observer = CFRunLoopObserverCreate(
															kCFAllocatorDefault,
															kCFRunLoopEntry | kCFRunLoopExit, //kCFRunLoopAllActivities,
															true,
															0,
															hook_status_proc,
															NULL
														);

						if (data->observer != NULL) {
							// Set the exit status.
							*status = UIOHOOK_SUCCESS;

							start_message_port_runloop();

							// Add the event source and observer to the runloop mode.
							CFRunLoopAddSource(event_loop, data->source, kCFRunLoopDefaultMode);
							CFRunLoopAddObserver(event_loop, data->observer, kCFRunLoopDefaultMode);

							// Start the hook thread runloop.
							CFRunLoopRun();
						}
						else {
							// We cant do a whole lot of anything if we cant
							// create run loop observer.

							logger(LOG_LEVEL_ERROR,	"%s [%u]: CFRunLoopObserverCreate failure!\n",
									__FUNCTION__, __LINE__);

							// Set the exit status.
							*status = UIOHOOK_ERROR_OBSERVER_CREATE;
						}

						// Cleanup Native Input Functions.
						unload_input_helper();
					}
					else {
						logger(LOG_LEVEL_ERROR,	"%s [%u]: CFRunLoopGetCurrent failure!\n",
								__FUNCTION__, __LINE__);

						// Set the exit status.
						*status = UIOHOOK_ERROR_GET_RUNLOOP;
					}
				}
				else {
					logger(LOG_LEVEL_ERROR,	"%s [%u]: CFMachPortCreateRunLoopSource failure!\n",
						__FUNCTION__, __LINE__);

					// Set the exit status.
					*status = UIOHOOK_ERROR_CREATE_RUN_LOOP_SOURCE;
				}
			}
			else {
				logger(LOG_LEVEL_ERROR,	"%s [%u]: Failed to create event port!\n",
						__FUNCTION__, __LINE__);

				// Set the exit status.
				*status = UIOHOOK_ERROR_EVENT_PORT;
			}
			
			// Execute the thread cleanup handler.
			pthread_cleanup_pop(1);
		}
		else {
			logger(LOG_LEVEL_ERROR,	"%s [%u]: Accessibility API is disabled!\n",
					__FUNCTION__, __LINE__);

			// Set the exit status.
			*status = UIOHOOK_ERROR_AXAPI_DISABLED;
		}
	} while (restart_tap);

	// Execute the thread cancel handler.
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

			// OS X does not support glibc pthread_setschedprio so we will
			// always use pthread_setschedparam instead.
			struct sched_param param = { .sched_priority = priority };
			if (pthread_setschedparam(hook_thread_id, SCHED_OTHER, &param) != 0) {
				logger(LOG_LEVEL_WARN,	"%s [%u]: Could not set thread priority %i for thread 0x%lX!\n",
						__FUNCTION__, __LINE__, priority, (unsigned long) hook_thread_id);
			}

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

	// Make sure the control mutex is unlocked.
	pthread_mutex_unlock(&hook_control_mutex);

	return status;
}

UIOHOOK_API int hook_disable() {
	int status = UIOHOOK_FAILURE;

	// Lock the thread control mutex.  This will be unlocked when the
	// thread has fully stopped.
	pthread_mutex_lock(&hook_control_mutex);

	if (hook_is_enabled() == true) {
		// Make sure the tap doesn't restart.
		restart_tap = false;

		// Stop the run loop.
		CFRunLoopStop(event_loop);

		// If we want method to behave synchronically, we must wait
		// for the thread to die.
		// NOTE This will prevent function calls from the callback!
		// pthread_cond_wait(&hook_control_cond, &hook_control_mutex);

		status = UIOHOOK_SUCCESS;
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
