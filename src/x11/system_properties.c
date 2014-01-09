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
#include <X11/Xlib.h>
#ifdef USE_XKB
#include <X11/XKBlib.h>
#endif
#ifdef USE_XF86MISC
#include <X11/extensions/xf86misc.h>
#include <X11/extensions/xf86mscstr.h>
#endif
#ifdef USE_XT
#include <X11/Intrinsic.h>

static XtAppContext xt_context;
static Display *xt_disp;
#endif

#include "copyright.h"
#include "logger.h"

Display *disp;

UIOHOOK_API long int hook_get_auto_repeat_rate() {
	bool successful = false;
	long int value = -1;
	unsigned int delay = 0, rate = 0;
	
	#ifdef USE_XKB
	// Attempt to acquire the keyboard auto repeat rate using the XKB extension.
	if (!successful) {
		successful = XkbGetAutoRepeatRate(disp, XkbUseCoreKbd, &delay, &rate);

		if (successful) {
			logger(LOG_LEVEL_INFO,	"%s [%u]: XkbGetAutoRepeatRate: %u.\n", 
					__FUNCTION__, __LINE__, rate);
		}
	}
	#endif

	#ifdef USE_XF86MISC
	// Fallback to the XF86 Misc extension if available and other efforts failed.
	if (!successful) {
		XF86MiscKbdSettings kb_info;
		successful = (bool) XF86MiscGetKbdSettings(disp, &kb_info);
		if (successful) {
			logger(LOG_LEVEL_INFO,	"%s [%u]: XF86MiscGetKbdSettings: %i.\n", 
					__FUNCTION__, __LINE__, kbdinfo.rate);

			delay = (unsigned int) kbdinfo.delay;
			rate = (unsigned int) kbdinfo.rate;
		}
	}
	#endif

	if (successful) {
		value = (long int) rate;
	}

	return value;
}

UIOHOOK_API long int hook_get_auto_repeat_delay() {
	bool successful = false;
	long int value = -1;
	unsigned int delay = 0, rate = 0;

	#ifdef USE_XKB
	// Attempt to acquire the keyboard auto repeat rate using the XKB extension.
	if (!successful) {
		successful = XkbGetAutoRepeatRate(disp, XkbUseCoreKbd, &delay, &rate);

		if (successful) {
			logger(LOG_LEVEL_INFO,	"%s [%u]: XkbGetAutoRepeatRate: %u.\n", 
					__FUNCTION__, __LINE__, delay);
		}
	}
	#endif

	#ifdef USE_XF86MISC
	// Fallback to the XF86 Misc extension if available and other efforts failed.
	if (!successful) {
		XF86MiscKbdSettings kb_info;
		successful = (bool) XF86MiscGetKbdSettings(disp, &kb_info);
		if (successful) {
			logger(LOG_LEVEL_INFO,	"%s [%u]: XF86MiscGetKbdSettings: %i.\n", 
					__FUNCTION__, __LINE__, kbdinfo.delay);

			delay = (unsigned int) kbdinfo.delay;
			rate = (unsigned int) kbdinfo.rate;
		}
	}
	#endif

	if (successful) {
		value = (long int) delay;
	}

	return value;
}

UIOHOOK_API long int hook_get_pointer_acceleration_multiplier() {
	long int value = -1;
	int accel_numerator, accel_denominator, threshold;

	XGetPointerControl(disp, &accel_numerator, &accel_denominator, &threshold);
	if (accel_denominator >= 0) {
		logger(LOG_LEVEL_INFO,	"%s [%u]: XGetPointerControl: %i.\n", 
				__FUNCTION__, __LINE__, accel_denominator);

		value = (long int) accel_denominator;
	}

	return value;
}

UIOHOOK_API long int hook_get_pointer_acceleration_threshold() {
	long int value = -1;
	int accel_numerator, accel_denominator, threshold;

	XGetPointerControl(disp, &accel_numerator, &accel_denominator, &threshold);
	if (threshold >= 0) {
		logger(LOG_LEVEL_INFO,	"%s [%u]: XGetPointerControl: %i.\n", 
				__FUNCTION__, __LINE__, threshold);

		value = (long int) threshold;
	}
	
	return value;
}

UIOHOOK_API long int hook_get_pointer_sensitivity() {
	long int value = -1;
	int accel_numerator, accel_denominator, threshold;

	XGetPointerControl(disp, &accel_numerator, &accel_denominator, &threshold);
	if (accel_numerator >= 0) {
		logger(LOG_LEVEL_INFO,	"%s [%u]: XGetPointerControl: %i.\n", 
				__FUNCTION__, __LINE__, accel_numerator);

		value = (long int) accel_numerator;
	}
	
	return value;
}

UIOHOOK_API long int hook_get_multi_click_time() {
	long int value = 200;
	int click_time;
	bool successful = false;

	#ifdef USE_XT
	// Try and use the Xt extention to get the current multi-click.
	if (!successful) {
		// Fall back to the X Toolkit extension if available and other efforts failed.
		click_time = XtGetMultiClickTime(xt_disp);
		if (click_time >= 0) {
			logger(LOG_LEVEL_INFO,	"%s [%u]: XtGetMultiClickTime: %i.\n", 
					__FUNCTION__, __LINE__, click_time);

			successful = true;
		}
	}
	#endif

	// Try and acquire the multi-click time from the user defined X defaults.
	if (!successful) {
		char *xprop = XGetDefault(disp, "*", "multiClickTime");
		if (xprop != NULL && sscanf(xprop, "%4i", &click_time) != EOF) {
			logger(LOG_LEVEL_INFO,	"%s [%u]: X default 'multiClickTime' property: %i.\n", 
					__FUNCTION__, __LINE__, click_time);

			successful = true;
		}
	}

	if (!successful) {
		char *xprop = XGetDefault(disp, "OpenWindows", "MultiClickTimeout");
		if (xprop != NULL && sscanf(xprop, "%4i", &click_time) != EOF) {
			logger(LOG_LEVEL_INFO,	"%s [%u]: X default 'MultiClickTimeout' property: %i.\n", 
					__FUNCTION__, __LINE__, click_time);

			successful = true;
		}
	}
	
	if (successful) {
		value = (long int) click_time;
	}

	return value;
}

// Create a shared object constructor.
__attribute__ ((constructor))
void on_library_load() {
	// Display the copyright on library load.
	COPYRIGHT();

	// Tell X Threads are OK.
	XInitThreads();

	// Open local display.
	disp = XOpenDisplay(XDisplayName(NULL));
	if (disp == NULL) {
		logger(LOG_LEVEL_ERROR,	"%s [%u]: %s\n",
				__FUNCTION__, __LINE__, "XOpenDisplay failure!");
	}
	else {
		logger(LOG_LEVEL_DEBUG,	"%s [%u]: %s\n",
				__FUNCTION__, __LINE__, "XOpenDisplay success.");
	}

	#ifdef USE_XT
	XtToolkitInitialize();
	xt_context = XtCreateApplicationContext();

	int argc = 0;
	char ** argv = { NULL };
	xt_disp = XtOpenDisplay(xt_context, NULL, "UIOHook", "libuiohook", NULL, 0, &argc, argv);
	#endif
}

// Create a shared object destructor.
__attribute__ ((destructor))
void on_library_unload() {
	/* FIXME This seems to be causing problems with SIGTERM
	// Stop the native thread if its running.
	if (hook_is_enabled()) {
		hook_disable();
	}
	*/

	#ifdef USE_XT
	XtCloseDisplay(xt_disp);
	XtDestroyApplicationContext(xt_context);
	#endif

	// Destroy the native displays.
	if (disp != NULL) {
		XCloseDisplay(disp);
		disp = NULL;
	}
}
