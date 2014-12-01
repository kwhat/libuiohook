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

#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <uiohook.h>

// NOTE: This function executes on the hook thread!  If you need to block,
// please do so on another thread with your own event dispatcher implementation.
void dispatch_proc(uiohook_event * const event) {
	char buffer[256] = { 0 };
	size_t length = snprintf(buffer, sizeof(buffer), 
			"id=%i,when=%" PRIu64 ",mask=0x%X", 
			event->type, event->time, event->mask);
	
	switch (event->type) {
		case EVENT_KEY_PRESSED:
			// If the escape key is pressed, naturally terminate the program.
			if (event->data.keyboard.keycode == VC_ESCAPE) {
				hook_disable();
			}
		case EVENT_KEY_RELEASED:
			snprintf(buffer + length, sizeof(buffer) - length, 
				",keycode=%u,rawcode=0x%X",
				event->data.keyboard.keycode, event->data.keyboard.rawcode);
			break;

		case EVENT_KEY_TYPED:
			snprintf(buffer + length, sizeof(buffer) - length, 
				",keychar=%lc,rawcode=%u",
				(wint_t) event->data.keyboard.keychar,
				event->data.keyboard.rawcode);
			break;

		case EVENT_MOUSE_PRESSED:
		case EVENT_MOUSE_RELEASED:
		case EVENT_MOUSE_CLICKED:
		case EVENT_MOUSE_MOVED:
		case EVENT_MOUSE_DRAGGED:
			snprintf(buffer + length, sizeof(buffer) - length, 
				",x=%i,y=%i,button=%i,clicks=%i",
				event->data.mouse.x, event->data.mouse.y,
				event->data.mouse.button, event->data.mouse.clicks);
			break;

		case EVENT_MOUSE_WHEEL:
			snprintf(buffer + length, sizeof(buffer) - length, 
				",type=%i,amount=%i,rotation=%i",
				event->data.wheel.type, event->data.wheel.amount,
				event->data.wheel.rotation);
			break;

		default:
			break;
	}

	fprintf(stdout, "%s\n",	 buffer);
}

bool logger_proc(unsigned int level, const char *format, ...) {
	bool status = false;
	
	va_list args;
	switch (level) {
		#ifndef USE_DEBUG
		case LOG_LEVEL_DEBUG:
		case LOG_LEVEL_INFO:
			va_start(args, format);
			status = vfprintf(stdout, format, args) >= 0;
			va_end(args);
			break;
		#endif

		case LOG_LEVEL_WARN:
		case LOG_LEVEL_ERROR:
			va_start(args, format);
			status = vfprintf(stderr, format, args) >= 0;
			va_end(args);
			break;
	}
	
	return status;
}

int main() {
	// Set the logger callback for library output.
	hook_set_logger_proc(&logger_proc);
	
	// Set the event callback for uiohook events.
	hook_set_dispatch_proc(&dispatch_proc);

	// Start the hook and block.
	// NOTE If EVENT_HOOK_ENABLED was delivered, the status will always succeed.
	int status = hook_enable();

	return status;
}
