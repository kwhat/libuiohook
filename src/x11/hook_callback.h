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

#ifndef _included_hook_callback
#define _included_hook_callback

#include <X11/Xlib.h>
#include <X11/extensions/record.h>

extern XRecordContext context;

typedef struct _hook_data {
	Display *display;
	XRecordRange *range;
} hook_data;
		
extern void hook_cleanup_proc(void *arg);

// Callback used by hook_thead for all events.
extern void hook_event_proc(XPointer pointer, XRecordInterceptData *hook);

#endif
