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

#include "logger.h"

// NOTE: This function executes on the hook thread!  If you need to block,
// please do so on another thread with your own event dispatcher implementation.
void dispatch_proc(uiohook_event * const event) {
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
				hook_disable();

				hook_continue();
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

		default:
			break;
	}

	logger(LOG_LEVEL_INFO, "\n");
}

int main() {
	hook_set_dispatch_proc(&dispatch_proc);

	int status = hook_enable();
	if (status == UIOHOOK_SUCCESS && hook_is_enabled()) {
		hook_wait();
	}

	return status;
}
