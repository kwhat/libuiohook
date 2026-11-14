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

#ifndef _included_input_helper
#define _included_input_helper

#include <limits.h>
#include <stdint.h>
#include <windows.h>

#include "unicode_helper.h"

#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020E
#endif

/* Converts a uiohook virtual key code to the appropriate Windows VK key code. */
extern DWORD uiocode_to_vkcode(uint16_t uiocode);

/* Converts a Windows VK key code to the appropriate uiohook virtual key code. */
extern uint16_t vkcode_to_uiocode(DWORD vk_code, DWORD flags);

/* Set the native modifier mask for future events. */
extern void set_modifier_mask(uint16_t mask);

/* Unset the native modifier mask for future events. */
extern void unset_modifier_mask(uint16_t mask);

/* Get the current native modifier mask state. */
extern uint16_t get_modifiers();

/* Initialize modifier state and the Unicode layout cache. */
extern int load_input_helper();

/* Cleanup the initialized locales. */
extern void unload_input_helper();

#endif
