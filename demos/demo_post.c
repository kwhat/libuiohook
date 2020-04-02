/* libUIOHook: Cross-platfrom userland keyboard and mouse hooking.
 * Copyright (C) 2006-2017 Alexander Barker.  All Rights Received.
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

#include <stdio.h>
#include <stdlib.h>
#include <uiohook.h>

#ifdef _WIN32
#include <windows.h>
#endif

// Virtual event pointers
static uiohook_event *event = NULL;

// TODO Implement CLI options.
//int main(int argc, char *argv[]) {
int main() {
	// Allocate memory for the virtual events only once.
	event = (uiohook_event *) malloc(sizeof(uiohook_event));

	event->type = EVENT_KEY_PRESSED;
	event->mask = 0x00;

	event->data.keyboard.keycode = VC_A;
	event->data.keyboard.keychar = CHAR_UNDEFINED;

	hook_post_event(event);

	event->type = EVENT_KEY_RELEASED;

	hook_post_event(event);

	free(event);

	return 0;
}
