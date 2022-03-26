/* libUIOHook: Cross-platform keyboard and mouse hooking from userland.
 * Copyright (C) 2006-2022 Alexander Barker.  All Rights Reserved.
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

#include <X11/Xlib.h>

// FIXME Shouldn't be extern, remove
#include <uiohook.h>
extern void dispatch_event(uiohook_event *const uio_event);

extern void dispatch_hook_enabled();

extern void dispatch_hook_disabled();

extern void dispatch_key_press(XKeyPressedEvent * const x_event);

extern void dispatch_key_release(XKeyReleasedEvent * const x_event);

extern void dispatch_mouse_press(XButtonEvent * const x_event);

extern void dispatch_mouse_release(XButtonEvent * const x_event);

extern void dispatch_mouse_move(XMotionEvent * const x_event);
