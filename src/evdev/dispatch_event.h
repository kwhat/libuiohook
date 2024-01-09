/* libUIOHook: Cross-platform keyboard and mouse hooking from userland.
 * Copyright (C) 2006-2023 Alexander Barker.  All Rights Reserved.
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

#include <xkbcommon/xkbcommon.h>
#include <linux/input.h>
#include <uiohook.h>

extern void dispatch_event(uiohook_event *const uio_event);

extern void dispatch_hook_enabled();

extern void dispatch_hook_disabled();

extern bool dispatch_key_press(uint64_t timestamp, xkb_keycode_t keycode, xkb_keysym_t keysym);

extern bool dispatch_key_release(uint64_t timestamp, xkb_keycode_t keycode, xkb_keysym_t keysym);

extern bool dispatch_mouse_press(struct input_event *const ev);

extern bool dispatch_mouse_release(struct input_event *const ev);

extern bool dispatch_mouse_move(uint64_t timestamp, int16_t x, int16_t y);

extern bool dispatch_mouse_wheel(uint64_t timestamp, int16_t rotation, uint8_t direction);
