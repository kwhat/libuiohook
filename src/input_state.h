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

#ifndef _included_input_state
#define _included_input_state

#include <stdint.h>

/* Set the specified modifier and mouse-button bits without changing other bits. */
extern void set_modifier_mask(uint16_t mask);

/* Clear the specified modifier and mouse-button bits without changing other bits. */
extern void unset_modifier_mask(uint16_t mask);

/* Return the current modifier and mouse-button mask. */
extern uint16_t get_modifiers(void);

#endif
