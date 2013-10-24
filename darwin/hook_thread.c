/* JNativeHook: Global keyboard and mouse hooking for Java.
 * Copyright (C) 2006-2013 Alexander Barker.  All Rights Received.
 * http://code.google.com/p/jnativehook/
 *
 * JNativeHook is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * JNativeHook is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <pthread.h>
#include <sys/time.h>

#include <ApplicationServices/ApplicationServices.h>

#include "hook_callback.h"
#include "logger.h"


// Thread and hook handles.
static CFRunLoopRef event_loop;
static CFRunLoopSourceRef event_source;

static pthread_mutex_t hook_running_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t hook_control_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t hook_thread_id; // = 0; ?
static pthread_attr_t hook_thread_attr;


static void *hook_thread_proc(void *arg) {
	int *status = (int *) arg;
	*status = NATIVEHOOK_FAILURE;

	// Check and make sure assistive devices is enabled.
	if (AXAPIEnabled() == true) {
		#ifdef DEBUG
		fprintf(stdout, "ThreadProc(): Accessibility API is enabled.\n");
		#endif

		// Setup the event mask to listen for.
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
									CGEventMaskBit(kCGEventScrollWheel);

		#ifdef DEBUG
		event_mask |= CGEventMaskBit(kCGEventNull);
		#endif

		CFMachPortRef event_port = CGEventTapCreate(
										kCGSessionEventTap,				// kCGHIDEventTap
										kCGHeadInsertEventTap,			// kCGTailAppendEventTap
										kCGEventTapOptionListenOnly,	// kCGEventTapOptionDefault See Bug #22
										event_mask,
										hook_event_proc,
										NULL
									);


		if (event_port != NULL) {
			logger(LOG_LEVEL_DEBUG,	"%s [%u]: CGEventTapCreate Successful.\n", 
					__FUNCTION__, __LINE__);

			event_source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, event_port, 0);
			if (event_source != NULL) {
				logger(LOG_LEVEL_DEBUG,	"%s [%u]: CFMachPortCreateRunLoopSource successful.\n", 
						__FUNCTION__, __LINE__);

				event_loop = CFRunLoopGetCurrent();
				if (event_loop != NULL) {
					logger(LOG_LEVEL_DEBUG,	"%s [%u]: CFRunLoopGetCurrent successful.\n", 
							__FUNCTION__, __LINE__);

					// Initialize Native Input Functions.
					LoadInputHelper();


					// Create run loop observers.
					CFRunLoopObserverRef observer = CFRunLoopObserverCreate(
														kCFAllocatorDefault,
														kCFRunLoopEntry | kCFRunLoopExit, //kCFRunLoopAllActivities,
														true,
														0,
														hook_status_proc,
														NULL
													);

					if (observer != NULL) {
						// Set the exit status.
						*status = NATIVEHOOK_SUCCESS;

						start_message_port_runloop();

						CFRunLoopAddSource(event_loop, event_source, kCFRunLoopDefaultMode);
						CFRunLoopAddObserver(event_loop, observer, kCFRunLoopDefaultMode);

						CFRunLoopRun();

						// Lock back up until we are done processing the exit.
						CFRunLoopRemoveObserver(event_loop, observer, kCFRunLoopDefaultMode);
						CFRunLoopRemoveSource(event_loop, event_source, kCFRunLoopDefaultMode);
						CFRunLoopObserverInvalidate(observer);

						stop_message_port_runloop();
					}
					else {
						// We cant do a whole lot of anything if we cant
						// create run loop observer.

						logger(LOG_LEVEL_ERROR,	"%s [%u]: CFRunLoopObserverCreate failure!\n", 
								__FUNCTION__, __LINE__);

						// Set the exit status.
						*status = NATIVEHOOK_ERROR_OBSERVER_CREATE;
					}

					// Callback for additional thread cleanup.
					ThreadStopCallback();
				}
				else {
					logger(LOG_LEVEL_ERROR,	"%s [%u]: CFRunLoopGetCurrent failure!\n", 
							__FUNCTION__, __LINE__);

					// Set the exit status.
					*status = NATIVEHOOK_ERROR_GET_RUNLOOP;
				}

				// Clean up the event source.
				CFRelease(event_source);
			}
			else {
				logger(LOG_LEVEL_ERROR,	"%s [%u]: CFMachPortCreateRunLoopSource failure!\n", 
					__FUNCTION__, __LINE__);

				// Set the exit status.
				*status = NATIVEHOOK_ERROR_CREATE_RUN_LOOP_SOURCE;
			}

			// Stop the CFMachPort from receiving any more messages.
			CFMachPortInvalidate(event_port);
			CFRelease(event_port);
		}
		else {
			logger(LOG_LEVEL_ERROR,	"%s [%u]: Failed to create event port!\n", 
					__FUNCTION__, __LINE__);

			// Set the exit status.
			*status = NATIVEHOOK_ERROR_EVENT_PORT;
		}
	}
	else {
		logger(LOG_LEVEL_ERROR,	"%s [%u]: Accessibility API is disabled!\n", 
				__FUNCTION__, __LINE__);

		// Set the exit status.
		*status = NATIVEHOOK_ERROR_AXAPI_DISABLED;
	}

	logger(LOG_LEVEL_DEBUG,	"%s [%u]: Something, something, something, complete.\n", 
			__FUNCTION__, __LINE__);

	// Make sure we signal that we have passed any exception throwing code.
	pthread_mutex_unlock(&hook_control_mutex);

	pthread_exit(status);
}

NATIVEHOOK_API int hook_enable() {
	int status = NATIVEHOOK_FAILURE;

	// We shall use the default pthread attributes: thread is joinable
	// (not detached) and has default (non real-time) scheduling policy.
	//pthread_mutex_init(&hook_control_mutex, NULL);

	// Lock the thread control mutex.  This will be unlocked when the
	// thread has finished starting, or when it has fully stopped.
	pthread_mutex_lock(&hook_control_mutex);

	// Make sure the native thread is not already running.
	if (hook_is_enabled() != true) {
		if (pthread_create(&hook_thread_id, NULL, ThreadProc, malloc(sizeof(int))) == 0) {
			logger(LOG_LEVEL_DEBUG,	"%s [%u]: Start successful\n", 
					__FUNCTION__, __LINE__);

			if (pthread_setschedprio(hook_thread_id, priority) != 0) {
				logger(LOG_LEVEL_ERROR,	"%s [%u]: Could not set thread priority %i for thread 0x%lX!\n", 
						__FUNCTION__, __LINE__, priority, (unsigned long) hook_thread_id);
			}
			
			// Wait for the thread to unlock the control mutex indicating
			// that it has started or failed.
			if (pthread_mutex_lock(&hook_control_mutex) == 0) {
				pthread_mutex_unlock(&hook_control_mutex);
			}

			// Handle any possible JNI issue that may have occurred.
			if (hook_is_enabled()) {
				logger(LOG_LEVEL_DEBUG,	"%s [%u]: Initialization successful.\n", 
						__FUNCTION__, __LINE__);

				status = NATIVEHOOK_SUCCESS;
			}
			else {
				logger(LOG_LEVEL_ERROR,	"%s [%u]: Initialization failure!\n", 
						__FUNCTION__, __LINE__);

				// Wait for the thread to die.
				void *thread_status;
				pthread_join(hook_thread_id, (void *) &thread_status);
				status = *(int *) thread_status;
				free(thread_status);

				logger(LOG_LEVEL_ERROR,	"%s [%u]: Thread Result: (%i)!\n", 
						__FUNCTION__, __LINE__, status);
			}
		}
		else {
			logger(LOG_LEVEL_ERROR,	"%s [%u]: Thread create failure!\n", 
					__FUNCTION__, __LINE__);

			status = NATIVEHOOK_ERROR_THREAD_CREATE;
		}
	}

	// Make sure the control mutex is unlocked.
	pthread_mutex_unlock(&hook_control_mutex);

	return status;
}

NATIVEHOOK_API int hook_disable() {
	int status = NATIVEHOOK_FAILURE;

	// Lock the thread control mutex.  This will be unlocked when the
	// thread has fully stopped.
	pthread_mutex_lock(&hook_control_mutex);

	if (hook_is_enabled() == true) {
		// Stop the run loop.
		CFRunLoopStop(event_loop);

		// Wait for the thread to die.
		void *thread_status;
		pthread_join(hook_thread_id, &thread_status);
		status = *(int *) thread_status;
		free(thread_status);

		// Clean up the thread attribute.
		pthread_attr_destroy(&hook_thread_attr);
		
		logger(LOG_LEVEL_DEBUG,	"%s [%u]: Thread Result (%i).\n", 
				__FUNCTION__, __LINE__, status);
	}

	// Clean up the mutex.
	//pthread_mutex_destroy(&hook_control_mutex);

	// Make sure the mutex gets unlocked.
	pthread_mutex_unlock(&hook_control_mutex);

	return status;
}

NATIVEHOOK_API bool hook_is_enabled() {
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

	return isRunning;
}
