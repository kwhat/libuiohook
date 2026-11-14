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

// Reference: http://boredzo.org/blog/wp-content/uploads/2007/05/imtx-virtual-keycodes.png
// Reference: https://svn.blender.org/svnroot/bf-blender/branches/render25/intern/ghost/intern/GHOST_SystemCocoa.mm
// Reference: http://www.mactech.com/macintosh-c/chap02-1.html

#ifndef _included_input_helper
#define _included_input_helper

#ifdef USE_APPLICATION_SERVICES
#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h> // For HIToolbox kVK_ key codes and TIS functions.
#else
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#endif

#include <stdbool.h>
#include <stdint.h>

#include "keycode_helper.h"

/* Check whether accessibility access is enabled, requesting the native permission prompt when the
 * modern API is available. */
extern bool is_accessibility_enabled();

/* Translate a keyboard event into UTF-16 characters using the native layout services, synchronizing
 * with the main runloop as needed; return the number written. */
extern UniCharCount event_to_unicode(CGEventRef event_ref, UniChar *buffer, UniCharCount size);

/* Extract the native system-event subtype and data1 fields, synchronizing with the main runloop as
 * needed. */
extern void event_to_objc(CGEventRef event_ref, UInt32 *subtype, UInt32 *data1);

/* Initialize modifier and dead-key state and prepare main-thread dispatch or runloop fallback
 * resources; return UIOHOOK_SUCCESS or an initialization error. */
extern int load_input_helper();

/* Release the fallback main-runloop resources and retained keyboard-layout reference when
 * applicable. */
extern void unload_input_helper();

#endif
