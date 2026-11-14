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

#ifndef _included_mouse_helper
#define _included_mouse_helper

#include <CoreGraphics/CGEventTypes.h>
#include <stdbool.h>

// These button codes do not appear to be defined by Apple.
#define kVK_LBUTTON                  kCGMouseButtonLeft
#define kVK_RBUTTON                  kCGMouseButtonRight
#define kVK_MBUTTON                  kCGMouseButtonCenter
#define kVK_XBUTTON1                 3
#define kVK_XBUTTON2                 4

// These button masks do not appear to be defined by Apple.
#define kCGEventFlagMaskButtonLeft   1U << 0
#define kCGEventFlagMaskButtonRight  1U << 1
#define kCGEventFlagMaskButtonCenter 1U << 2
#define kCGEventFlagMaskXButton1     1U << 3
#define kCGEventFlagMaskXButton2     1U << 4

/* Return the drag state tracked by the native mouse event handler. */
extern bool is_mouse_dragged(void);

/* Set the drag state used to classify motion and suppress synthesized clicks. */
extern void set_mouse_dragged(bool dragged);

#endif
