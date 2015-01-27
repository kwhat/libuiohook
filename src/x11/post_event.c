/* libUIOHook: Cross-platfrom userland keyboard and mouse hooking.
 * Copyright (C) 2006-2015 Alexander Barker.  All Rights Received.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <uiohook.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#ifdef USE_XTEST
#include <X11/extensions/XTest.h>
#endif

#include "input_helper.h"
#include "logger.h"

extern Display *disp;

// This lookup table must be in the same order the masks are defined.
#ifdef USE_XTEST
static KeySym keymask_lookup[8] = {
	XK_Shift_L,
	XK_Control_L,
	XK_Meta_L,
	XK_Alt_L,

	XK_Shift_R,
	XK_Control_R,
	XK_Meta_R,
	XK_Alt_R
};

static unsigned int btnmask_lookup[5] = {
	MASK_BUTTON1,
	MASK_BUTTON2,
	MASK_BUTTON3,
	MASK_BUTTON4,
	MASK_BUTTON5
};
#else
// TODO Possibly relocate to input helper.
static unsigned int convert_to_native_mask(unsigned int mask) {
	unsigned int native_mask = 0x00;

	if (mask & (MASK_SHIFT))	{ native_mask |= ShiftMask;		}
	if (mask & (MASK_CTRL))		{ native_mask |= ControlMask;	}
	if (mask & (MASK_META))		{ native_mask |= Mod4Mask;		}
	if (mask & (MASK_ALT))		{ native_mask |= Mod1Mask;		}

	if (mask & MASK_BUTTON1)	{ native_mask |= Button1Mask;	}
	if (mask & MASK_BUTTON2)	{ native_mask |= Button2Mask;	}
	if (mask & MASK_BUTTON3)	{ native_mask |= Button3Mask;	}
	if (mask & MASK_BUTTON4)	{ native_mask |= Button4Mask;	}
	if (mask & MASK_BUTTON5)	{ native_mask |= Button5Mask;	}

	return native_mask;
}

static inline XKeyEvent * create_key_event() {
	XKeyEvent *event = malloc(sizeof(XKeyEvent));

	event->serial = 0x00;
	event->send_event = False;
	event->display = disp;
	event->time = CurrentTime;
	event->same_screen = True;

	unsigned int mask;
	if (!XQueryPointer(disp, DefaultRootWindow(disp), &(event->root), &(event->subwindow), &(event->x_root), &(event->y_root), &(event->x), &(event->y), &mask)) {
		event->root = DefaultRootWindow(disp);
		event->window = event->root;
		event->subwindow = None;

		event->x_root = 0;
		event->y_root = 0;
		event->x = 0;
		event->y = 0;
	}

	event->type = 0x00;
	event->state = 0x00;
	event->keycode = 0x00;

	return event;
}

static inline XButtonEvent * create_button_event() {
	XButtonEvent *event = malloc(sizeof(XButtonEvent));

	event->serial = 0x00;
	event->send_event = False;
	event->display = disp;
	event->time = CurrentTime;
	event->same_screen = True;

	event->root = DefaultRootWindow(disp);
	event->window = event->root;
	event->subwindow = None;

	event->type = 0x00;
	event->state = 0x00;
	event->x_root = 0;
	event->y_root = 0;
	event->x = 0;
	event->y = 0;
	event->button = 0x00;

	return event;
}

static inline XMotionEvent * create_motion_event() {
	XMotionEvent *event = malloc(sizeof(XMotionEvent));

	event->serial = MotionNotify;
	event->send_event = False;
	event->display = disp;
	event->time = CurrentTime;
	event->same_screen = True;
	event->is_hint = NotifyNormal,
	event->root = DefaultRootWindow(disp);
	event->window = event->root;
	event->subwindow = None;

	event->type = 0x00;
	event->state = 0x00;
	event->x_root = 0;
	event->y_root = 0;
	event->x = 0;
	event->y = 0;

	return event;
}
#endif

UIOHOOK_API void hook_post_event(uiohook_event * const event) {
	#ifdef USE_XTEST
	// XTest does not have modifier support, so we fake it by depressing the
	// appropriate modifier keys.
	for (unsigned int i = 0; i < sizeof(keymask_lookup) / sizeof(KeySym); i++) {
		if (event->mask & 1 << i) {
			XTestFakeKeyEvent(disp, XKeysymToKeycode(disp, keymask_lookup[i]), True, 0);
		}
	}

	for (unsigned int i = 0; i < sizeof(btnmask_lookup) / sizeof(unsigned int); i++) {
		if (event->mask & btnmask_lookup[i]) {
			XTestFakeButtonEvent(disp, i + 1, True, 0);
		}
	}

	switch (event->type) {
		case EVENT_KEY_PRESSED:
			XTestFakeKeyEvent(
				disp,
				scancode_to_keycode(event->data.keyboard.keycode),
				True,
				0);
			break;

		case EVENT_KEY_RELEASED:
			XTestFakeKeyEvent(
				disp,
				scancode_to_keycode(event->data.keyboard.keycode),
				False,
				0);
			break;


		case EVENT_MOUSE_PRESSED:
			XTestFakeButtonEvent(disp, event->data.mouse.button, True, 0);
			break;

		case EVENT_MOUSE_RELEASED:
			XTestFakeButtonEvent(disp, event->data.mouse.button, False, 0);
			break;

		case EVENT_MOUSE_WHEEL:
			// Wheel events should be the same as click events on X11.
			// type, amount and rotation
			if (event->data.wheel.rotation < 0) {
				XTestFakeButtonEvent(disp, WheelUp, True, 0);
				XTestFakeButtonEvent(disp, WheelUp, False, 0);
			}
			else {
				XTestFakeButtonEvent(disp, WheelDown, True, 0);
				XTestFakeButtonEvent(disp, WheelDown, False, 0);
			}
			break;


		case EVENT_MOUSE_DRAGGED:
			// The button masks are all applied with the modifier masks.

		case EVENT_MOUSE_MOVED:
			XTestFakeMotionEvent(disp, -1, event->data.mouse.x, event->data.mouse.y, 0);
			break;


		case EVENT_MOUSE_CLICKED:
		case EVENT_KEY_TYPED:
			// Ignore clicked and typed events.

		case EVENT_HOOK_ENABLED:
		case EVENT_HOOK_DISABLED:
			// Ignore hook enabled / disabled events.

		default:
			// Ignore any other garbage.
			logger(LOG_LEVEL_WARN, "%s [%u]: Ignoring post event type %#X\n",
				__FUNCTION__, __LINE__, event->type);
			break;
	}

	// Release the previously held modifier keys used to fake the event mask.
	for (unsigned int i = 0; i < sizeof(keymask_lookup) / sizeof(KeySym); i++) {
		if (event->mask & 1 << i) {
			XTestFakeKeyEvent(disp, XKeysymToKeycode(disp, keymask_lookup[i]), False, 0);
		}
	}

	for (unsigned int i = 0; i < sizeof(btnmask_lookup) / sizeof(unsigned int); i++) {
		if (event->mask & btnmask_lookup[i]) {
			XTestFakeButtonEvent(disp, i + 1, False, 0);
		}
	}
	#else
	XEvent *x_event;

	#if defined(USE_XINERAMA) || defined(USE_XRANDR)
	uint8_t screen_count;
	screen_data *screens;
	#endif

	switch (event->type) {
		case EVENT_KEY_PRESSED:
		case EVENT_KEY_RELEASED:
			// Allocate memory for XKeyEvent and pre-populate.
			x_event = (XEvent *) create_key_event();

			((XKeyEvent *) x_event)->state = convert_to_native_mask(event->mask);
			((XKeyEvent *) x_event)->keycode = XKeysymToKeycode(disp, scancode_to_keycode(event->data.keyboard.keycode));

			if (event->type == EVENT_KEY_PRESSED) {
				((XKeyEvent *) x_event)->type = KeyPress;
				XSendEvent(disp, InputFocus, False, KeyPressMask, x_event);
			}
			else {
				((XKeyEvent *) x_event)->type = KeyRelease;
				XSendEvent(disp, InputFocus, False, KeyReleaseMask, x_event);
			}

			free(x_event);
			break;


		case EVENT_MOUSE_PRESSED:
		case EVENT_MOUSE_RELEASED:
		case EVENT_MOUSE_WHEEL:
			// Allocate memory for XButtonEvent and pre-populate.
			x_event = (XEvent *) create_button_event();

			((XButtonEvent *) x_event)->state = convert_to_native_mask(event->mask);

			((XButtonEvent *) x_event)->x = event->data.mouse.x;
			((XButtonEvent *) x_event)->y = event->data.mouse.y;

			#if defined(USE_XINERAMA) || defined(USE_XRANDR)
			screens = hook_create_screen_info(&screen_count);
			if (screen_count > 1) {
				((XButtonEvent *) x_event)->x += screens[0].x;
				((XButtonEvent *) x_event)->y += screens[0].y;
			}

			if (screens != NULL) {
				free(screens);
			}
			#endif

			// These are the same because Window == Root Window.
			((XButtonEvent *) x_event)->x_root = ((XButtonEvent *) x_event)->x;
			((XButtonEvent *) x_event)->y_root = ((XButtonEvent *) x_event)->y;

			if (event->type == EVENT_MOUSE_WHEEL) {
				((XButtonEvent *) x_event)->type = ButtonPress;

				// type, amount and rotation
				if (event->data.wheel.rotation < 0) {
					((XButtonEvent *) x_event)->button = WheelUp;
				}
				else {
					((XButtonEvent *) x_event)->button = WheelDown;
				}
				XSendEvent(disp, InputFocus, False, ButtonPressMask, x_event);
			}

			if (event->type == EVENT_KEY_PRESSED) {
				((XButtonEvent *) x_event)->type = ButtonPress;
				XSendEvent(disp, InputFocus, False, ButtonPressMask, x_event);
			}
			else {
				((XButtonEvent *) x_event)->type = ButtonRelease;
				XSendEvent(disp, InputFocus, False, ButtonReleaseMask, x_event);
			}

			free(x_event);
			break;

		case EVENT_MOUSE_MOVED:
		case EVENT_MOUSE_DRAGGED:
			x_event = (XEvent *) create_motion_event();

			((XMotionEvent *) x_event)->state = convert_to_native_mask(event->mask);

			((XButtonEvent *) x_event)->x = event->data.mouse.x;
			((XButtonEvent *) x_event)->y = event->data.mouse.y;

			#if defined(USE_XINERAMA) || defined(USE_XRANDR)
			screens = hook_create_screen_info(&screen_count);
			if (screen_count > 1) {
				((XButtonEvent *) x_event)->x += screens[0].x;
				((XButtonEvent *) x_event)->y += screens[0].y;
			}

			if (screens != NULL) {
				free(screens);
			}
			#endif

			// These are the same because Window == Root Window.
			((XButtonEvent *) x_event)->x_root = ((XButtonEvent *) x_event)->x;
			((XButtonEvent *) x_event)->y_root = ((XButtonEvent *) x_event)->y;

			long int x_mask = NoEventMask;
			if (event->type == EVENT_MOUSE_DRAGGED) {
				#if Button1Mask == Button1MotionMask && \
					Button2Mask == Button2MotionMask && \
					Button3Mask == Button3MotionMask && \
					Button4Mask == Button4MotionMask && \
					Button5Mask == Button5MotionMask
				// This little trick only works if Button#MotionMasks align with
				// the Button#Masks.
				x_mask = ((XMotionEvent *) x_event)->state &
						(Button1MotionMask | Button2MotionMask |
						Button2MotionMask | Button3MotionMask | Button5MotionMask);
				#else
				// Fallback to some slightly larger...
				if (((XMotionEvent *) x_event)->state & Button1Mask) {
					x_mask |= Button1MotionMask;
				}

				if (((XMotionEvent *) x_event)->state & Button2Mask) {
					x_mask |= Button2MotionMask;
				}

				if (((XMotionEvent *) x_event)->state & Button3Mask) {
					x_mask |= Button3MotionMask;
				}

				if (((XMotionEvent *) x_event)->state & Button4Mask) {
					x_mask |= Button4MotionMask;
				}

				if (((XMotionEvent *) x_event)->state & Button5Mask) {
					x_mask |= Button5MotionMask;
				}
				#endif
			}

			// NOTE x_mask = NoEventMask.
			XSendEvent(disp, InputFocus, False, x_mask, x_event);
			free(x_event);
			break;


		case EVENT_MOUSE_CLICKED:
		case EVENT_KEY_TYPED:
			// Ignore clicked and typed events.

		case EVENT_HOOK_ENABLED:
		case EVENT_HOOK_DISABLED:
			// Ignore hook enabled / disabled events.

		default:
			// Ignore any other garbage.
			logger(LOG_LEVEL_WARN, "%s [%u]: Ignoring post event type %#X\n",
				__FUNCTION__, __LINE__, event->type);
			break;
	}


	#endif

	// Don't forget to flush!
	XFlush(disp);
}
