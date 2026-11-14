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

#ifndef _included_dispatch_event
#define _included_dispatch_event

#include <stdbool.h>
#include <stdint.h>

#ifdef USE_APPLICATION_SERVICES
#include <ApplicationServices/ApplicationServices.h>
#else
#include <CoreGraphics/CoreGraphics.h>
#endif

/* Dispatch the hook-enabled notification; return whether the event was consumed. */
extern bool dispatch_hook_enabled(uint64_t timestamp);

/* Dispatch the hook-disabled notification; return whether the event was consumed. */
extern bool dispatch_hook_disabled(uint64_t timestamp);

/* Translate and dispatch a key press and any resulting typed characters; return whether the event
 * was consumed. */
extern bool dispatch_key_press(uint64_t timestamp, CGEventRef event_ref);

/* Translate and dispatch a key release; return whether the event was consumed. */
extern bool dispatch_key_release(uint64_t timestamp, CGEventRef event_ref);

/* Translate and dispatch native media or Caps Lock system-key events; return whether the event was
 * consumed. */
extern bool dispatch_system_key(uint64_t timestamp, CGEventRef event_ref);

/* Update modifier state and dispatch corresponding modifier-key events; return whether the event
 * was consumed. */
extern bool dispatch_modifier_change(uint64_t timestamp, CGEventRef event_ref);

/* Update click tracking and dispatch a mouse-button press; return whether the event was consumed. */
extern bool dispatch_button_press(uint64_t timestamp, CGEventRef event_ref, uint16_t button);

/* Dispatch a mouse-button release and synthesize a click when appropriate; return whether the event
 * was consumed. */
extern bool dispatch_button_release(uint64_t timestamp, CGEventRef event_ref, uint16_t button);

/* Dispatch pointer motion as a move or drag event; return whether the event was consumed. */
extern bool dispatch_mouse_move(uint64_t timestamp, CGEventRef event_ref);

/* Dispatch wheel motion using the native scroll settings; return whether the event was consumed. */
extern bool dispatch_mouse_wheel(uint64_t timestamp, CGEventRef event_ref);

#endif
