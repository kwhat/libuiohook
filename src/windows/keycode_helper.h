/* libUIOHook: Cross-platform keyboard and mouse hooking from userland.
 * Copyright (C) 2006-2026 Alexander Barker.  All Rights Reserved.
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

#ifndef _included_keycode_helper
#define _included_keycode_helper

#include <stdint.h>
#include <windows.h>

/* Map a uiohook virtual code to a Windows virtual-key code, handling modifier aliases and keypad
 * codes; return zero if unmapped. */
extern DWORD uiocode_to_vkcode(uint16_t uiocode);

/* Map a Windows virtual-key code and hook flags to a uiohook virtual code, distinguishing keypad
 * Enter and navigation keys; return VC_UNDEFINED if unmapped. */
extern uint16_t vkcode_to_uiocode(DWORD vk_code, DWORD flags);

#endif
