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

#include <stdint.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/extensions/record.h>

// Virtual button codes that are not defined by X11.
#define Button1     1
#define Button2     2
#define Button3     3
#define WheelUp     4
#define WheelDown   5
#define WheelLeft   6
#define WheelRight  7
#define XButton1    8
#define XButton2    9

// For this struct, refer to libxnee, requires Xlibint.h
typedef union {
    unsigned char       type;
    xEvent              event;
    xResourceReq        req;
    xGenericReply       reply;
    xError              error;
    xConnSetupPrefix    setup;
} XRecordDatum;

// Helper display used by input helper, properties and post event.
extern Display *helper_disp;


/* Converts a X11 key symbol to the appropriate uiohook virtual key code. */
extern uint16_t keysym_to_vcode(KeySym keycode);

/* Converts a uiohook virtual key code to the appropriate X11 key code. */
extern KeyCode vcode_to_keycode(uint16_t uiocod);

/* Set the native modifier mask for future events. */
extern void set_modifier_mask(uint16_t mask);

/* Unset the native modifier mask for future events. */
extern void unset_modifier_mask(uint16_t mask);

/* Get the current native modifier mask state. */
extern uint16_t get_modifiers();

/* Convert XRecord data to XEvent structures. */
extern void wire_data_to_event(XRecordInterceptData *recorded_data, XEvent *x_event);

/* Lookup a X11 buttons possible remapping and return that value. */
extern uint8_t button_map_lookup(uint8_t button);

/* Lookup a KeySym and get it's string representation. */
extern size_t x_key_event_lookup(XKeyEvent *x_event, wchar_t *surrogate, size_t length, KeySym *keysym);

/* Enable detectable auto-repeat for keys */
extern bool enable_key_repeat();

/* Initialize items required for KeyCodeToKeySym() and KeySymToUnicode()
 * functionality.  This method is called by OnLibraryLoad() and may need to be
 * called in combination with UnloadInputHelper() if the native keyboard layout
 * is changed. */
extern void load_input_helper();

/* De-initialize items required for KeyCodeToKeySym() and KeySymToUnicode()
 * functionality.  This method is called by OnLibraryUnload() and may need to be
 * called in combination with LoadInputHelper() if the native keyboard layout
 * is changed. */
extern void unload_input_helper();

#endif
