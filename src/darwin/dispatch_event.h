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
#include <stdint.h>

#include <ApplicationServices/ApplicationServices.h>

extern bool dispatch_hook_enabled(uint64_t timestamp);

extern bool dispatch_hook_disabled(uint64_t timestamp);

extern bool dispatch_key_press(uint64_t timestamp, CGEventRef event_ref);

extern bool dispatch_key_release(uint64_t timestamp, CGEventRef event_ref);

/* These events are totally undocumented for the CGEvent type, but are required to grab media and caps-lock keys. */
extern bool dispatch_system_key(uint64_t timestamp, CGEventRef event_ref);

extern bool dispatch_modifier_change(uint64_t timestamp, CGEventRef event_ref);

extern bool dispatch_button_press(uint64_t timestamp, CGEventRef event_ref, uint16_t button);

extern bool dispatch_button_release(uint64_t timestamp, CGEventRef event_ref, uint16_t button);

extern bool dispatch_mouse_move(uint64_t timestamp, CGEventRef event_ref);

extern bool dispatch_mouse_wheel(uint64_t timestamp, CGEventRef event_ref);
