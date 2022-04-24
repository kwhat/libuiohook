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

#include <stdbool.h>
#include <windows.h>

extern bool dispatch_hook_enable();

extern bool dispatch_hook_disable();

extern bool dispatch_key_press(KBDLLHOOKSTRUCT *kbhook);

extern bool dispatch_key_release(KBDLLHOOKSTRUCT *kbhook);

extern bool dispatch_button_press(MSLLHOOKSTRUCT *mshook, uint16_t button);

extern bool dispatch_button_release(MSLLHOOKSTRUCT *mshook, uint16_t button);

extern bool dispatch_mouse_move(MSLLHOOKSTRUCT *mshook);

extern bool dispatch_mouse_wheel(MSLLHOOKSTRUCT *mshook, uint8_t direction);
