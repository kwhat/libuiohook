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

#include <stdbool.h>
#include <stdint.h>

// macOS CGKeyCode is a uint16_t. Keep pure mapping/state logic independent of
// Apple frameworks so it can also be tested on other platforms.
#define DARWIN_KEYCODE_UNDEFINED 0x00FF

extern uint16_t keycode_to_uiocode(uint16_t keycode);
extern uint16_t uiocode_to_keycode(uint16_t uiocode);
extern void set_modifier_mask(uint16_t mask);
extern void unset_modifier_mask(uint16_t mask);
extern uint16_t get_modifiers();
extern bool is_mouse_dragged();
extern void set_mouse_dragged(bool dragged);

#endif
