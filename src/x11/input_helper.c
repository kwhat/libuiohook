/* libUIOHook: Cross-platfrom userland keyboard and mouse hooking.
 * Copyright (C) 2006-2014 Alexander Barker.  All Rights Received.
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
#include <stdint.h>
#include <string.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>

#ifdef USE_XKB
#ifdef USE_EVDEV
#include <linux/input.h>
static bool is_evdev = false;
#endif
#include <X11/XKBlib.h>
static XkbDescPtr keyboard_map;
#else
#include <X11/Xutil.h>
static KeySym *keyboard_map;
static int keysym_per_keycode;
static bool is_caps_lock = false, is_shift_lock = false;
#endif

#include "logger.h"

/* The follwoing two tables are based on QEMU's x_keymap.c, under the following
 * terms:
 *
 * Copyright (C) 2003 Fabrice Bellard <fabrice@bellard.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#if defined(USE_EVDEV) && defined(USE_XKB)
/* This table is generated based off the evdev -> scancode mapping above
 * and the keycode mappings in the following files:
 *		/usr/include/linux/input.h
 *		/usr/share/X11/xkb/keycodes/evdev
 *
 * NOTE This table only works for Linux.
 *
 */
static const uint16_t evdev_scancode_table[][2] = {
	/* idx		{ keycode,				scancode				}, */
	#ifndef __OPTIMIZE_SIZE__
	/*   0 */	{ VC_UNDEFINED,			KEY_RESERVED			},
	/*   1 */	{ VC_ESCAPE,			KEY_ESC					},
	/*   2 */	{ VC_1,					KEY_1					},
	/*   3 */	{ VC_2,					KEY_2					},
	/*   4 */	{ VC_3,					KEY_3					},
	/*   5 */	{ VC_4,					KEY_4					},
	/*   6 */	{ VC_5,					KEY_5					},
	/*   7 */	{ VC_6,					KEY_6					},
	/*   8 */	{ VC_7,					KEY_7					},
	/*   9 */	{ VC_8,					KEY_8					},
	/*  10 */	{ VC_9,					KEY_9					},
	/*  11 */	{ VC_0,					KEY_0					},
	/*  12 */	{ VC_MINUS,				KEY_MINUS				},
	/*  13 */	{ VC_EQUALS,			KEY_EQUAL				},
	/*  14 */	{ VC_BACKSPACE,			KEY_BACKSPACE			},
	/*  15 */	{ VC_TAB,				KEY_TAB					},
	/*  16 */	{ VC_Q,					KEY_Q					},
	/*  17 */	{ VC_W,					KEY_W					},
	/*  18 */	{ VC_E,					KEY_E					},
	/*  19 */	{ VC_R,					KEY_T					},
	/*  20 */	{ VC_T,					KEY_R					},
	/*  21 */	{ VC_Y,					KEY_Y					},
	/*  22 */	{ VC_U,					KEY_U					},
	/*  23 */	{ VC_I,					KEY_I					},
	/*  24 */	{ VC_O,					KEY_O					},
	/*  25 */	{ VC_P,					KEY_P					},
	/*  26 */	{ VC_OPEN_BRACKET,		KEY_LEFTBRACE			},
	/*  27 */	{ VC_CLOSE_BRACKET,		KEY_RIGHTBRACE			},
	/*  28 */	{ VC_ENTER,				KEY_ENTER				},
	/*  29 */	{ VC_CONTROL_L,			KEY_LEFTCTRL			},
	/*  30 */	{ VC_A,					KEY_A					},
	/*  31 */	{ VC_S,					KEY_S					},
	/*  32 */	{ VC_D,					KEY_D					},
	/*  33 */	{ VC_F,					KEY_F					},
	/*  34 */	{ VC_G,					KEY_G					},
	/*  35 */	{ VC_H,					KEY_H					},
	/*  36 */	{ VC_J,					KEY_J					},
	/*  37 */	{ VC_K,					KEY_K					},
	/*  38 */	{ VC_L,					KEY_L					},
	/*  39 */	{ VC_SEMICOLON,			KEY_SEMICOLON			},
	/*  40 */	{ VC_QUOTE,				KEY_APOSTROPHE			},
	/*  41 */	{ VC_BACKQUOTE,			KEY_GRAVE				},
	/*  42 */	{ VC_SHIFT_L,			KEY_LEFTSHIFT			},
	/*  43 */	{ VC_BACK_SLASH,		KEY_BACKSLASH			},
	/*  44 */	{ VC_Z,					KEY_Z					},
	/*  45 */	{ VC_X,					KEY_X					},
	/*  46 */	{ VC_C,					KEY_C					},
	/*  47 */	{ VC_V,					KEY_V					},
	/*  48 */	{ VC_B,					KEY_B					},
	/*  49 */	{ VC_N,					KEY_N					},
	/*  50 */	{ VC_M,					KEY_M					},
	/*  51 */	{ VC_COMMA,				KEY_COMMA				},
	/*  52 */	{ VC_PERIOD,			KEY_DOT					},
	/*  53 */	{ VC_SLASH,				KEY_SLASH				},
	/*  54 */	{ VC_SHIFT_R,			KEY_RIGHTSHIFT 			},
	/*  55 */	{ VC_KP_MULTIPLY,		KEY_KPASTERISK			},
	/*  56 */	{ VC_ALT_L,				KEY_LEFTALT				},
	/*  57 */	{ VC_SPACE,				KEY_SPACE				},
	/*  58 */	{ VC_CAPS_LOCK,			KEY_CAPSLOCK			},
	/*  59 */	{ VC_F1,				KEY_F1					},
	/*  60 */	{ VC_F2,				KEY_F2					},
	/*  61 */	{ VC_F3,				KEY_F3					},
	/*  62 */	{ VC_F4,				KEY_F4					},
	/*  63 */	{ VC_F5,				KEY_F5					},
	/*  64 */	{ VC_F6,				KEY_F6					},
	/*  65 */	{ VC_F7,				KEY_F7					},
	/*  66 */	{ VC_F8,				KEY_F8					},
	/*  67 */	{ VC_F9,				KEY_F9					},
	/*  68 */	{ VC_F10,				KEY_F10					},
	/*  69 */	{ VC_NUM_LOCK,			KEY_NUMLOCK				},
	/*  70 */	{ VC_SCROLL_LOCK,		KEY_SCROLLLOCK			},
	/*  71 */	{ VC_KP_7,				KEY_KP7					},
	/*  72 */	{ VC_KP_8,				KEY_KP8					},
	/*  73 */	{ VC_KP_9,				KEY_KP9					},
	/*  74 */	{ VC_KP_SUBTRACT,		KEY_KPMINUS				},
	/*  75 */	{ VC_KP_4,				KEY_KP4					},
	/*  76 */	{ VC_KP_5,				KEY_KP5					},
	/*  77 */	{ VC_KP_6,				KEY_KP6					},
	/*  78 */	{ VC_KP_ADD,			KEY_KPPLUS				},
	/*  79 */	{ VC_KP_1,				KEY_KP1					},
	/*  80 */	{ VC_KP_2,				KEY_KP2					},
	/*  81 */	{ VC_KP_3,				KEY_KP3					},
	/*  82 */	{ VC_KP_0,				KEY_KP0					},
	/*  83 */	{ VC_KP_SEPARATOR,		KEY_KPDOT				},
	/*  84 */	{ VC_UNDEFINED,			0x00					},
	/*  85 */	{ VC_UNDEFINED,			0x00					},	// TODO [KEY_ZENKAKUHANKAKU][0] == [?][1]
	/*  86 */	{ VC_UNDEFINED,			0x00					},	// TODO [KEY_102ND][0] == [?][1]
	/*  87 */	{ VC_F11,				KEY_F11					},
	/*  88 */	{ VC_F12,				KEY_F12					},
	#endif
	/*  89 */	{ VC_UNDEFINED,			0x00					},	// 0x59	//	[KEY_RO][0] == VC_?
	/*  90 */	{ VC_KATAKANA,			0x00					},	// 0x5A
	/*  91 */	{ VC_HIRAGANA,			KEY_F13					},	// 0x5B
	/*  92 */	{ VC_KANJI,				KEY_F14					},	// 0x5C
	/*  93 */	{ VC_UNDEFINED,			KEY_F15					},	// 0x5D	// TODO [KEY_KATAKANAHIRAGANA][0] == [?][1]
	/*  94 */	{ VC_UNDEFINED,			0x00					},	// 0x5E	// TODO [KEY_MUHENKAN][0] == [?][1]
	/*  95 */	{ VC_KP_COMMA,			0x00					},	// 0x5F
	/*  96 */	{ VC_KP_ENTER,			0x00					},	// 0x60
	/*  97 */	{ VC_CONTROL_R,			0x00					},	// 0x61
	/*  98 */	{ VC_KP_DIVIDE,			0x00					},	// 0x62
	/*  99 */	{ VC_PRINTSCREEN,		KEY_F16					},	// 0x63
	/* 100 */	{ VC_ALT_R,				KEY_F17					},	// 0x64
	/* 101 */	{ VC_UNDEFINED,			KEY_F18					},	// 0x65	TODO [KEY_LINEFEED][0] == [?][1]
	/* 102 */	{ VC_HOME,				KEY_F19					},	// 0x66
	/* 103 */	{ VC_UP,				KEY_F20					},	// 0x67
	/* 104 */	{ VC_PAGE_UP,			KEY_F21					},	// 0x68
	/* 105 */	{ VC_LEFT,				KEY_F22					},	// 0x69
	/* 106 */	{ VC_RIGHT,				KEY_F23					},	// 0x6A
	/* 107 */	{ VC_END,				KEY_F24					},	// 0x6B
	/* 108 */	{ VC_DOWN,				0x00					},	// 0x6C
	/* 109 */	{ VC_PAGE_DOWN,			0x00					},	// 0x6D
	/* 110 */	{ VC_INSERT,			0x00					},	// 0x6E
	/* 111 */	{ VC_DELETE,			0x00					},	// 0x6F
	/* 112 */	{ VC_UNDEFINED,			KEY_KATAKANA  			},	// 0x70	TODO [KEY_MACRO][0] == VC_?
	/* 113 */	{ VC_VOLUME_MUTE,		0x00					},	// 0x71
	/* 114 */	{ VC_VOLUME_DOWN,		0x00					},	// 0x72
	/* 115 */	{ VC_VOLUME_UP,			0x00					},	// 0x73	TODO KEY_? = [VC_UNDERSCORE][1]
	/* 116 */	{ VC_POWER,				0x00					},	// 0x74	TODO KEY_? = [VC_FURIGANA][1]
	/* 117 */	{ VC_KP_EQUALS,			0x00					},	// 0x75
	/* 118 */	{ VC_UNDEFINED,			0x00					},	// 0x76	TODO [KEY_KPPLUSMINUS][0] = VC_?
	/* 119 */	{ VC_PAUSE,				0x00					},	// 0x77
	/* 120 */	{ VC_UNDEFINED,			0x00					},	// 0x78	TODO [KEY_SCALE][0] = VC_?
	/* 121 */	{ VC_UNDEFINED,			KEY_HENKAN				},	// 0x79	TODO [KEY_KPCOMMA][0] = VC_KP_COMMA?
	/* 122 */	{ VC_UNDEFINED,			0x00					},	// 0x7A
	/* 123 */	{ VC_UNDEFINED,			KEY_HIRAGANA			},	// 0x7B	FIXME [KEY_HANJA][0] = VC_HIRAGANA
	/* 124 */	{ VC_YEN,				0x00					},	// 0x7C
	/* 125 */	{ VC_META_L,			KEY_YEN					},	// 0x7D
	/* 126 */	{ VC_META_R,			KEY_KPJPCOMMA			},	// 0x7E
	/* 127 */	{ VC_CONTEXT_MENU,		0x00					},	// 0x7F

	//			No Offset				Offset (i & 0x00FF) + 128

	/* 128 */	{ VC_SUN_STOP,			0						},	// 0x86
	/* 129 */	{ VC_SUN_AGAIN,			0						},	// 0x87
	/* 130 */	{ VC_SUN_PROPS,			0						},	// 0x88
	/* 131 */	{ VC_SUN_UNDO,			0						},	// 0x89
	/* 132 */	{ VC_SUN_FRONT,			0						},	// 0x8A
	/* 133 */	{ VC_SUN_COPY,			0						},	// 0x8B
	/* 134 */	{ VC_SUN_OPEN,			0						},	// 0x8C
	/* 135 */	{ VC_SUN_INSERT,		0						},	// 0x8D
	/* 136 */	{ VC_SUN_FIND,			0						},	// 0x8E
	/* 137 */	{ VC_SUN_CUT,			0						},	// 0x8F
	/* 138 */	{ VC_SUN_HELP,			0						},	// 0x90
	/* 139 */	{ VC_UNDEFINED,			0						},	// 0x91
	/* 140 */	{ VC_APP_CALCULATOR,	0						},	// 0x92
	/* 141 */	{ VC_UNDEFINED,			KEY_KPEQUAL				},	// 0x93
	/* 142 */	{ VC_UNDEFINED,			0						},	// 0x94
	/* 143 */	{ VC_UNDEFINED,			0						},	// 0x95
	/* 144 */	{ VC_UNDEFINED,			0						},	// 0x96
	/* 145 */	{ VC_UNDEFINED,			0						},	// 0x97
	/* 146 */	{ VC_UNDEFINED,			0						},	// 0x98
	/* 147 */	{ VC_UNDEFINED,			0						},	// 0x99
	/* 148 */	{ VC_UNDEFINED,			0						},	// 0x9A
	/* 149 */	{ VC_UNDEFINED,			0						},	// 0x9B
	/* 150 */	{ VC_UNDEFINED,			0						},	// 0x9C
	/* 151 */	{ VC_UNDEFINED,			0						},	// 0x9D
	/* 152 */	{ VC_UNDEFINED,			0						},	// 0x9F
	/* 153 */	{ VC_UNDEFINED,			0						},	// 0xA0
	/* 154 */	{ VC_UNDEFINED,			0						},	// 0xA1
	/* 155 */	{ VC_UNDEFINED,			0						},	// 0xA2
	/* 156 */	{ VC_UNDEFINED,			KEY_KPENTER				},	// 0xA3
	/* 157 */	{ VC_UNDEFINED,			KEY_RIGHTCTRL			},	// 0xA4
	/* 158 */	{ VC_APP_MAIL,			0						},	// 0xA5
	/* 159 */	{ VC_MEDIA_PLAY,		0						},	// 0xA6
	/* 160 */	{ VC_UNDEFINED,			KEY_MUTE				},	// 0xA7
	/* 161 */	{ VC_UNDEFINED,			KEY_CALC				},	// 0xA8
	/* 162 */	{ VC_UNDEFINED,			KEY_FORWARD				},	// 0xA9
	/* 163 */	{ VC_UNDEFINED,			0						},	// 0xAA
	/* 164 */	{ VC_UNDEFINED,			0						},	// 0xAB
	/* 165 */	{ VC_UNDEFINED,			0						},	// 0xAC
	/* 166 */	{ VC_UNDEFINED,			0						},	// 0xAD
	/* 167 */	{ VC_UNDEFINED,			0						},	// 0xAE
	/* 168 */	{ VC_UNDEFINED,			0						},	// 0xAF
	/* 169 */	{ VC_UNDEFINED,			0						},	// 0xB0
	/* 170 */	{ VC_UNDEFINED,			0						},	// 0xB1
	/* 171 */	{ VC_UNDEFINED,			0						},	// 0xB2
	/* 172 */	{ VC_UNDEFINED,			0						},	// 0xB3
	/* 173 */	{ VC_UNDEFINED,			0						},	// 0xB4
	/* 174 */	{ VC_UNDEFINED,			KEY_VOLUMEDOWN			},	// 0xB5
	/* 175 */	{ VC_UNDEFINED,			KEY_SLEEP				},	// 0xB6
	/* 176 */	{ VC_UNDEFINED,			KEY_VOLUMEUP			},	// 0xB7
	/* 177 */	{ VC_BROWSER_HOME,		0x00					},	// 0xB8
	/* 178 */	{ VC_UNDEFINED,			KEY_SCROLLUP 			},	// 0xB9
	/* 179 */	{ VC_UNDEFINED,			0x00					},	// 0xBA
	/* 180 */	{ VC_UNDEFINED,			0x00					},	// 0xBB
	/* 181 */	{ VC_UNDEFINED,			KEY_KPSLASH 			},	// 0xBC
	/* 182 */	{ VC_UNDEFINED,			0x00					},	// 0xBD
	/* 183 */	{ VC_F13,				KEY_SYSRQ				},	// 0xBE
	/* 184 */	{ VC_F14,				KEY_RIGHTALT			},	// 0xBF
	/* 185 */	{ VC_F15,				0x00					},	// 0xC0
	/* 186 */	{ VC_F16,				0x00					},	// 0xC1
	/* 187 */	{ VC_F17,				0x00					},	// 0xC2
	/* 188 */	{ VC_F18,				0x00					},	// 0xC3
	/* 189 */	{ VC_F19,				0x00					},	// 0xC4
	/* 190 */	{ VC_F20,				0x00					},	// 0xC5
	/* 191 */	{ VC_F21,				0x00					},	// 0xC6
	/* 192 */	{ VC_F22,				0x00					},	// 0xC7
	/* 193 */	{ VC_F23,				0x00					},	// 0xC8
	/* 194 */	{ VC_F24,				0x00					},	// 0xC9
	/* 195 */	{ VC_UNDEFINED,			0x00					},	// 0xCA
	/* 196 */	{ VC_UNDEFINED,			0x00					},	// 0xCB
	/* 197 */	{ VC_UNDEFINED,			KEY_PAUSE				},	// 0xCC
	/* 198 */	{ VC_UNDEFINED,			0x00					},	// 0xCD
	/* 199 */	{ VC_UNDEFINED,			KEY_HOME				},	// 0xCE
	/* 200 */	{ VC_UNDEFINED,			KEY_UP					},	// 0xCF
	/* 201 */	{ VC_UNDEFINED,			KEY_PAGEUP 				},	// 0xD0
	/* 202 */	{ VC_UNDEFINED,			0x00					},	// 0xD1
	/* 203 */	{ VC_UNDEFINED,			KEY_LEFT				},	// 0xD2
	/* 204 */	{ VC_UNDEFINED,			0x00					},	// 0xD3
	/* 205 */	{ VC_UNDEFINED,			KEY_RIGHT				},	// 0xD4
	/* 206 */	{ VC_UNDEFINED,			0x00					},	// 0xD5
	/* 207 */	{ VC_UNDEFINED,			KEY_END					},	// 0xD6
	/* 208 */	{ VC_UNDEFINED,			KEY_DOWN				},	// 0xD7
	/* 209 */	{ VC_UNDEFINED,			KEY_PAGEDOWN			},	// 0xD8
	/* 210 */	{ VC_UNDEFINED,			KEY_INSERT				},	// 0xD9
	/* 211 */	{ VC_UNDEFINED,			KEY_DELETE 				},	// 0xDA
	/* 212 */	{ VC_UNDEFINED,			0x00					},	// 0xDB
	/* 213 */	{ VC_UNDEFINED,			0x00					},	// 0xDC
	/* 214 */	{ VC_UNDEFINED,			0x00					},	// 0xDD
	/* 215 */	{ VC_UNDEFINED,			0x00					},	// 0xDE
	/* 216 */	{ VC_UNDEFINED,			0x00					},	// 0xDF
	/* 217 */	{ VC_BROWSER_SEARCH,	0x00					},	// 0xE0
	/* 218 */	{ VC_UNDEFINED,			0x00					},	// 0xE1
	/* 219 */	{ VC_UNDEFINED,			KEY_LEFTMETA			},	// 0xE2
	/* 220 */	{ VC_UNDEFINED,			KEY_RIGHTMETA 			},	// 0xE3
	/* 221 */	{ VC_UNDEFINED,			KEY_COMPOSE				},	// 0xE4
	/* 222 */	{ VC_UNDEFINED,			KEY_POWER 				},	// 0xE5
	/* 223 */	{ VC_UNDEFINED,			0x00					},	// 0xE6
	/* 224 */	{ VC_UNDEFINED,			0x00					},	// 0xE7
	/* 225 */	{ VC_UNDEFINED,			0x00					},	// 0xE8
	/* 226 */	{ VC_UNDEFINED,			0x00					},	// 0xE9
	/* 227 */	{ VC_UNDEFINED,			0x00					},	// 0xEA
	/* 228 */	{ VC_UNDEFINED,			0x00					},	// 0xEB
	/* 229 */	{ VC_UNDEFINED,			KEY_SEARCH				},	// 0xEC
	/* 230 */	{ VC_UNDEFINED,			0x00					},	// 0xED
	/* 231 */	{ VC_UNDEFINED,			0x00					},	// 0xEE
	/* 232 */	{ VC_UNDEFINED,			0x00					},	// 0xF0
	/* 233 */	{ VC_UNDEFINED,			0x00					},	// 0xF1
	/* 234 */	{ VC_UNDEFINED,			0x00					},	// 0xF2
	/* 235 */	{ VC_UNDEFINED,			0x00					},	// 0xF3
	/* 236 */	{ VC_UNDEFINED,			KEY_BACK 				},	// 0xF4
	/* 237 */	{ VC_UNDEFINED,			0x00					},	// 0xF5
	/* 238 */	{ VC_UNDEFINED,			0x00					},	// 0xF6
	/* 239 */	{ VC_UNDEFINED,			0x00					},	// 0xF7
	/* 240 */	{ VC_UNDEFINED,			0x00					},	// 0xF8
	/* 241 */	{ VC_UNDEFINED,			0x00					},	// 0xF8
	/* 242 */	{ VC_UNDEFINED,			0x00					},	// 0xF9
	/* 243 */	{ VC_UNDEFINED,			0x00					},	// 0xFA
	/* 244 */	{ VC_UNDEFINED,			KEY_OPEN 				},	// 0xFB
	/* 245 */	{ VC_UNDEFINED,			KEY_HELP				},	// 0xFC
	/* 246 */	{ VC_UNDEFINED,			KEY_PROPS				},	// 0xFD
	/* 247 */	{ VC_UNDEFINED,			KEY_FRONT				},	// 0xFE
	/* 248 */	{ VC_UNDEFINED,			KEY_STOP 				},	// 0xFF
	/* 249 */	{ VC_UNDEFINED,			KEY_AGAIN 				},	// 0xF8
	/* 250 */	{ VC_UNDEFINED,			KEY_UNDO				},	// 0xF8
	/* 251 */	{ VC_UNDEFINED,			KEY_CUT					},	// 0xF8
	/* 252 */	{ VC_UNDEFINED,			KEY_COPY				},	// 0xF8
	/* 253 */	{ VC_UNDEFINED,			KEY_PASTE				},	// 0xF8
	/* 254 */	{ VC_UNDEFINED,			KEY_FIND				},	// 0xF8
	/* 255 */	{ VC_UNDEFINED,			0x00					},	// 0xF8
};
#endif


/* This table is generated based off the xfree86 -> scancode mapping above
 * and the keycode mappings in the following files:
 *		/usr/share/X11/xkb/keycodes/xfree86
 *
 * TODO Everything after 157 needs to be populated with scancodes for media
 * controls and internet keyboards.
 */
static const uint16_t xfree86_scancode_table[][2] = {
	/* idx		{ keycode,				scancode				}, */
	#ifndef __OPTIMIZE_SIZE__
	/*   0 */	{ VC_UNDEFINED,			  0						},
	/*   1 */	{ VC_UNDEFINED,			  1						},
	/*   2 */	{ VC_UNDEFINED,			  2						},
	/*   3 */	{ VC_UNDEFINED,			  3						},
	/*   4 */	{ VC_UNDEFINED,			  4						},
	/*   5 */	{ VC_UNDEFINED,			  5						},
	/*   6 */	{ VC_UNDEFINED,			  6						},
	/*   7 */	{ VC_UNDEFINED,			  7						},
	/*   8 */	{ VC_UNDEFINED,			  8						},
	/*   9 */	{ VC_ESCAPE,			  9						},
	/*  10 */	{ VC_1,					 10						},	// <AE01>
	/*  11 */	{ VC_2,					 11						},	// <AE02>
	/*  12 */	{ VC_3,					 12						},	// <AE03>
	/*  13 */	{ VC_4,					 13						},	// <AE04>
	/*  14 */	{ VC_5,					 14						},	// <AE05>
	/*  15 */	{ VC_6,					 15						},	// <AE06>
	/*  16 */	{ VC_7,					 16						},	// <AE07>
	/*  17 */	{ VC_8,					 17						},	// <AE08>
	/*  18 */	{ VC_9,					 18						},	// <AE09>
	/*  19 */	{ VC_0,					 19						},	// <AE10>
	/*  20 */	{ VC_MINUS,				 20						},	// <AE11>
	/*  21 */	{ VC_EQUALS,			 21						},	// <AE12>
	/*  22 */	{ VC_BACKSPACE,			 22						},	// <BKSP>
	/*  23 */	{ VC_TAB,				 23						},	// <TAB>
	/*  24 */	{ VC_Q,					 24						},	// <AD01>
	/*  25 */	{ VC_W,					 25						},	// <AD02>
	/*  26 */	{ VC_E,					 26						},	// <AD03>
	/*  27 */	{ VC_R,					 27						},	// <AD04>
	/*  28 */	{ VC_T,					 28						},	// <AD05>
	/*  29 */	{ VC_Y,					 29						},	// <AD06>
	/*  30 */	{ VC_U,					 30						},	// <AD07>
	/*  31 */	{ VC_I,					 31						},	// <AD08>
	/*  32 */	{ VC_O,					 32						},	// <AD09>
	/*  33 */	{ VC_P,					 33						},	// <AD10>
	/*  34 */	{ VC_OPEN_BRACKET,		 34						},	// <AD11>
	/*  35 */	{ VC_CLOSE_BRACKET,		 35						},	// <AD12>
	/*  36 */	{ VC_ENTER,				 36						},	// <RTRN>
	/*  37 */	{ VC_CONTROL_L,			 37						},	// <LCTL>
	/*  38 */	{ VC_A,					 38						},  // <AC01>
	/*  39 */	{ VC_S,					 39						},	// <AC02>
	/*  40 */	{ VC_D,					 40						},	// <AC03>
	/*  41 */	{ VC_F,					 41						},	// <AC04>
	/*  42 */	{ VC_G,					 42						},	// <AC05>
	/*  43 */	{ VC_H,					 43						},	// <AC06>
	/*  44 */	{ VC_J,					 44						},	// <AC07>
	/*  45 */	{ VC_K,					 45						},	// <AC08>
	/*  46 */	{ VC_L,					 46						},	// <AC09>
	/*  47 */	{ VC_SEMICOLON,			 47						},	// <AC10>
	/*  48 */	{ VC_QUOTE,				 48						},	// <AC11>
	/*  49 */	{ VC_BACKQUOTE,			 49						},	// <TLDE>
	/*  50 */	{ VC_SHIFT_L,			 50						},	// <LFSH>
	/*  51 */	{ VC_BACK_SLASH,		 51						},	// <BKSL>
	/*  52 */	{ VC_Z,					 52						},	// <AB01>
	/*  53 */	{ VC_X,					 53						},	// <AB02>
	/*  54 */	{ VC_C,					 54						},	// <AB03>
	/*  55 */	{ VC_V,					 55						},	// <AB04>
	/*  56 */	{ VC_B,					 56						},	// <AB05>
	/*  57 */	{ VC_N,					 57						},	// <AB06>
	/*  58 */	{ VC_M,					 58						},	// <AB07>
	/*  59 */	{ VC_COMMA,				 59						},	// <AB08>
	/*  60 */	{ VC_PERIOD,			 60						},	// <AB09>
	/*  61 */	{ VC_SLASH,				 61						},	// <AB10>
	/*  62 */	{ VC_SHIFT_R,			 62						},	// <RTSH>
	/*  63 */	{ VC_KP_MULTIPLY,		 63						},	// <KPMU>
	/*  64 */	{ VC_ALT_L,				 64						},	// <LALT>
	/*  65 */	{ VC_SPACE,				 65						},	// <SPCE>
	/*  66 */	{ VC_CAPS_LOCK,			 66						},	// <CAPS>
	/*  67 */	{ VC_F1,				 67						},	// <FK01>
	/*  68 */	{ VC_F2,				 68						},	// <FK02>
	/*  69 */	{ VC_F3,				 69						},	// <FK03>
	/*  70 */	{ VC_F4,				 70						},	// <FK04>
	/*  71 */	{ VC_F5,				 71						},	// <FK05>
	/*  72 */	{ VC_F6,				 72						},	// <FK06>
	/*  73 */	{ VC_F7,				 73						},	// <FK07>
	/*  74 */	{ VC_F8,				 74						},	// <FK08>
	/*  75 */	{ VC_F9,				 75						},	// <FK09>
	/*  76 */	{ VC_F10,				 76						},	// <FK10>
	/*  77 */	{ VC_NUM_LOCK,			 77						},	// <NMLK>
	/*  78 */	{ VC_SCROLL_LOCK,		 78						},	// <SCLK>
	/*  79 */	{ VC_KP_7,				 79						},	// <KP7>
	/*  80 */	{ VC_KP_8,				 80						},	// <KP8>
	/*  81 */	{ VC_KP_9,				 89						},	// <KP3>
	/*  82 */	{ VC_KP_SUBTRACT,		 82						},	// <KPSU>
	/*  83 */	{ VC_KP_4,				 83						},	// <KP4>
	/*  84 */	{ VC_KP_5,				 84						},	// <KP5>
	/*  85 */	{ VC_KP_6,				 85						},	// <KP6>
	/*  86 */	{ VC_KP_ADD,			 86						},	// <KPAD>
	/*  87 */	{ VC_KP_1,				 87						},	// <KP1>
	/*  88 */	{ VC_KP_2,				 88						},	// <KP2>
	#endif
	/*  89 */	{ VC_KP_3,				 81						},	// <KP9>
	/*  90 */	{ VC_KP_0,				 90						},	// <KP0>
	/*  91 */	{ VC_KP_SEPARATOR,		 91						},	// <KPDL>
	/*  92 */	{ VC_UNDEFINED,			  0						},	// <SYRQ>
	/*  93 */	{ VC_UNDEFINED,			  0						},
	/*  94 */	{ VC_UNDEFINED,			 94						},	// <LSGT>
	/*  95 */	{ VC_F11,				 95						},	// <FK11>
	/*  96 */	{ VC_HOME,				0						},
	/*  97 */	{ VC_UP,				0						},
	/*  98 */	{ VC_PAGE_UP,			0						},
	/*  99 */	{ VC_LEFT,				0						},
	/* 100 */	{ VC_KP_5,				0						},
	/* 101 */	{ VC_RIGHT,				0						},
	/* 102 */	{ VC_END,				0						},
	/* 103 */	{ VC_DOWN,				0						},
	/* 104 */	{ VC_PAGE_DOWN,			0						},
	/* 105 */	{ VC_INSERT,			0						},
	/* 106 */	{ VC_DELETE,			0						},
	/* 107 */	{ VC_KP_ENTER,			0						},
	/* 108 */	{ VC_CONTROL_R,			0						},
	/* 109 */	{ VC_PAUSE,				0						},
	/* 110 */	{ VC_PRINTSCREEN,		0						},
	/* 111 */	{ VC_SLASH,				0						},
	/* 112 */	{ VC_ALT_R,				0						},
	/* 113 */	{ VC_PAUSE,				0						},
	/* 114 */	{ VC_UNDEFINED,			0						},
	/* 115 */	{ VC_UNDEFINED,			0						},
	/* 116 */	{ VC_UNDEFINED,			0						},
	/* 117 */	{ VC_UNDEFINED,			0						},
	/* 118 */	{ VC_UNDEFINED,			0						},
	/* 119 */	{ VC_UNDEFINED,			0						},
	/* 120 */	{ VC_UNDEFINED,			0						},
	/* 121 */	{ VC_UNDEFINED,			0						},
	/* 122 */	{ VC_UNDEFINED,			0						},
	/* 123 */	{ VC_UNDEFINED,			0						},
	/* 124 */	{ VC_UNDEFINED,			0						},
	/* 125 */	{ VC_UNDEFINED,			0						},
	/* 126 */	{ VC_UNDEFINED,			0						},
	/* 127 */	{ VC_UNDEFINED,			0						},
	/* 128 */	{ VC_KANJI,				0						},
	/* 129 */	{ VC_UNDEFINED,			0						},
	/* 130 */	{ VC_HIRAGANA,			0						},
	/* 131 */	{ VC_UNDEFINED,			0						},
	/* 132 */	{ VC_YEN,				0						},
	/* 133 */	{ VC_UNDEFINED,			0						},
	/* 134 */	{ VC_UNDEFINED,			0						},
	/* 135 */	{ VC_KP_7,				0						},
	/* 136 */	{ VC_KP_8,				0						},
	/* 137 */	{ VC_KP_9,				0						},
	/* 138 */	{ VC_KP_4,				0						},
	/* 139 */	{ VC_KP_5,				0						},
	/* 140 */	{ VC_KP_6,				0						},
	/* 141 */	{ VC_KP_1,				0						},
	/* 142 */	{ VC_KP_2,				0						},
	/* 143 */	{ VC_KP_3,				0						},
	/* 144 */	{ VC_KP_0,				0						},
	/* 145 */	{ VC_KP_SEPARATOR,		0						},
	/* 146 */	{ VC_KP_7,				0						},
	/* 147 */	{ VC_KP_8,				0						},
	/* 148 */	{ VC_KP_9,				0						},
	/* 149 */	{ VC_KP_4,				0						},
	/* 150 */	{ VC_KP_5,				0						},
	/* 151 */	{ VC_KP_6,				0						},
	/* 152 */	{ VC_KP_1,				0						},
	/* 153 */	{ VC_KP_2,				0						},
	/* 154 */	{ VC_KP_3,				0						},
	/* 155 */	{ VC_KP_0,				0						},
	/* 156 */	{ VC_KP_SEPARATOR,		0						},
	#ifdef _UNUSED
	/* 156 */	{ VC_UNDEFINED,			0						},	// 158 I1E
	/* 156 */	{ VC_UNDEFINED,			0						},	// 159 I1F
	/* 160 */	{ VC_UNDEFINED,			0						},	// 160 I20
	/* 161 */	{ VC_UNDEFINED,			0						},	// 161 I21
	/* 162 */	{ VC_UNDEFINED,			0						},	// 162 I22
	/* 163 */	{ VC_UNDEFINED,			0						},	// 163 I23
	/* 164 */	{ VC_UNDEFINED,			0						},	// 164 I24
	/* 165 */	{ VC_UNDEFINED,			0						},	// 165 I25
	/* 166 */	{ VC_UNDEFINED,			0						},	// 166 I26
	/* 167 */	{ VC_UNDEFINED,			0						},	// 167 I27
	/* 168 */	{ VC_UNDEFINED,			0						},	// 168 I28
	/* 169 */	{ VC_UNDEFINED,			0						},	// 169 I29
	/* 170 */	{ VC_UNDEFINED,			0						},	// 170 I2A			<K5A>
	/* 171 */	{ VC_UNDEFINED,			0						},	// 171 I2B
	/* 172 */	{ VC_UNDEFINED,			0						},	// 172 I2C
	/* 173 */	{ VC_UNDEFINED,			0						},	// 173 I2D
	/* 174 */	{ VC_UNDEFINED,			0						},	// 174 I2E
	/* 175 */	{ VC_UNDEFINED,			0						},	// 175 I2F
	/* 176 */	{ VC_UNDEFINED,			0						},	// 176 I30
	/* 177 */	{ VC_UNDEFINED,			0						},	// 177 I31
	/* 178 */	{ VC_UNDEFINED,			0						},	// 178 I32
	/* 179 */	{ VC_UNDEFINED,			0						},	// 179 I33
	/* 180 */	{ VC_UNDEFINED,			0						},	// 180 I34
	/* 181 */	{ VC_UNDEFINED,			0						},	// 181 I35			<K5B>
	/* 182 */	{ VC_UNDEFINED,			0						},	// 182 I36			<K5D>
	/* 183 */	{ VC_UNDEFINED,			0						},	// 183 I37			<K5E>
	/* 184 */	{ VC_UNDEFINED,			0						},	// 184 I38			<K5F>
	/* 185 */	{ VC_UNDEFINED,			0						},	// 185 I39
	/* 186 */	{ VC_UNDEFINED,			0						},	// 186 I3A
	/* 187 */	{ VC_UNDEFINED,			0						},	// 187 I3B
	/* 188 */	{ VC_UNDEFINED,			0						},	// 188 I3C
	/* 189 */	{ VC_UNDEFINED,			0						},	// 189 I3D			<K62>
	/* 190 */	{ VC_UNDEFINED,			0						},	// 190 I3E			<K63>
	/* 191 */	{ VC_UNDEFINED,			0						},	// 191 I3F			<K64>
	/* 192 */	{ VC_UNDEFINED,			0						},	// 192 I40			<K65>
	/* 193 */	{ VC_UNDEFINED,			0						},	// 193 I41			<K66>
	/* 194 */	{ VC_UNDEFINED,			0						},	// 194 I42
	/* 195 */	{ VC_UNDEFINED,			0						},	// 195 I43
	/* 196 */	{ VC_UNDEFINED,			0						},	// 196 I44
	/* 197 */	{ VC_UNDEFINED,			0						},	// 197 I45
	/* 198 */	{ VC_UNDEFINED,			0						},	// 198 I46			<K67>
	/* 199 */	{ VC_UNDEFINED,			0						},	// 199 I47			<K68>
	/* 200 */	{ VC_UNDEFINED,			0						},	// 200 I48			<K69>
	/* 201 */	{ VC_UNDEFINED,			0						},	// 201 I49			<K6A>
	/* 202 */	{ VC_UNDEFINED,			0						},	// 202 I4A
	/* 203 */	{ VC_UNDEFINED,			0						},	// 203 I4B			<K6B>
	/* 204 */	{ VC_UNDEFINED,			0						},	// 204 I4C			<K6C>
	/* 205 */	{ VC_UNDEFINED,			0						},	// 205 I4D			<K6D>
	/* 206 */	{ VC_UNDEFINED,			0						},	// 206 I4E			<K6E>
	/* 207 */	{ VC_UNDEFINED,			0						},	// 207 I4F			<K6F>
	/* 208 */	{ VC_UNDEFINED,			0						},	// 208 I50			<K70>
	/* 209 */	{ VC_UNDEFINED,			0						},	// 209 I51			<K71>
	/* 210 */	{ VC_UNDEFINED,			0						},	// 210 I52			<K72>
	/* 211 */	{ VC_UNDEFINED,			0						},	// 211 I53			<K73>
	/* 212 */	{ VC_UNDEFINED,			0						},	// 212 I54
	/* 213 */	{ VC_UNDEFINED,			0						},	// 213 I55
	/* 214 */	{ VC_UNDEFINED,			0						},	// 214 I56
	/* 215 */	{ VC_UNDEFINED,			0						},	// 215 I57
	/* 216 */	{ VC_UNDEFINED,			0						},	// 216 I58
	/* 217 */	{ VC_UNDEFINED,			0						},	// 217 I59
	/* 218 */	{ VC_UNDEFINED,			0						},	// 218 I5A
	/* 219 */	{ VC_UNDEFINED,			0						},	// 219 I5B			<K74>
	/* 220 */	{ VC_UNDEFINED,			0						},	// 220 I5C			<K75>
	/* 221 */	{ VC_UNDEFINED,			0						},	// 221 I5D			<K76>
	/* 222 */	{ VC_UNDEFINED,			0						},	// 222 I5E
	/* 223 */	{ VC_UNDEFINED,			0						},	// 223 I5F
	/* 224 */	{ VC_UNDEFINED,			0						},	// 224 I60
	/* 225 */	{ VC_UNDEFINED,			0						},	// 225 I61
	/* 226 */	{ VC_UNDEFINED,			0						},	// 226 I62
	/* 227 */	{ VC_UNDEFINED,			0						},	// 227 I63
	/* 228 */	{ VC_UNDEFINED,			0						},	// 228 I64
	/* 229 */	{ VC_UNDEFINED,			0						},	// 229 I65
	/* 230 */	{ VC_UNDEFINED,			0						},	// 230 I66
	/* 231 */	{ VC_UNDEFINED,			0						},	// 231 I67
	/* 232 */	{ VC_UNDEFINED,			0						},	// 232 I68
	/* 233 */	{ VC_UNDEFINED,			0						},	// 233 I69
	/* 234 */	{ VC_UNDEFINED,			0						},	// 234 I6A
	/* 235 */	{ VC_UNDEFINED,			0						},	// 235 I6B
	/* 236 */	{ VC_UNDEFINED,			0						},	// 236 I6C
	/* 237 */	{ VC_UNDEFINED,			0						},	// 237 I6D
	/* 238 */	{ VC_UNDEFINED,			0						},	// 238 I6E
	/* 239 */	{ VC_UNDEFINED,			0						},	// 239 I6F
	/* 240 */	{ VC_UNDEFINED,			0						},	// 240 I70
	/* 241 */	{ VC_UNDEFINED,			0						},	// 241 I71
	/* 242 */	{ VC_UNDEFINED,			0						},	// 242 I72
	/* 243 */	{ VC_UNDEFINED,			0						},	// 243 I73
	/* 244 */	{ VC_UNDEFINED,			0						},	// 244 I74
	/* 245 */	{ VC_UNDEFINED,			0						},	// 245 I75
	/* 246 */	{ VC_UNDEFINED,			0						},	// 246 I76
	/* 247 */	{ VC_UNDEFINED,			0						},	// 247 I77
	/* 248 */	{ VC_UNDEFINED,			0						},	// 248 I78
	/* 249 */	{ VC_UNDEFINED,			0						},	// 249 I79
	/* 250 */	{ VC_UNDEFINED,			0						},	// 250 I7A
	/* 251 */	{ VC_UNDEFINED,			0						},	// 251 I7B
	/* 252 */	{ VC_UNDEFINED,			0						},	// 252 I7C
	/* 253 */	{ VC_UNDEFINED,			0						},	// 253 I7D
	/* 254 */	{ VC_UNDEFINED,			0						},	// 254 I7E
	/* 255 */	{ VC_UNDEFINED,			0						},	// 255 I7F
	#endif
 };


/***********************************************************************
 * The following table contains pairs of X11 keysym values for graphical
 * characters and the corresponding Unicode value. The function
 * keysym_to_unicode() maps a keysym onto a Unicode value using a binary
 * search, therefore keysym_unicode_table[] must remain SORTED by KeySym
 * value.
 *
 * We allow to represent any UCS character in the range U+00000000 to
 * U+00FFFFFF by a keysym value in the range 0x01000000 to 0x01FFFFFF.
 * This admittedly does not cover the entire 31-bit space of UCS, but
 * it does cover all of the characters up to U+10FFFF, which can be
 * represented by UTF-16, and more, and it is very unlikely that higher
 * UCS codes will ever be assigned by ISO. So to get Unicode character
 * U+ABCD you can directly use keysym 0x1000ABCD.
 *
 * NOTE: The comments in the table below contain the actual character
 * encoded in UTF-8, so for viewing and editing best use an editor in
 * UTF-8 mode.
 *
 * Author: Markus G. Kuhn <mkuhn@acm.org>, University of Cambridge,
 * June 1999
 *
 * Special thanks to Richard Verhoeven <river@win.tue.nl> for preparing
 * an initial draft of the mapping table.
 *
 * This table is in the public domain. Share and enjoy!
 ***********************************************************************/
static struct codepair {
  uint16_t keysym;
  uint16_t unicode;
} keysym_unicode_table[] = {
  { 0x01A1, 0x0104 }, /*                     Aogonek Ą LATIN CAPITAL LETTER A WITH OGONEK */
  { 0x01A2, 0x02D8 }, /*                       breve ˘ BREVE */
  { 0x01A3, 0x0141 }, /*                     Lstroke Ł LATIN CAPITAL LETTER L WITH STROKE */
  { 0x01A5, 0x013D }, /*                      Lcaron Ľ LATIN CAPITAL LETTER L WITH CARON */
  { 0x01A6, 0x015A }, /*                      Sacute Ś LATIN CAPITAL LETTER S WITH ACUTE */
  { 0x01A9, 0x0160 }, /*                      Scaron Š LATIN CAPITAL LETTER S WITH CARON */
  { 0x01AA, 0x015E }, /*                    Scedilla Ş LATIN CAPITAL LETTER S WITH CEDILLA */
  { 0x01AB, 0x0164 }, /*                      Tcaron Ť LATIN CAPITAL LETTER T WITH CARON */
  { 0x01AC, 0x0179 }, /*                      Zacute Ź LATIN CAPITAL LETTER Z WITH ACUTE */
  { 0x01AE, 0x017D }, /*                      Zcaron Ž LATIN CAPITAL LETTER Z WITH CARON */
  { 0x01AF, 0x017B }, /*                   Zabovedot Ż LATIN CAPITAL LETTER Z WITH DOT ABOVE */
  { 0x01B1, 0x0105 }, /*                     aogonek ą LATIN SMALL LETTER A WITH OGONEK */
  { 0x01B2, 0x02DB }, /*                      ogonek ˛ OGONEK */
  { 0x01B3, 0x0142 }, /*                     lstroke ł LATIN SMALL LETTER L WITH STROKE */
  { 0x01B5, 0x013E }, /*                      lcaron ľ LATIN SMALL LETTER L WITH CARON */
  { 0x01B6, 0x015B }, /*                      sacute ś LATIN SMALL LETTER S WITH ACUTE */
  { 0x01B7, 0x02C7 }, /*                       caron ˇ CARON */
  { 0x01B9, 0x0161 }, /*                      scaron š LATIN SMALL LETTER S WITH CARON */
  { 0x01BA, 0x015F }, /*                    scedilla ş LATIN SMALL LETTER S WITH CEDILLA */
  { 0x01BB, 0x0165 }, /*                      tcaron ť LATIN SMALL LETTER T WITH CARON */
  { 0x01BC, 0x017A }, /*                      zacute ź LATIN SMALL LETTER Z WITH ACUTE */
  { 0x01BD, 0x02DD }, /*                 doubleacute ˝ DOUBLE ACUTE ACCENT */
  { 0x01BE, 0x017E }, /*                      zcaron ž LATIN SMALL LETTER Z WITH CARON */
  { 0x01BF, 0x017C }, /*                   zabovedot ż LATIN SMALL LETTER Z WITH DOT ABOVE */
  { 0x01C0, 0x0154 }, /*                      Racute Ŕ LATIN CAPITAL LETTER R WITH ACUTE */
  { 0x01C3, 0x0102 }, /*                      Abreve Ă LATIN CAPITAL LETTER A WITH BREVE */
  { 0x01C5, 0x0139 }, /*                      Lacute Ĺ LATIN CAPITAL LETTER L WITH ACUTE */
  { 0x01C6, 0x0106 }, /*                      Cacute Ć LATIN CAPITAL LETTER C WITH ACUTE */
  { 0x01C8, 0x010C }, /*                      Ccaron Č LATIN CAPITAL LETTER C WITH CARON */
  { 0x01CA, 0x0118 }, /*                     Eogonek Ę LATIN CAPITAL LETTER E WITH OGONEK */
  { 0x01CC, 0x011A }, /*                      Ecaron Ě LATIN CAPITAL LETTER E WITH CARON */
  { 0x01CF, 0x010E }, /*                      Dcaron Ď LATIN CAPITAL LETTER D WITH CARON */
  { 0x01D0, 0x0110 }, /*                     Dstroke Đ LATIN CAPITAL LETTER D WITH STROKE */
  { 0x01D1, 0x0143 }, /*                      Nacute Ń LATIN CAPITAL LETTER N WITH ACUTE */
  { 0x01D2, 0x0147 }, /*                      Ncaron Ň LATIN CAPITAL LETTER N WITH CARON */
  { 0x01D5, 0x0150 }, /*                Odoubleacute Ő LATIN CAPITAL LETTER O WITH DOUBLE ACUTE */
  { 0x01D8, 0x0158 }, /*                      Rcaron Ř LATIN CAPITAL LETTER R WITH CARON */
  { 0x01D9, 0x016E }, /*                       Uring Ů LATIN CAPITAL LETTER U WITH RING ABOVE */
  { 0x01DB, 0x0170 }, /*                Udoubleacute Ű LATIN CAPITAL LETTER U WITH DOUBLE ACUTE */
  { 0x01DE, 0x0162 }, /*                    Tcedilla Ţ LATIN CAPITAL LETTER T WITH CEDILLA */
  { 0x01E0, 0x0155 }, /*                      racute ŕ LATIN SMALL LETTER R WITH ACUTE */
  { 0x01E3, 0x0103 }, /*                      abreve ă LATIN SMALL LETTER A WITH BREVE */
  { 0x01E5, 0x013A }, /*                      lacute ĺ LATIN SMALL LETTER L WITH ACUTE */
  { 0x01E6, 0x0107 }, /*                      cacute ć LATIN SMALL LETTER C WITH ACUTE */
  { 0x01E8, 0x010D }, /*                      ccaron č LATIN SMALL LETTER C WITH CARON */
  { 0x01EA, 0x0119 }, /*                     eogonek ę LATIN SMALL LETTER E WITH OGONEK */
  { 0x01EC, 0x011B }, /*                      ecaron ě LATIN SMALL LETTER E WITH CARON */
  { 0x01EF, 0x010F }, /*                      dcaron ď LATIN SMALL LETTER D WITH CARON */
  { 0x01F0, 0x0111 }, /*                     dstroke đ LATIN SMALL LETTER D WITH STROKE */
  { 0x01F1, 0x0144 }, /*                      nacute ń LATIN SMALL LETTER N WITH ACUTE */
  { 0x01F2, 0x0148 }, /*                      ncaron ň LATIN SMALL LETTER N WITH CARON */
  { 0x01F5, 0x0151 }, /*                odoubleacute ő LATIN SMALL LETTER O WITH DOUBLE ACUTE */
  { 0x01F8, 0x0159 }, /*                      rcaron ř LATIN SMALL LETTER R WITH CARON */
  { 0x01F9, 0x016F }, /*                       uring ů LATIN SMALL LETTER U WITH RING ABOVE */
  { 0x01FB, 0x0171 }, /*                udoubleacute ű LATIN SMALL LETTER U WITH DOUBLE ACUTE */
  { 0x01FE, 0x0163 }, /*                    tcedilla ţ LATIN SMALL LETTER T WITH CEDILLA */
  { 0x01FF, 0x02D9 }, /*                    abovedot ˙ DOT ABOVE */
  { 0x02A1, 0x0126 }, /*                     Hstroke Ħ LATIN CAPITAL LETTER H WITH STROKE */
  { 0x02A6, 0x0124 }, /*                 Hcircumflex Ĥ LATIN CAPITAL LETTER H WITH CIRCUMFLEX */
  { 0x02A9, 0x0130 }, /*                   Iabovedot İ LATIN CAPITAL LETTER I WITH DOT ABOVE */
  { 0x02AB, 0x011E }, /*                      Gbreve Ğ LATIN CAPITAL LETTER G WITH BREVE */
  { 0x02AC, 0x0134 }, /*                 Jcircumflex Ĵ LATIN CAPITAL LETTER J WITH CIRCUMFLEX */
  { 0x02B1, 0x0127 }, /*                     hstroke ħ LATIN SMALL LETTER H WITH STROKE */
  { 0x02B6, 0x0125 }, /*                 hcircumflex ĥ LATIN SMALL LETTER H WITH CIRCUMFLEX */
  { 0x02B9, 0x0131 }, /*                    idotless ı LATIN SMALL LETTER DOTLESS I */
  { 0x02BB, 0x011F }, /*                      gbreve ğ LATIN SMALL LETTER G WITH BREVE */
  { 0x02BC, 0x0135 }, /*                 jcircumflex ĵ LATIN SMALL LETTER J WITH CIRCUMFLEX */
  { 0x02C5, 0x010A }, /*                   Cabovedot Ċ LATIN CAPITAL LETTER C WITH DOT ABOVE */
  { 0x02C6, 0x0108 }, /*                 Ccircumflex Ĉ LATIN CAPITAL LETTER C WITH CIRCUMFLEX */
  { 0x02D5, 0x0120 }, /*                   Gabovedot Ġ LATIN CAPITAL LETTER G WITH DOT ABOVE */
  { 0x02D8, 0x011C }, /*                 Gcircumflex Ĝ LATIN CAPITAL LETTER G WITH CIRCUMFLEX */
  { 0x02DD, 0x016C }, /*                      Ubreve Ŭ LATIN CAPITAL LETTER U WITH BREVE */
  { 0x02DE, 0x015C }, /*                 Scircumflex Ŝ LATIN CAPITAL LETTER S WITH CIRCUMFLEX */
  { 0x02E5, 0x010B }, /*                   cabovedot ċ LATIN SMALL LETTER C WITH DOT ABOVE */
  { 0x02E6, 0x0109 }, /*                 ccircumflex ĉ LATIN SMALL LETTER C WITH CIRCUMFLEX */
  { 0x02F5, 0x0121 }, /*                   gabovedot ġ LATIN SMALL LETTER G WITH DOT ABOVE */
  { 0x02F8, 0x011D }, /*                 gcircumflex ĝ LATIN SMALL LETTER G WITH CIRCUMFLEX */
  { 0x02FD, 0x016D }, /*                      ubreve ŭ LATIN SMALL LETTER U WITH BREVE */
  { 0x02FE, 0x015D }, /*                 scircumflex ŝ LATIN SMALL LETTER S WITH CIRCUMFLEX */
  { 0x03A2, 0x0138 }, /*                         kra ĸ LATIN SMALL LETTER KRA */
  { 0x03A3, 0x0156 }, /*                    Rcedilla Ŗ LATIN CAPITAL LETTER R WITH CEDILLA */
  { 0x03A5, 0x0128 }, /*                      Itilde Ĩ LATIN CAPITAL LETTER I WITH TILDE */
  { 0x03A6, 0x013B }, /*                    Lcedilla Ļ LATIN CAPITAL LETTER L WITH CEDILLA */
  { 0x03AA, 0x0112 }, /*                     Emacron Ē LATIN CAPITAL LETTER E WITH MACRON */
  { 0x03AB, 0x0122 }, /*                    Gcedilla Ģ LATIN CAPITAL LETTER G WITH CEDILLA */
  { 0x03AC, 0x0166 }, /*                      Tslash Ŧ LATIN CAPITAL LETTER T WITH STROKE */
  { 0x03B3, 0x0157 }, /*                    rcedilla ŗ LATIN SMALL LETTER R WITH CEDILLA */
  { 0x03B5, 0x0129 }, /*                      itilde ĩ LATIN SMALL LETTER I WITH TILDE */
  { 0x03B6, 0x013C }, /*                    lcedilla ļ LATIN SMALL LETTER L WITH CEDILLA */
  { 0x03BA, 0x0113 }, /*                     emacron ē LATIN SMALL LETTER E WITH MACRON */
  { 0x03BB, 0x0123 }, /*                    gcedilla ģ LATIN SMALL LETTER G WITH CEDILLA */
  { 0x03BC, 0x0167 }, /*                      tslash ŧ LATIN SMALL LETTER T WITH STROKE */
  { 0x03BD, 0x014A }, /*                         ENG Ŋ LATIN CAPITAL LETTER ENG */
  { 0x03BF, 0x014B }, /*                         eng ŋ LATIN SMALL LETTER ENG */
  { 0x03C0, 0x0100 }, /*                     Amacron Ā LATIN CAPITAL LETTER A WITH MACRON */
  { 0x03C7, 0x012E }, /*                     Iogonek Į LATIN CAPITAL LETTER I WITH OGONEK */
  { 0x03CC, 0x0116 }, /*                   Eabovedot Ė LATIN CAPITAL LETTER E WITH DOT ABOVE */
  { 0x03CF, 0x012A }, /*                     Imacron Ī LATIN CAPITAL LETTER I WITH MACRON */
  { 0x03D1, 0x0145 }, /*                    Ncedilla Ņ LATIN CAPITAL LETTER N WITH CEDILLA */
  { 0x03D2, 0x014C }, /*                     Omacron Ō LATIN CAPITAL LETTER O WITH MACRON */
  { 0x03D3, 0x0136 }, /*                    Kcedilla Ķ LATIN CAPITAL LETTER K WITH CEDILLA */
  { 0x03D9, 0x0172 }, /*                     Uogonek Ų LATIN CAPITAL LETTER U WITH OGONEK */
  { 0x03DD, 0x0168 }, /*                      Utilde Ũ LATIN CAPITAL LETTER U WITH TILDE */
  { 0x03DE, 0x016A }, /*                     Umacron Ū LATIN CAPITAL LETTER U WITH MACRON */
  { 0x03E0, 0x0101 }, /*                     amacron ā LATIN SMALL LETTER A WITH MACRON */
  { 0x03E7, 0x012F }, /*                     iogonek į LATIN SMALL LETTER I WITH OGONEK */
  { 0x03EC, 0x0117 }, /*                   eabovedot ė LATIN SMALL LETTER E WITH DOT ABOVE */
  { 0x03EF, 0x012B }, /*                     imacron ī LATIN SMALL LETTER I WITH MACRON */
  { 0x03F1, 0x0146 }, /*                    ncedilla ņ LATIN SMALL LETTER N WITH CEDILLA */
  { 0x03F2, 0x014D }, /*                     omacron ō LATIN SMALL LETTER O WITH MACRON */
  { 0x03F3, 0x0137 }, /*                    kcedilla ķ LATIN SMALL LETTER K WITH CEDILLA */
  { 0x03F9, 0x0173 }, /*                     uogonek ų LATIN SMALL LETTER U WITH OGONEK */
  { 0x03FD, 0x0169 }, /*                      utilde ũ LATIN SMALL LETTER U WITH TILDE */
  { 0x03FE, 0x016B }, /*                     umacron ū LATIN SMALL LETTER U WITH MACRON */
  { 0x047E, 0x203E }, /*                    overline ‾ OVERLINE */
  { 0x04A1, 0x3002 }, /*               kana_fullstop 。 IDEOGRAPHIC FULL STOP */
  { 0x04A2, 0x300C }, /*         kana_openingbracket 「 LEFT CORNER BRACKET */
  { 0x04A3, 0x300D }, /*         kana_closingbracket 」 RIGHT CORNER BRACKET */
  { 0x04A4, 0x3001 }, /*                  kana_comma 、 IDEOGRAPHIC COMMA */
  { 0x04A5, 0x30FB }, /*            kana_conjunctive ・ KATAKANA MIDDLE DOT */
  { 0x04A6, 0x30F2 }, /*                     kana_WO ヲ KATAKANA LETTER WO */
  { 0x04A7, 0x30A1 }, /*                      kana_a ァ KATAKANA LETTER SMALL A */
  { 0x04A8, 0x30A3 }, /*                      kana_i ィ KATAKANA LETTER SMALL I */
  { 0x04A9, 0x30A5 }, /*                      kana_u ゥ KATAKANA LETTER SMALL U */
  { 0x04AA, 0x30A7 }, /*                      kana_e ェ KATAKANA LETTER SMALL E */
  { 0x04AB, 0x30A9 }, /*                      kana_o ォ KATAKANA LETTER SMALL O */
  { 0x04AC, 0x30E3 }, /*                     kana_ya ャ KATAKANA LETTER SMALL YA */
  { 0x04AD, 0x30E5 }, /*                     kana_yu ュ KATAKANA LETTER SMALL YU */
  { 0x04AE, 0x30E7 }, /*                     kana_yo ョ KATAKANA LETTER SMALL YO */
  { 0x04AF, 0x30C3 }, /*                    kana_tsu ッ KATAKANA LETTER SMALL TU */
  { 0x04B0, 0x30FC }, /*              prolongedsound ー KATAKANA-HIRAGANA PROLONGED SOUND MARK */
  { 0x04B1, 0x30A2 }, /*                      kana_A ア KATAKANA LETTER A */
  { 0x04B2, 0x30A4 }, /*                      kana_I イ KATAKANA LETTER I */
  { 0x04B3, 0x30A6 }, /*                      kana_U ウ KATAKANA LETTER U */
  { 0x04B4, 0x30A8 }, /*                      kana_E エ KATAKANA LETTER E */
  { 0x04B5, 0x30AA }, /*                      kana_O オ KATAKANA LETTER O */
  { 0x04B6, 0x30AB }, /*                     kana_KA カ KATAKANA LETTER KA */
  { 0x04B7, 0x30AD }, /*                     kana_KI キ KATAKANA LETTER KI */
  { 0x04B8, 0x30AF }, /*                     kana_KU ク KATAKANA LETTER KU */
  { 0x04B9, 0x30B1 }, /*                     kana_KE ケ KATAKANA LETTER KE */
  { 0x04BA, 0x30B3 }, /*                     kana_KO コ KATAKANA LETTER KO */
  { 0x04BB, 0x30B5 }, /*                     kana_SA サ KATAKANA LETTER SA */
  { 0x04BC, 0x30B7 }, /*                    kana_SHI シ KATAKANA LETTER SI */
  { 0x04BD, 0x30B9 }, /*                     kana_SU ス KATAKANA LETTER SU */
  { 0x04BE, 0x30BB }, /*                     kana_SE セ KATAKANA LETTER SE */
  { 0x04BF, 0x30BD }, /*                     kana_SO ソ KATAKANA LETTER SO */
  { 0x04C0, 0x30BF }, /*                     kana_TA タ KATAKANA LETTER TA */
  { 0x04C1, 0x30C1 }, /*                    kana_CHI チ KATAKANA LETTER TI */
  { 0x04C2, 0x30C4 }, /*                    kana_TSU ツ KATAKANA LETTER TU */
  { 0x04C3, 0x30C6 }, /*                     kana_TE テ KATAKANA LETTER TE */
  { 0x04C4, 0x30C8 }, /*                     kana_TO ト KATAKANA LETTER TO */
  { 0x04C5, 0x30CA }, /*                     kana_NA ナ KATAKANA LETTER NA */
  { 0x04C6, 0x30CB }, /*                     kana_NI ニ KATAKANA LETTER NI */
  { 0x04C7, 0x30CC }, /*                     kana_NU ヌ KATAKANA LETTER NU */
  { 0x04C8, 0x30CD }, /*                     kana_NE ネ KATAKANA LETTER NE */
  { 0x04C9, 0x30CE }, /*                     kana_NO ノ KATAKANA LETTER NO */
  { 0x04CA, 0x30CF }, /*                     kana_HA ハ KATAKANA LETTER HA */
  { 0x04CB, 0x30D2 }, /*                     kana_HI ヒ KATAKANA LETTER HI */
  { 0x04CC, 0x30D5 }, /*                     kana_FU フ KATAKANA LETTER HU */
  { 0x04CD, 0x30D8 }, /*                     kana_HE ヘ KATAKANA LETTER HE */
  { 0x04CE, 0x30DB }, /*                     kana_HO ホ KATAKANA LETTER HO */
  { 0x04CF, 0x30DE }, /*                     kana_MA マ KATAKANA LETTER MA */
  { 0x04D0, 0x30DF }, /*                     kana_MI ミ KATAKANA LETTER MI */
  { 0x04D1, 0x30E0 }, /*                     kana_MU ム KATAKANA LETTER MU */
  { 0x04D2, 0x30E1 }, /*                     kana_ME メ KATAKANA LETTER ME */
  { 0x04D3, 0x30E2 }, /*                     kana_MO モ KATAKANA LETTER MO */
  { 0x04D4, 0x30E4 }, /*                     kana_YA ヤ KATAKANA LETTER YA */
  { 0x04D5, 0x30E6 }, /*                     kana_YU ユ KATAKANA LETTER YU */
  { 0x04D6, 0x30E8 }, /*                     kana_YO ヨ KATAKANA LETTER YO */
  { 0x04D7, 0x30E9 }, /*                     kana_RA ラ KATAKANA LETTER RA */
  { 0x04D8, 0x30EA }, /*                     kana_RI リ KATAKANA LETTER RI */
  { 0x04D9, 0x30EB }, /*                     kana_RU ル KATAKANA LETTER RU */
  { 0x04DA, 0x30EC }, /*                     kana_RE レ KATAKANA LETTER RE */
  { 0x04DB, 0x30ED }, /*                     kana_RO ロ KATAKANA LETTER RO */
  { 0x04DC, 0x30EF }, /*                     kana_WA ワ KATAKANA LETTER WA */
  { 0x04DD, 0x30F3 }, /*                      kana_N ン KATAKANA LETTER N */
  { 0x04DE, 0x309B }, /*                 voicedsound ゛ KATAKANA-HIRAGANA VOICED SOUND MARK */
  { 0x04DF, 0x309C }, /*             semivoicedsound ゜ KATAKANA-HIRAGANA SEMI-VOICED SOUND MARK */
  { 0x05AC, 0x060C }, /*                Arabic_comma ، ARABIC COMMA */
  { 0x05BB, 0x061B }, /*            Arabic_semicolon ؛ ARABIC SEMICOLON */
  { 0x05BF, 0x061F }, /*        Arabic_question_mark ؟ ARABIC QUESTION MARK */
  { 0x05C1, 0x0621 }, /*                Arabic_hamza ء ARABIC LETTER HAMZA */
  { 0x05C2, 0x0622 }, /*          Arabic_maddaonalef آ ARABIC LETTER ALEF WITH MADDA ABOVE */
  { 0x05C3, 0x0623 }, /*          Arabic_hamzaonalef أ ARABIC LETTER ALEF WITH HAMZA ABOVE */
  { 0x05C4, 0x0624 }, /*           Arabic_hamzaonwaw ؤ ARABIC LETTER WAW WITH HAMZA ABOVE */
  { 0x05C5, 0x0625 }, /*       Arabic_hamzaunderalef إ ARABIC LETTER ALEF WITH HAMZA BELOW */
  { 0x05C6, 0x0626 }, /*           Arabic_hamzaonyeh ئ ARABIC LETTER YEH WITH HAMZA ABOVE */
  { 0x05C7, 0x0627 }, /*                 Arabic_alef ا ARABIC LETTER ALEF */
  { 0x05C8, 0x0628 }, /*                  Arabic_beh ب ARABIC LETTER BEH */
  { 0x05C9, 0x0629 }, /*           Arabic_tehmarbuta ة ARABIC LETTER TEH MARBUTA */
  { 0x05CA, 0x062A }, /*                  Arabic_teh ت ARABIC LETTER TEH */
  { 0x05CB, 0x062B }, /*                 Arabic_theh ث ARABIC LETTER THEH */
  { 0x05CC, 0x062C }, /*                 Arabic_jeem ج ARABIC LETTER JEEM */
  { 0x05CD, 0x062D }, /*                  Arabic_hah ح ARABIC LETTER HAH */
  { 0x05CE, 0x062E }, /*                 Arabic_khah خ ARABIC LETTER KHAH */
  { 0x05CF, 0x062F }, /*                  Arabic_dal د ARABIC LETTER DAL */
  { 0x05D0, 0x0630 }, /*                 Arabic_thal ذ ARABIC LETTER THAL */
  { 0x05D1, 0x0631 }, /*                   Arabic_ra ر ARABIC LETTER REH */
  { 0x05D2, 0x0632 }, /*                 Arabic_zain ز ARABIC LETTER ZAIN */
  { 0x05D3, 0x0633 }, /*                 Arabic_seen س ARABIC LETTER SEEN */
  { 0x05D4, 0x0634 }, /*                Arabic_sheen ش ARABIC LETTER SHEEN */
  { 0x05D5, 0x0635 }, /*                  Arabic_sad ص ARABIC LETTER SAD */
  { 0x05D6, 0x0636 }, /*                  Arabic_dad ض ARABIC LETTER DAD */
  { 0x05D7, 0x0637 }, /*                  Arabic_tah ط ARABIC LETTER TAH */
  { 0x05D8, 0x0638 }, /*                  Arabic_zah ظ ARABIC LETTER ZAH */
  { 0x05D9, 0x0639 }, /*                  Arabic_ain ع ARABIC LETTER AIN */
  { 0x05DA, 0x063A }, /*                Arabic_ghain غ ARABIC LETTER GHAIN */
  { 0x05E0, 0x0640 }, /*              Arabic_tatweel ـ ARABIC TATWEEL */
  { 0x05E1, 0x0641 }, /*                  Arabic_feh ف ARABIC LETTER FEH */
  { 0x05E2, 0x0642 }, /*                  Arabic_qaf ق ARABIC LETTER QAF */
  { 0x05E3, 0x0643 }, /*                  Arabic_kaf ك ARABIC LETTER KAF */
  { 0x05E4, 0x0644 }, /*                  Arabic_lam ل ARABIC LETTER LAM */
  { 0x05E5, 0x0645 }, /*                 Arabic_meem م ARABIC LETTER MEEM */
  { 0x05E6, 0x0646 }, /*                 Arabic_noon ن ARABIC LETTER NOON */
  { 0x05E7, 0x0647 }, /*                   Arabic_ha ه ARABIC LETTER HEH */
  { 0x05E8, 0x0648 }, /*                  Arabic_waw و ARABIC LETTER WAW */
  { 0x05E9, 0x0649 }, /*          Arabic_alefmaksura ى ARABIC LETTER ALEF MAKSURA */
  { 0x05EA, 0x064A }, /*                  Arabic_yeh ي ARABIC LETTER YEH */
  { 0x05EB, 0x064B }, /*             Arabic_fathatan ً ARABIC FATHATAN */
  { 0x05EC, 0x064C }, /*             Arabic_dammatan ٌ ARABIC DAMMATAN */
  { 0x05ED, 0x064D }, /*             Arabic_kasratan ٍ ARABIC KASRATAN */
  { 0x05EE, 0x064E }, /*                Arabic_fatha َ ARABIC FATHA */
  { 0x05EF, 0x064F }, /*                Arabic_damma ُ ARABIC DAMMA */
  { 0x05F0, 0x0650 }, /*                Arabic_kasra ِ ARABIC KASRA */
  { 0x05F1, 0x0651 }, /*               Arabic_shadda ّ ARABIC SHADDA */
  { 0x05F2, 0x0652 }, /*                Arabic_sukun ْ ARABIC SUKUN */
  { 0x06A1, 0x0452 }, /*                 Serbian_dje ђ CYRILLIC SMALL LETTER DJE */
  { 0x06A2, 0x0453 }, /*               Macedonia_gje ѓ CYRILLIC SMALL LETTER GJE */
  { 0x06A3, 0x0451 }, /*                 Cyrillic_io ё CYRILLIC SMALL LETTER IO */
  { 0x06A4, 0x0454 }, /*                Ukrainian_ie є CYRILLIC SMALL LETTER UKRAINIAN IE */
  { 0x06A5, 0x0455 }, /*               Macedonia_dse ѕ CYRILLIC SMALL LETTER DZE */
  { 0x06A6, 0x0456 }, /*                 Ukrainian_i і CYRILLIC SMALL LETTER BYELORUSSIAN-UKRAINIAN I */
  { 0x06A7, 0x0457 }, /*                Ukrainian_yi ї CYRILLIC SMALL LETTER YI */
  { 0x06A8, 0x0458 }, /*                 Cyrillic_je ј CYRILLIC SMALL LETTER JE */
  { 0x06A9, 0x0459 }, /*                Cyrillic_lje љ CYRILLIC SMALL LETTER LJE */
  { 0x06AA, 0x045A }, /*                Cyrillic_nje њ CYRILLIC SMALL LETTER NJE */
  { 0x06AB, 0x045B }, /*                Serbian_tshe ћ CYRILLIC SMALL LETTER TSHE */
  { 0x06AC, 0x045C }, /*               Macedonia_kje ќ CYRILLIC SMALL LETTER KJE */
  { 0x06AE, 0x045E }, /*         Byelorussian_shortu ў CYRILLIC SMALL LETTER SHORT U */
  { 0x06AF, 0x045F }, /*               Cyrillic_dzhe џ CYRILLIC SMALL LETTER DZHE */
  { 0x06B0, 0x2116 }, /*                  numerosign № NUMERO SIGN */
  { 0x06B1, 0x0402 }, /*                 Serbian_DJE Ђ CYRILLIC CAPITAL LETTER DJE */
  { 0x06B2, 0x0403 }, /*               Macedonia_GJE Ѓ CYRILLIC CAPITAL LETTER GJE */
  { 0x06B3, 0x0401 }, /*                 Cyrillic_IO Ё CYRILLIC CAPITAL LETTER IO */
  { 0x06B4, 0x0404 }, /*                Ukrainian_IE Є CYRILLIC CAPITAL LETTER UKRAINIAN IE */
  { 0x06B5, 0x0405 }, /*               Macedonia_DSE Ѕ CYRILLIC CAPITAL LETTER DZE */
  { 0x06B6, 0x0406 }, /*                 Ukrainian_I І CYRILLIC CAPITAL LETTER BYELORUSSIAN-UKRAINIAN I */
  { 0x06B7, 0x0407 }, /*                Ukrainian_YI Ї CYRILLIC CAPITAL LETTER YI */
  { 0x06B8, 0x0408 }, /*                 Cyrillic_JE Ј CYRILLIC CAPITAL LETTER JE */
  { 0x06B9, 0x0409 }, /*                Cyrillic_LJE Љ CYRILLIC CAPITAL LETTER LJE */
  { 0x06BA, 0x040A }, /*                Cyrillic_NJE Њ CYRILLIC CAPITAL LETTER NJE */
  { 0x06BB, 0x040B }, /*                Serbian_TSHE Ћ CYRILLIC CAPITAL LETTER TSHE */
  { 0x06BC, 0x040C }, /*               Macedonia_KJE Ќ CYRILLIC CAPITAL LETTER KJE */
  { 0x06BE, 0x040E }, /*         Byelorussian_SHORTU Ў CYRILLIC CAPITAL LETTER SHORT U */
  { 0x06BF, 0x040F }, /*               Cyrillic_DZHE Џ CYRILLIC CAPITAL LETTER DZHE */
  { 0x06C0, 0x044E }, /*                 Cyrillic_yu ю CYRILLIC SMALL LETTER YU */
  { 0x06C1, 0x0430 }, /*                  Cyrillic_a а CYRILLIC SMALL LETTER A */
  { 0x06C2, 0x0431 }, /*                 Cyrillic_be б CYRILLIC SMALL LETTER BE */
  { 0x06C3, 0x0446 }, /*                Cyrillic_tse ц CYRILLIC SMALL LETTER TSE */
  { 0x06C4, 0x0434 }, /*                 Cyrillic_de д CYRILLIC SMALL LETTER DE */
  { 0x06C5, 0x0435 }, /*                 Cyrillic_ie е CYRILLIC SMALL LETTER IE */
  { 0x06C6, 0x0444 }, /*                 Cyrillic_ef ф CYRILLIC SMALL LETTER EF */
  { 0x06C7, 0x0433 }, /*                Cyrillic_ghe г CYRILLIC SMALL LETTER GHE */
  { 0x06C8, 0x0445 }, /*                 Cyrillic_ha х CYRILLIC SMALL LETTER HA */
  { 0x06C9, 0x0438 }, /*                  Cyrillic_i и CYRILLIC SMALL LETTER I */
  { 0x06CA, 0x0439 }, /*             Cyrillic_shorti й CYRILLIC SMALL LETTER SHORT I */
  { 0x06CB, 0x043A }, /*                 Cyrillic_ka к CYRILLIC SMALL LETTER KA */
  { 0x06CC, 0x043B }, /*                 Cyrillic_el л CYRILLIC SMALL LETTER EL */
  { 0x06CD, 0x043C }, /*                 Cyrillic_em м CYRILLIC SMALL LETTER EM */
  { 0x06CE, 0x043D }, /*                 Cyrillic_en н CYRILLIC SMALL LETTER EN */
  { 0x06CF, 0x043E }, /*                  Cyrillic_o о CYRILLIC SMALL LETTER O */
  { 0x06D0, 0x043F }, /*                 Cyrillic_pe п CYRILLIC SMALL LETTER PE */
  { 0x06D1, 0x044F }, /*                 Cyrillic_ya я CYRILLIC SMALL LETTER YA */
  { 0x06D2, 0x0440 }, /*                 Cyrillic_er р CYRILLIC SMALL LETTER ER */
  { 0x06D3, 0x0441 }, /*                 Cyrillic_es с CYRILLIC SMALL LETTER ES */
  { 0x06D4, 0x0442 }, /*                 Cyrillic_te т CYRILLIC SMALL LETTER TE */
  { 0x06D5, 0x0443 }, /*                  Cyrillic_u у CYRILLIC SMALL LETTER U */
  { 0x06D6, 0x0436 }, /*                Cyrillic_zhe ж CYRILLIC SMALL LETTER ZHE */
  { 0x06D7, 0x0432 }, /*                 Cyrillic_ve в CYRILLIC SMALL LETTER VE */
  { 0x06D8, 0x044C }, /*           Cyrillic_softsign ь CYRILLIC SMALL LETTER SOFT SIGN */
  { 0x06D9, 0x044B }, /*               Cyrillic_yeru ы CYRILLIC SMALL LETTER YERU */
  { 0x06DA, 0x0437 }, /*                 Cyrillic_ze з CYRILLIC SMALL LETTER ZE */
  { 0x06DB, 0x0448 }, /*                Cyrillic_sha ш CYRILLIC SMALL LETTER SHA */
  { 0x06DC, 0x044D }, /*                  Cyrillic_e э CYRILLIC SMALL LETTER E */
  { 0x06DD, 0x0449 }, /*              Cyrillic_shcha щ CYRILLIC SMALL LETTER SHCHA */
  { 0x06DE, 0x0447 }, /*                Cyrillic_che ч CYRILLIC SMALL LETTER CHE */
  { 0x06DF, 0x044A }, /*           Cyrillic_hardsign ъ CYRILLIC SMALL LETTER HARD SIGN */
  { 0x06E0, 0x042E }, /*                 Cyrillic_YU Ю CYRILLIC CAPITAL LETTER YU */
  { 0x06E1, 0x0410 }, /*                  Cyrillic_A А CYRILLIC CAPITAL LETTER A */
  { 0x06E2, 0x0411 }, /*                 Cyrillic_BE Б CYRILLIC CAPITAL LETTER BE */
  { 0x06E3, 0x0426 }, /*                Cyrillic_TSE Ц CYRILLIC CAPITAL LETTER TSE */
  { 0x06E4, 0x0414 }, /*                 Cyrillic_DE Д CYRILLIC CAPITAL LETTER DE */
  { 0x06E5, 0x0415 }, /*                 Cyrillic_IE Е CYRILLIC CAPITAL LETTER IE */
  { 0x06E6, 0x0424 }, /*                 Cyrillic_EF Ф CYRILLIC CAPITAL LETTER EF */
  { 0x06E7, 0x0413 }, /*                Cyrillic_GHE Г CYRILLIC CAPITAL LETTER GHE */
  { 0x06E8, 0x0425 }, /*                 Cyrillic_HA Х CYRILLIC CAPITAL LETTER HA */
  { 0x06E9, 0x0418 }, /*                  Cyrillic_I И CYRILLIC CAPITAL LETTER I */
  { 0x06EA, 0x0419 }, /*             Cyrillic_SHORTI Й CYRILLIC CAPITAL LETTER SHORT I */
  { 0x06EB, 0x041A }, /*                 Cyrillic_KA К CYRILLIC CAPITAL LETTER KA */
  { 0x06EC, 0x041B }, /*                 Cyrillic_EL Л CYRILLIC CAPITAL LETTER EL */
  { 0x06ED, 0x041C }, /*                 Cyrillic_EM М CYRILLIC CAPITAL LETTER EM */
  { 0x06EE, 0x041D }, /*                 Cyrillic_EN Н CYRILLIC CAPITAL LETTER EN */
  { 0x06EF, 0x041E }, /*                  Cyrillic_O О CYRILLIC CAPITAL LETTER O */
  { 0x06F0, 0x041F }, /*                 Cyrillic_PE П CYRILLIC CAPITAL LETTER PE */
  { 0x06F1, 0x042F }, /*                 Cyrillic_YA Я CYRILLIC CAPITAL LETTER YA */
  { 0x06F2, 0x0420 }, /*                 Cyrillic_ER Р CYRILLIC CAPITAL LETTER ER */
  { 0x06F3, 0x0421 }, /*                 Cyrillic_ES С CYRILLIC CAPITAL LETTER ES */
  { 0x06F4, 0x0422 }, /*                 Cyrillic_TE Т CYRILLIC CAPITAL LETTER TE */
  { 0x06F5, 0x0423 }, /*                  Cyrillic_U У CYRILLIC CAPITAL LETTER U */
  { 0x06F6, 0x0416 }, /*                Cyrillic_ZHE Ж CYRILLIC CAPITAL LETTER ZHE */
  { 0x06F7, 0x0412 }, /*                 Cyrillic_VE В CYRILLIC CAPITAL LETTER VE */
  { 0x06F8, 0x042C }, /*           Cyrillic_SOFTSIGN Ь CYRILLIC CAPITAL LETTER SOFT SIGN */
  { 0x06F9, 0x042B }, /*               Cyrillic_YERU Ы CYRILLIC CAPITAL LETTER YERU */
  { 0x06FA, 0x0417 }, /*                 Cyrillic_ZE З CYRILLIC CAPITAL LETTER ZE */
  { 0x06FB, 0x0428 }, /*                Cyrillic_SHA Ш CYRILLIC CAPITAL LETTER SHA */
  { 0x06FC, 0x042D }, /*                  Cyrillic_E Э CYRILLIC CAPITAL LETTER E */
  { 0x06FD, 0x0429 }, /*              Cyrillic_SHCHA Щ CYRILLIC CAPITAL LETTER SHCHA */
  { 0x06FE, 0x0427 }, /*                Cyrillic_CHE Ч CYRILLIC CAPITAL LETTER CHE */
  { 0x06FF, 0x042A }, /*           Cyrillic_HARDSIGN Ъ CYRILLIC CAPITAL LETTER HARD SIGN */
  { 0x07A1, 0x0386 }, /*           Greek_ALPHAaccent Ά GREEK CAPITAL LETTER ALPHA WITH TONOS */
  { 0x07A2, 0x0388 }, /*         Greek_EPSILONaccent Έ GREEK CAPITAL LETTER EPSILON WITH TONOS */
  { 0x07A3, 0x0389 }, /*             Greek_ETAaccent Ή GREEK CAPITAL LETTER ETA WITH TONOS */
  { 0x07A4, 0x038A }, /*            Greek_IOTAaccent Ί GREEK CAPITAL LETTER IOTA WITH TONOS */
  { 0x07A5, 0x03AA }, /*         Greek_IOTAdiaeresis Ϊ GREEK CAPITAL LETTER IOTA WITH DIALYTIKA */
  { 0x07A7, 0x038C }, /*         Greek_OMICRONaccent Ό GREEK CAPITAL LETTER OMICRON WITH TONOS */
  { 0x07A8, 0x038E }, /*         Greek_UPSILONaccent Ύ GREEK CAPITAL LETTER UPSILON WITH TONOS */
  { 0x07A9, 0x03AB }, /*       Greek_UPSILONdieresis Ϋ GREEK CAPITAL LETTER UPSILON WITH DIALYTIKA */
  { 0x07AB, 0x038F }, /*           Greek_OMEGAaccent Ώ GREEK CAPITAL LETTER OMEGA WITH TONOS */
  { 0x07AE, 0x0385 }, /*        Greek_accentdieresis ΅ GREEK DIALYTIKA TONOS */
  { 0x07AF, 0x2015 }, /*              Greek_horizbar ― HORIZONTAL BAR */
  { 0x07B1, 0x03AC }, /*           Greek_alphaaccent ά GREEK SMALL LETTER ALPHA WITH TONOS */
  { 0x07B2, 0x03AD }, /*         Greek_epsilonaccent έ GREEK SMALL LETTER EPSILON WITH TONOS */
  { 0x07B3, 0x03AE }, /*             Greek_etaaccent ή GREEK SMALL LETTER ETA WITH TONOS */
  { 0x07B4, 0x03AF }, /*            Greek_iotaaccent ί GREEK SMALL LETTER IOTA WITH TONOS */
  { 0x07B5, 0x03CA }, /*          Greek_iotadieresis ϊ GREEK SMALL LETTER IOTA WITH DIALYTIKA */
  { 0x07B6, 0x0390 }, /*    Greek_iotaaccentdieresis ΐ GREEK SMALL LETTER IOTA WITH DIALYTIKA AND TONOS */
  { 0x07B7, 0x03CC }, /*         Greek_omicronaccent ό GREEK SMALL LETTER OMICRON WITH TONOS */
  { 0x07B8, 0x03CD }, /*         Greek_upsilonaccent ύ GREEK SMALL LETTER UPSILON WITH TONOS */
  { 0x07B9, 0x03CB }, /*       Greek_upsilondieresis ϋ GREEK SMALL LETTER UPSILON WITH DIALYTIKA */
  { 0x07BA, 0x03B0 }, /* Greek_upsilonaccentdieresis ΰ GREEK SMALL LETTER UPSILON WITH DIALYTIKA AND TONOS */
  { 0x07BB, 0x03CE }, /*           Greek_omegaaccent ώ GREEK SMALL LETTER OMEGA WITH TONOS */
  { 0x07C1, 0x0391 }, /*                 Greek_ALPHA Α GREEK CAPITAL LETTER ALPHA */
  { 0x07C2, 0x0392 }, /*                  Greek_BETA Β GREEK CAPITAL LETTER BETA */
  { 0x07C3, 0x0393 }, /*                 Greek_GAMMA Γ GREEK CAPITAL LETTER GAMMA */
  { 0x07C4, 0x0394 }, /*                 Greek_DELTA Δ GREEK CAPITAL LETTER DELTA */
  { 0x07C5, 0x0395 }, /*               Greek_EPSILON Ε GREEK CAPITAL LETTER EPSILON */
  { 0x07C6, 0x0396 }, /*                  Greek_ZETA Ζ GREEK CAPITAL LETTER ZETA */
  { 0x07C7, 0x0397 }, /*                   Greek_ETA Η GREEK CAPITAL LETTER ETA */
  { 0x07C8, 0x0398 }, /*                 Greek_THETA Θ GREEK CAPITAL LETTER THETA */
  { 0x07C9, 0x0399 }, /*                  Greek_IOTA Ι GREEK CAPITAL LETTER IOTA */
  { 0x07CA, 0x039A }, /*                 Greek_KAPPA Κ GREEK CAPITAL LETTER KAPPA */
  { 0x07CB, 0x039B }, /*                Greek_LAMBDA Λ GREEK CAPITAL LETTER LAMDA */
  { 0x07CC, 0x039C }, /*                    Greek_MU Μ GREEK CAPITAL LETTER MU */
  { 0x07CD, 0x039D }, /*                    Greek_NU Ν GREEK CAPITAL LETTER NU */
  { 0x07CE, 0x039E }, /*                    Greek_XI Ξ GREEK CAPITAL LETTER XI */
  { 0x07CF, 0x039F }, /*               Greek_OMICRON Ο GREEK CAPITAL LETTER OMICRON */
  { 0x07D0, 0x03A0 }, /*                    Greek_PI Π GREEK CAPITAL LETTER PI */
  { 0x07D1, 0x03A1 }, /*                   Greek_RHO Ρ GREEK CAPITAL LETTER RHO */
  { 0x07D2, 0x03A3 }, /*                 Greek_SIGMA Σ GREEK CAPITAL LETTER SIGMA */
  { 0x07D4, 0x03A4 }, /*                   Greek_TAU Τ GREEK CAPITAL LETTER TAU */
  { 0x07D5, 0x03A5 }, /*               Greek_UPSILON Υ GREEK CAPITAL LETTER UPSILON */
  { 0x07D6, 0x03A6 }, /*                   Greek_PHI Φ GREEK CAPITAL LETTER PHI */
  { 0x07D7, 0x03A7 }, /*                   Greek_CHI Χ GREEK CAPITAL LETTER CHI */
  { 0x07D8, 0x03A8 }, /*                   Greek_PSI Ψ GREEK CAPITAL LETTER PSI */
  { 0x07D9, 0x03A9 }, /*                 Greek_OMEGA Ω GREEK CAPITAL LETTER OMEGA */
  { 0x07E1, 0x03B1 }, /*                 Greek_alpha α GREEK SMALL LETTER ALPHA */
  { 0x07E2, 0x03B2 }, /*                  Greek_beta β GREEK SMALL LETTER BETA */
  { 0x07E3, 0x03B3 }, /*                 Greek_gamma γ GREEK SMALL LETTER GAMMA */
  { 0x07E4, 0x03B4 }, /*                 Greek_delta δ GREEK SMALL LETTER DELTA */
  { 0x07E5, 0x03B5 }, /*               Greek_epsilon ε GREEK SMALL LETTER EPSILON */
  { 0x07E6, 0x03B6 }, /*                  Greek_zeta ζ GREEK SMALL LETTER ZETA */
  { 0x07E7, 0x03B7 }, /*                   Greek_eta η GREEK SMALL LETTER ETA */
  { 0x07E8, 0x03B8 }, /*                 Greek_theta θ GREEK SMALL LETTER THETA */
  { 0x07E9, 0x03B9 }, /*                  Greek_iota ι GREEK SMALL LETTER IOTA */
  { 0x07EA, 0x03BA }, /*                 Greek_kappa κ GREEK SMALL LETTER KAPPA */
  { 0x07EB, 0x03BB }, /*                Greek_lambda λ GREEK SMALL LETTER LAMDA */
  { 0x07EC, 0x03BC }, /*                    Greek_mu μ GREEK SMALL LETTER MU */
  { 0x07ED, 0x03BD }, /*                    Greek_nu ν GREEK SMALL LETTER NU */
  { 0x07EE, 0x03BE }, /*                    Greek_xi ξ GREEK SMALL LETTER XI */
  { 0x07EF, 0x03BF }, /*               Greek_omicron ο GREEK SMALL LETTER OMICRON */
  { 0x07F0, 0x03C0 }, /*                    Greek_pi π GREEK SMALL LETTER PI */
  { 0x07F1, 0x03C1 }, /*                   Greek_rho ρ GREEK SMALL LETTER RHO */
  { 0x07F2, 0x03C3 }, /*                 Greek_sigma σ GREEK SMALL LETTER SIGMA */
  { 0x07F3, 0x03C2 }, /*       Greek_finalsmallsigma ς GREEK SMALL LETTER FINAL SIGMA */
  { 0x07F4, 0x03C4 }, /*                   Greek_tau τ GREEK SMALL LETTER TAU */
  { 0x07F5, 0x03C5 }, /*               Greek_upsilon υ GREEK SMALL LETTER UPSILON */
  { 0x07F6, 0x03C6 }, /*                   Greek_phi φ GREEK SMALL LETTER PHI */
  { 0x07F7, 0x03C7 }, /*                   Greek_chi χ GREEK SMALL LETTER CHI */
  { 0x07F8, 0x03C8 }, /*                   Greek_psi ψ GREEK SMALL LETTER PSI */
  { 0x07F9, 0x03C9 }, /*                 Greek_omega ω GREEK SMALL LETTER OMEGA */
  { 0x08A1, 0x23B7 }, /*                 leftradical ⎷ ??? */
  { 0x08A2, 0x250C }, /*              topleftradical ┌ BOX DRAWINGS LIGHT DOWN AND RIGHT */
  { 0x08A3, 0x2500 }, /*              horizconnector ─ BOX DRAWINGS LIGHT HORIZONTAL */
  { 0x08A4, 0x2320 }, /*                 topintegral ⌠ TOP HALF INTEGRAL */
  { 0x08A5, 0x2321 }, /*                 botintegral ⌡ BOTTOM HALF INTEGRAL */
  { 0x08A6, 0x2502 }, /*               vertconnector │ BOX DRAWINGS LIGHT VERTICAL */
  { 0x08A7, 0x23A1 }, /*            topleftsqbracket ⎡ ??? */
  { 0x08A8, 0x23A3 }, /*            botleftsqbracket ⎣ ??? */
  { 0x08A9, 0x23A4 }, /*           toprightsqbracket ⎤ ??? */
  { 0x08AA, 0x23A6 }, /*           botrightsqbracket ⎦ ??? */
  { 0x08AB, 0x239B }, /*               topleftparens ⎛ ??? */
  { 0x08AC, 0x239D }, /*               botleftparens ⎝ ??? */
  { 0x08AD, 0x239E }, /*              toprightparens ⎞ ??? */
  { 0x08AE, 0x23A0 }, /*              botrightparens ⎠ ??? */
  { 0x08AF, 0x23A8 }, /*        leftmiddlecurlybrace ⎨ ??? */
  { 0x08B0, 0x23AC }, /*       rightmiddlecurlybrace ⎬ ??? */
/*  0x08B1                          topleftsummation ? ??? */
/*  0x08B2                          botleftsummation ? ??? */
/*  0x08B3                 topvertsummationconnector ? ??? */
/*  0x08B4                 botvertsummationconnector ? ??? */
/*  0x08B5                         toprightsummation ? ??? */
/*  0x08B6                         botrightsummation ? ??? */
/*  0x08B7                      rightmiddlesummation ? ??? */
  { 0x08BC, 0x2264 }, /*               lessthanequal ≤ LESS-THAN OR EQUAL TO */
  { 0x08BD, 0x2260 }, /*                    notequal ≠ NOT EQUAL TO */
  { 0x08BE, 0x2265 }, /*            greaterthanequal ≥ GREATER-THAN OR EQUAL TO */
  { 0x08BF, 0x222B }, /*                    integral ∫ INTEGRAL */
  { 0x08C0, 0x2234 }, /*                   therefore ∴ THEREFORE */
  { 0x08C1, 0x221D }, /*                   variation ∝ PROPORTIONAL TO */
  { 0x08C2, 0x221E }, /*                    infinity ∞ INFINITY */
  { 0x08C5, 0x2207 }, /*                       nabla ∇ NABLA */
  { 0x08C8, 0x223C }, /*                 approximate ∼ TILDE OPERATOR */
  { 0x08C9, 0x2243 }, /*                similarequal ≃ ASYMPTOTICALLY EQUAL TO */
  { 0x08CD, 0x21D4 }, /*                    ifonlyif ⇔ LEFT RIGHT DOUBLE ARROW */
  { 0x08CE, 0x21D2 }, /*                     implies ⇒ RIGHTWARDS DOUBLE ARROW */
  { 0x08CF, 0x2261 }, /*                   identical ≡ IDENTICAL TO */
  { 0x08D6, 0x221A }, /*                     radical √ SQUARE ROOT */
  { 0x08DA, 0x2282 }, /*                  includedin ⊂ SUBSET OF */
  { 0x08DB, 0x2283 }, /*                    includes ⊃ SUPERSET OF */
  { 0x08DC, 0x2229 }, /*                intersection ∩ INTERSECTION */
  { 0x08DD, 0x222A }, /*                       union ∪ UNION */
  { 0x08DE, 0x2227 }, /*                  logicaland ∧ LOGICAL AND */
  { 0x08DF, 0x2228 }, /*                   logicalor ∨ LOGICAL OR */
  { 0x08EF, 0x2202 }, /*           partialderivative ∂ PARTIAL DIFFERENTIAL */
  { 0x08F6, 0x0192 }, /*                    function ƒ LATIN SMALL LETTER F WITH HOOK */
  { 0x08FB, 0x2190 }, /*                   leftarrow ← LEFTWARDS ARROW */
  { 0x08FC, 0x2191 }, /*                     uparrow ↑ UPWARDS ARROW */
  { 0x08FD, 0x2192 }, /*                  rightarrow → RIGHTWARDS ARROW */
  { 0x08FE, 0x2193 }, /*                   downarrow ↓ DOWNWARDS ARROW */
/*  0x09DF                                     blank ? ??? */
  { 0x09E0, 0x25C6 }, /*                soliddiamond ◆ BLACK DIAMOND */
  { 0x09E1, 0x2592 }, /*                checkerboard ▒ MEDIUM SHADE */
  { 0x09E2, 0x2409 }, /*                          ht ␉ SYMBOL FOR HORIZONTAL TABULATION */
  { 0x09E3, 0x240C }, /*                          ff ␌ SYMBOL FOR FORM FEED */
  { 0x09E4, 0x240D }, /*                          cr ␍ SYMBOL FOR CARRIAGE RETURN */
  { 0x09E5, 0x240A }, /*                          lf ␊ SYMBOL FOR LINE FEED */
  { 0x09E8, 0x2424 }, /*                          nl ␤ SYMBOL FOR NEWLINE */
  { 0x09E9, 0x240B }, /*                          vt ␋ SYMBOL FOR VERTICAL TABULATION */
  { 0x09EA, 0x2518 }, /*              lowrightcorner ┘ BOX DRAWINGS LIGHT UP AND LEFT */
  { 0x09EB, 0x2510 }, /*               uprightcorner ┐ BOX DRAWINGS LIGHT DOWN AND LEFT */
  { 0x09EC, 0x250C }, /*                upleftcorner ┌ BOX DRAWINGS LIGHT DOWN AND RIGHT */
  { 0x09ED, 0x2514 }, /*               lowleftcorner └ BOX DRAWINGS LIGHT UP AND RIGHT */
  { 0x09EE, 0x253C }, /*               crossinglines ┼ BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL */
  { 0x09EF, 0x23BA }, /*              horizlinescan1 ⎺ HORIZONTAL SCAN LINE-1 (Unicode 3.2 draft) */
  { 0x09F0, 0x23BB }, /*              horizlinescan3 ⎻ HORIZONTAL SCAN LINE-3 (Unicode 3.2 draft) */
  { 0x09F1, 0x2500 }, /*              horizlinescan5 ─ BOX DRAWINGS LIGHT HORIZONTAL */
  { 0x09F2, 0x23BC }, /*              horizlinescan7 ⎼ HORIZONTAL SCAN LINE-7 (Unicode 3.2 draft) */
  { 0x09F3, 0x23BD }, /*              horizlinescan9 ⎽ HORIZONTAL SCAN LINE-9 (Unicode 3.2 draft) */
  { 0x09F4, 0x251C }, /*                       leftt ├ BOX DRAWINGS LIGHT VERTICAL AND RIGHT */
  { 0x09F5, 0x2524 }, /*                      rightt ┤ BOX DRAWINGS LIGHT VERTICAL AND LEFT */
  { 0x09F6, 0x2534 }, /*                        bott ┴ BOX DRAWINGS LIGHT UP AND HORIZONTAL */
  { 0x09F7, 0x252C }, /*                        topt ┬ BOX DRAWINGS LIGHT DOWN AND HORIZONTAL */
  { 0x09F8, 0x2502 }, /*                     vertbar │ BOX DRAWINGS LIGHT VERTICAL */
  { 0x0AA1, 0x2003 }, /*                     emspace   EM SPACE */
  { 0x0AA2, 0x2002 }, /*                     enspace   EN SPACE */
  { 0x0AA3, 0x2004 }, /*                    em3space   THREE-PER-EM SPACE */
  { 0x0AA4, 0x2005 }, /*                    em4space   FOUR-PER-EM SPACE */
  { 0x0AA5, 0x2007 }, /*                  digitspace   FIGURE SPACE */
  { 0x0AA6, 0x2008 }, /*                  punctspace   PUNCTUATION SPACE */
  { 0x0AA7, 0x2009 }, /*                   thinspace   THIN SPACE */
  { 0x0AA8, 0x200A }, /*                   hairspace   HAIR SPACE */
  { 0x0AA9, 0x2014 }, /*                      emdash — EM DASH */
  { 0x0AAA, 0x2013 }, /*                      endash – EN DASH */
/*  0x0AAC                               signifblank ? ??? */
  { 0x0AAE, 0x2026 }, /*                    ellipsis … HORIZONTAL ELLIPSIS */
  { 0x0AAF, 0x2025 }, /*             doubbaselinedot ‥ TWO DOT LEADER */
  { 0x0AB0, 0x2153 }, /*                    onethird ⅓ VULGAR FRACTION ONE THIRD */
  { 0x0AB1, 0x2154 }, /*                   twothirds ⅔ VULGAR FRACTION TWO THIRDS */
  { 0x0AB2, 0x2155 }, /*                    onefifth ⅕ VULGAR FRACTION ONE FIFTH */
  { 0x0AB3, 0x2156 }, /*                   twofifths ⅖ VULGAR FRACTION TWO FIFTHS */
  { 0x0AB4, 0x2157 }, /*                 threefifths ⅗ VULGAR FRACTION THREE FIFTHS */
  { 0x0AB5, 0x2158 }, /*                  fourfifths ⅘ VULGAR FRACTION FOUR FIFTHS */
  { 0x0AB6, 0x2159 }, /*                    onesixth ⅙ VULGAR FRACTION ONE SIXTH */
  { 0x0AB7, 0x215A }, /*                  fivesixths ⅚ VULGAR FRACTION FIVE SIXTHS */
  { 0x0AB8, 0x2105 }, /*                      careof ℅ CARE OF */
  { 0x0ABB, 0x2012 }, /*                     figdash ‒ FIGURE DASH */
  { 0x0ABC, 0x2329 }, /*            leftanglebracket 〈 LEFT-POINTING ANGLE BRACKET */
/*  0x0ABD                              decimalpoint ? ??? */
  { 0x0ABE, 0x232A }, /*           rightanglebracket 〉 RIGHT-POINTING ANGLE BRACKET */
/*  0x0ABF                                    marker ? ??? */
  { 0x0AC3, 0x215B }, /*                   oneeighth ⅛ VULGAR FRACTION ONE EIGHTH */
  { 0x0AC4, 0x215C }, /*                threeeighths ⅜ VULGAR FRACTION THREE EIGHTHS */
  { 0x0AC5, 0x215D }, /*                 fiveeighths ⅝ VULGAR FRACTION FIVE EIGHTHS */
  { 0x0AC6, 0x215E }, /*                seveneighths ⅞ VULGAR FRACTION SEVEN EIGHTHS */
  { 0x0AC9, 0x2122 }, /*                   trademark ™ TRADE MARK SIGN */
  { 0x0ACA, 0x2613 }, /*               signaturemark ☓ SALTIRE */
/*  0x0ACB                         trademarkincircle ? ??? */
  { 0x0ACC, 0x25C1 }, /*            leftopentriangle ◁ WHITE LEFT-POINTING TRIANGLE */
  { 0x0ACD, 0x25B7 }, /*           rightopentriangle ▷ WHITE RIGHT-POINTING TRIANGLE */
  { 0x0ACE, 0x25CB }, /*                emopencircle ○ WHITE CIRCLE */
  { 0x0ACF, 0x25AF }, /*             emopenrectangle ▯ WHITE VERTICAL RECTANGLE */
  { 0x0AD0, 0x2018 }, /*         leftsinglequotemark ‘ LEFT SINGLE QUOTATION MARK */
  { 0x0AD1, 0x2019 }, /*        rightsinglequotemark ’ RIGHT SINGLE QUOTATION MARK */
  { 0x0AD2, 0x201C }, /*         leftdoublequotemark “ LEFT DOUBLE QUOTATION MARK */
  { 0x0AD3, 0x201D }, /*        rightdoublequotemark ” RIGHT DOUBLE QUOTATION MARK */
  { 0x0AD4, 0x211E }, /*                prescription ℞ PRESCRIPTION TAKE */
  { 0x0AD6, 0x2032 }, /*                     minutes ′ PRIME */
  { 0x0AD7, 0x2033 }, /*                     seconds ″ DOUBLE PRIME */
  { 0x0AD9, 0x271D }, /*                  latincross ✝ LATIN CROSS */
/*  0x0ADA                                  hexagram ? ??? */
  { 0x0ADB, 0x25AC }, /*            filledrectbullet ▬ BLACK RECTANGLE */
  { 0x0ADC, 0x25C0 }, /*         filledlefttribullet ◀ BLACK LEFT-POINTING TRIANGLE */
  { 0x0ADD, 0x25B6 }, /*        filledrighttribullet ▶ BLACK RIGHT-POINTING TRIANGLE */
  { 0x0ADE, 0x25CF }, /*              emfilledcircle ● BLACK CIRCLE */
  { 0x0ADF, 0x25AE }, /*                emfilledrect ▮ BLACK VERTICAL RECTANGLE */
  { 0x0AE0, 0x25E6 }, /*            enopencircbullet ◦ WHITE BULLET */
  { 0x0AE1, 0x25AB }, /*          enopensquarebullet ▫ WHITE SMALL SQUARE */
  { 0x0AE2, 0x25AD }, /*              openrectbullet ▭ WHITE RECTANGLE */
  { 0x0AE3, 0x25B3 }, /*             opentribulletup △ WHITE UP-POINTING TRIANGLE */
  { 0x0AE4, 0x25BD }, /*           opentribulletdown ▽ WHITE DOWN-POINTING TRIANGLE */
  { 0x0AE5, 0x2606 }, /*                    openstar ☆ WHITE STAR */
  { 0x0AE6, 0x2022 }, /*          enfilledcircbullet • BULLET */
  { 0x0AE7, 0x25AA }, /*            enfilledsqbullet ▪ BLACK SMALL SQUARE */
  { 0x0AE8, 0x25B2 }, /*           filledtribulletup ▲ BLACK UP-POINTING TRIANGLE */
  { 0x0AE9, 0x25BC }, /*         filledtribulletdown ▼ BLACK DOWN-POINTING TRIANGLE */
  { 0x0AEA, 0x261C }, /*                 leftpointer ☜ WHITE LEFT POINTING INDEX */
  { 0x0AEB, 0x261E }, /*                rightpointer ☞ WHITE RIGHT POINTING INDEX */
  { 0x0AEC, 0x2663 }, /*                        club ♣ BLACK CLUB SUIT */
  { 0x0AED, 0x2666 }, /*                     diamond ♦ BLACK DIAMOND SUIT */
  { 0x0AEE, 0x2665 }, /*                       heart ♥ BLACK HEART SUIT */
  { 0x0AF0, 0x2720 }, /*                maltesecross ✠ MALTESE CROSS */
  { 0x0AF1, 0x2020 }, /*                      dagger † DAGGER */
  { 0x0AF2, 0x2021 }, /*                doubledagger ‡ DOUBLE DAGGER */
  { 0x0AF3, 0x2713 }, /*                   checkmark ✓ CHECK MARK */
  { 0x0AF4, 0x2717 }, /*                 ballotcross ✗ BALLOT X */
  { 0x0AF5, 0x266F }, /*                musicalsharp ♯ MUSIC SHARP SIGN */
  { 0x0AF6, 0x266D }, /*                 musicalflat ♭ MUSIC FLAT SIGN */
  { 0x0AF7, 0x2642 }, /*                  malesymbol ♂ MALE SIGN */
  { 0x0AF8, 0x2640 }, /*                femalesymbol ♀ FEMALE SIGN */
  { 0x0AF9, 0x260E }, /*                   telephone ☎ BLACK TELEPHONE */
  { 0x0AFA, 0x2315 }, /*           telephonerecorder ⌕ TELEPHONE RECORDER */
  { 0x0AFB, 0x2117 }, /*         phonographcopyright ℗ SOUND RECORDING COPYRIGHT */
  { 0x0AFC, 0x2038 }, /*                       caret ‸ CARET */
  { 0x0AFD, 0x201A }, /*          singlelowquotemark ‚ SINGLE LOW-9 QUOTATION MARK */
  { 0x0AFE, 0x201E }, /*          doublelowquotemark „ DOUBLE LOW-9 QUOTATION MARK */
/*  0x0AFF                                    cursor ? ??? */
  { 0x0BA3, 0x003C }, /*                   leftcaret < LESS-THAN SIGN */
  { 0x0BA6, 0x003E }, /*                  rightcaret > GREATER-THAN SIGN */
  { 0x0BA8, 0x2228 }, /*                   downcaret ∨ LOGICAL OR */
  { 0x0BA9, 0x2227 }, /*                     upcaret ∧ LOGICAL AND */
  { 0x0BC0, 0x00AF }, /*                     overbar ¯ MACRON */
  { 0x0BC2, 0x22A5 }, /*                    downtack ⊥ UP TACK */
  { 0x0BC3, 0x2229 }, /*                      upshoe ∩ INTERSECTION */
  { 0x0BC4, 0x230A }, /*                   downstile ⌊ LEFT FLOOR */
  { 0x0BC6, 0x005F }, /*                    underbar _ LOW LINE */
  { 0x0BCA, 0x2218 }, /*                         jot ∘ RING OPERATOR */
  { 0x0BCC, 0x2395 }, /*                        quad ⎕ APL FUNCTIONAL SYMBOL QUAD */
  { 0x0BCE, 0x22A4 }, /*                      uptack ⊤ DOWN TACK */
  { 0x0BCF, 0x25CB }, /*                      circle ○ WHITE CIRCLE */
  { 0x0BD3, 0x2308 }, /*                     upstile ⌈ LEFT CEILING */
  { 0x0BD6, 0x222A }, /*                    downshoe ∪ UNION */
  { 0x0BD8, 0x2283 }, /*                   rightshoe ⊃ SUPERSET OF */
  { 0x0BDA, 0x2282 }, /*                    leftshoe ⊂ SUBSET OF */
  { 0x0BDC, 0x22A2 }, /*                    lefttack ⊢ RIGHT TACK */
  { 0x0BFC, 0x22A3 }, /*                   righttack ⊣ LEFT TACK */
  { 0x0CDF, 0x2017 }, /*        hebrew_doublelowline ‗ DOUBLE LOW LINE */
  { 0x0CE0, 0x05D0 }, /*                hebrew_aleph א HEBREW LETTER ALEF */
  { 0x0CE1, 0x05D1 }, /*                  hebrew_bet ב HEBREW LETTER BET */
  { 0x0CE2, 0x05D2 }, /*                hebrew_gimel ג HEBREW LETTER GIMEL */
  { 0x0CE3, 0x05D3 }, /*                hebrew_dalet ד HEBREW LETTER DALET */
  { 0x0CE4, 0x05D4 }, /*                   hebrew_he ה HEBREW LETTER HE */
  { 0x0CE5, 0x05D5 }, /*                  hebrew_waw ו HEBREW LETTER VAV */
  { 0x0CE6, 0x05D6 }, /*                 hebrew_zain ז HEBREW LETTER ZAYIN */
  { 0x0CE7, 0x05D7 }, /*                 hebrew_chet ח HEBREW LETTER HET */
  { 0x0CE8, 0x05D8 }, /*                  hebrew_tet ט HEBREW LETTER TET */
  { 0x0CE9, 0x05D9 }, /*                  hebrew_yod י HEBREW LETTER YOD */
  { 0x0CEA, 0x05DA }, /*            hebrew_finalkaph ך HEBREW LETTER FINAL KAF */
  { 0x0CEB, 0x05DB }, /*                 hebrew_kaph כ HEBREW LETTER KAF */
  { 0x0CEC, 0x05DC }, /*                hebrew_lamed ל HEBREW LETTER LAMED */
  { 0x0CED, 0x05DD }, /*             hebrew_finalmem ם HEBREW LETTER FINAL MEM */
  { 0x0CEE, 0x05DE }, /*                  hebrew_mem מ HEBREW LETTER MEM */
  { 0x0CEF, 0x05DF }, /*             hebrew_finalnun ן HEBREW LETTER FINAL NUN */
  { 0x0CF0, 0x05E0 }, /*                  hebrew_nun נ HEBREW LETTER NUN */
  { 0x0CF1, 0x05E1 }, /*               hebrew_samech ס HEBREW LETTER SAMEKH */
  { 0x0CF2, 0x05E2 }, /*                 hebrew_ayin ע HEBREW LETTER AYIN */
  { 0x0CF3, 0x05E3 }, /*              hebrew_finalpe ף HEBREW LETTER FINAL PE */
  { 0x0CF4, 0x05E4 }, /*                   hebrew_pe פ HEBREW LETTER PE */
  { 0x0CF5, 0x05E5 }, /*            hebrew_finalzade ץ HEBREW LETTER FINAL TSADI */
  { 0x0CF6, 0x05E6 }, /*                 hebrew_zade צ HEBREW LETTER TSADI */
  { 0x0CF7, 0x05E7 }, /*                 hebrew_qoph ק HEBREW LETTER QOF */
  { 0x0CF8, 0x05E8 }, /*                 hebrew_resh ר HEBREW LETTER RESH */
  { 0x0CF9, 0x05E9 }, /*                 hebrew_shin ש HEBREW LETTER SHIN */
  { 0x0CFA, 0x05EA }, /*                  hebrew_taw ת HEBREW LETTER TAV */
  { 0x0DA1, 0x0E01 }, /*                  Thai_kokai ก THAI CHARACTER KO KAI */
  { 0x0DA2, 0x0E02 }, /*                Thai_khokhai ข THAI CHARACTER KHO KHAI */
  { 0x0DA3, 0x0E03 }, /*               Thai_khokhuat ฃ THAI CHARACTER KHO KHUAT */
  { 0x0DA4, 0x0E04 }, /*               Thai_khokhwai ค THAI CHARACTER KHO KHWAI */
  { 0x0DA5, 0x0E05 }, /*                Thai_khokhon ฅ THAI CHARACTER KHO KHON */
  { 0x0DA6, 0x0E06 }, /*             Thai_khorakhang ฆ THAI CHARACTER KHO RAKHANG */
  { 0x0DA7, 0x0E07 }, /*                 Thai_ngongu ง THAI CHARACTER NGO NGU */
  { 0x0DA8, 0x0E08 }, /*                Thai_chochan จ THAI CHARACTER CHO CHAN */
  { 0x0DA9, 0x0E09 }, /*               Thai_choching ฉ THAI CHARACTER CHO CHING */
  { 0x0DAA, 0x0E0A }, /*               Thai_chochang ช THAI CHARACTER CHO CHANG */
  { 0x0DAB, 0x0E0B }, /*                   Thai_soso ซ THAI CHARACTER SO SO */
  { 0x0DAC, 0x0E0C }, /*                Thai_chochoe ฌ THAI CHARACTER CHO CHOE */
  { 0x0DAD, 0x0E0D }, /*                 Thai_yoying ญ THAI CHARACTER YO YING */
  { 0x0DAE, 0x0E0E }, /*                Thai_dochada ฎ THAI CHARACTER DO CHADA */
  { 0x0DAF, 0x0E0F }, /*                Thai_topatak ฏ THAI CHARACTER TO PATAK */
  { 0x0DB0, 0x0E10 }, /*                Thai_thothan ฐ THAI CHARACTER THO THAN */
  { 0x0DB1, 0x0E11 }, /*          Thai_thonangmontho ฑ THAI CHARACTER THO NANGMONTHO */
  { 0x0DB2, 0x0E12 }, /*             Thai_thophuthao ฒ THAI CHARACTER THO PHUTHAO */
  { 0x0DB3, 0x0E13 }, /*                  Thai_nonen ณ THAI CHARACTER NO NEN */
  { 0x0DB4, 0x0E14 }, /*                  Thai_dodek ด THAI CHARACTER DO DEK */
  { 0x0DB5, 0x0E15 }, /*                  Thai_totao ต THAI CHARACTER TO TAO */
  { 0x0DB6, 0x0E16 }, /*               Thai_thothung ถ THAI CHARACTER THO THUNG */
  { 0x0DB7, 0x0E17 }, /*              Thai_thothahan ท THAI CHARACTER THO THAHAN */
  { 0x0DB8, 0x0E18 }, /*               Thai_thothong ธ THAI CHARACTER THO THONG */
  { 0x0DB9, 0x0E19 }, /*                   Thai_nonu น THAI CHARACTER NO NU */
  { 0x0DBA, 0x0E1A }, /*               Thai_bobaimai บ THAI CHARACTER BO BAIMAI */
  { 0x0DBB, 0x0E1B }, /*                  Thai_popla ป THAI CHARACTER PO PLA */
  { 0x0DBC, 0x0E1C }, /*               Thai_phophung ผ THAI CHARACTER PHO PHUNG */
  { 0x0DBD, 0x0E1D }, /*                   Thai_fofa ฝ THAI CHARACTER FO FA */
  { 0x0DBE, 0x0E1E }, /*                Thai_phophan พ THAI CHARACTER PHO PHAN */
  { 0x0DBF, 0x0E1F }, /*                  Thai_fofan ฟ THAI CHARACTER FO FAN */
  { 0x0DC0, 0x0E20 }, /*             Thai_phosamphao ภ THAI CHARACTER PHO SAMPHAO */
  { 0x0DC1, 0x0E21 }, /*                   Thai_moma ม THAI CHARACTER MO MA */
  { 0x0DC2, 0x0E22 }, /*                  Thai_yoyak ย THAI CHARACTER YO YAK */
  { 0x0DC3, 0x0E23 }, /*                  Thai_rorua ร THAI CHARACTER RO RUA */
  { 0x0DC4, 0x0E24 }, /*                     Thai_ru ฤ THAI CHARACTER RU */
  { 0x0DC5, 0x0E25 }, /*                 Thai_loling ล THAI CHARACTER LO LING */
  { 0x0DC6, 0x0E26 }, /*                     Thai_lu ฦ THAI CHARACTER LU */
  { 0x0DC7, 0x0E27 }, /*                 Thai_wowaen ว THAI CHARACTER WO WAEN */
  { 0x0DC8, 0x0E28 }, /*                 Thai_sosala ศ THAI CHARACTER SO SALA */
  { 0x0DC9, 0x0E29 }, /*                 Thai_sorusi ษ THAI CHARACTER SO RUSI */
  { 0x0DCA, 0x0E2A }, /*                  Thai_sosua ส THAI CHARACTER SO SUA */
  { 0x0DCB, 0x0E2B }, /*                  Thai_hohip ห THAI CHARACTER HO HIP */
  { 0x0DCC, 0x0E2C }, /*                Thai_lochula ฬ THAI CHARACTER LO CHULA */
  { 0x0DCD, 0x0E2D }, /*                   Thai_oang อ THAI CHARACTER O ANG */
  { 0x0DCE, 0x0E2E }, /*               Thai_honokhuk ฮ THAI CHARACTER HO NOKHUK */
  { 0x0DCF, 0x0E2F }, /*              Thai_paiyannoi ฯ THAI CHARACTER PAIYANNOI */
  { 0x0DD0, 0x0E30 }, /*                  Thai_saraa ะ THAI CHARACTER SARA A */
  { 0x0DD1, 0x0E31 }, /*             Thai_maihanakat ั THAI CHARACTER MAI HAN-AKAT */
  { 0x0DD2, 0x0E32 }, /*                 Thai_saraaa า THAI CHARACTER SARA AA */
  { 0x0DD3, 0x0E33 }, /*                 Thai_saraam ำ THAI CHARACTER SARA AM */
  { 0x0DD4, 0x0E34 }, /*                  Thai_sarai ิ THAI CHARACTER SARA I */
  { 0x0DD5, 0x0E35 }, /*                 Thai_saraii ี THAI CHARACTER SARA II */
  { 0x0DD6, 0x0E36 }, /*                 Thai_saraue ึ THAI CHARACTER SARA UE */
  { 0x0DD7, 0x0E37 }, /*                Thai_sarauee ื THAI CHARACTER SARA UEE */
  { 0x0DD8, 0x0E38 }, /*                  Thai_sarau ุ THAI CHARACTER SARA U */
  { 0x0DD9, 0x0E39 }, /*                 Thai_sarauu ู THAI CHARACTER SARA UU */
  { 0x0DDA, 0x0E3A }, /*                Thai_phinthu ฺ THAI CHARACTER PHINTHU */
/*  0x0DDE                    Thai_maihanakat_maitho ? ??? */
  { 0x0DDF, 0x0E3F }, /*                   Thai_baht ฿ THAI CURRENCY SYMBOL BAHT */
  { 0x0DE0, 0x0E40 }, /*                  Thai_sarae เ THAI CHARACTER SARA E */
  { 0x0DE1, 0x0E41 }, /*                 Thai_saraae แ THAI CHARACTER SARA AE */
  { 0x0DE2, 0x0E42 }, /*                  Thai_sarao โ THAI CHARACTER SARA O */
  { 0x0DE3, 0x0E43 }, /*          Thai_saraaimaimuan ใ THAI CHARACTER SARA AI MAIMUAN */
  { 0x0DE4, 0x0E44 }, /*         Thai_saraaimaimalai ไ THAI CHARACTER SARA AI MAIMALAI */
  { 0x0DE5, 0x0E45 }, /*            Thai_lakkhangyao ๅ THAI CHARACTER LAKKHANGYAO */
  { 0x0DE6, 0x0E46 }, /*               Thai_maiyamok ๆ THAI CHARACTER MAIYAMOK */
  { 0x0DE7, 0x0E47 }, /*              Thai_maitaikhu ็ THAI CHARACTER MAITAIKHU */
  { 0x0DE8, 0x0E48 }, /*                  Thai_maiek ่ THAI CHARACTER MAI EK */
  { 0x0DE9, 0x0E49 }, /*                 Thai_maitho ้ THAI CHARACTER MAI THO */
  { 0x0DEA, 0x0E4A }, /*                 Thai_maitri ๊ THAI CHARACTER MAI TRI */
  { 0x0DEB, 0x0E4B }, /*            Thai_maichattawa ๋ THAI CHARACTER MAI CHATTAWA */
  { 0x0DEC, 0x0E4C }, /*            Thai_thanthakhat ์ THAI CHARACTER THANTHAKHAT */
  { 0x0DED, 0x0E4D }, /*               Thai_nikhahit ํ THAI CHARACTER NIKHAHIT */
  { 0x0DF0, 0x0E50 }, /*                 Thai_leksun ๐ THAI DIGIT ZERO */
  { 0x0DF1, 0x0E51 }, /*                Thai_leknung ๑ THAI DIGIT ONE */
  { 0x0DF2, 0x0E52 }, /*                Thai_leksong ๒ THAI DIGIT TWO */
  { 0x0DF3, 0x0E53 }, /*                 Thai_leksam ๓ THAI DIGIT THREE */
  { 0x0DF4, 0x0E54 }, /*                  Thai_leksi ๔ THAI DIGIT FOUR */
  { 0x0DF5, 0x0E55 }, /*                  Thai_lekha ๕ THAI DIGIT FIVE */
  { 0x0DF6, 0x0E56 }, /*                 Thai_lekhok ๖ THAI DIGIT SIX */
  { 0x0DF7, 0x0E57 }, /*                Thai_lekchet ๗ THAI DIGIT SEVEN */
  { 0x0DF8, 0x0E58 }, /*                Thai_lekpaet ๘ THAI DIGIT EIGHT */
  { 0x0DF9, 0x0E59 }, /*                 Thai_lekkao ๙ THAI DIGIT NINE */
  { 0x0EA1, 0x3131 }, /*               Hangul_Kiyeog ㄱ HANGUL LETTER KIYEOK */
  { 0x0EA2, 0x3132 }, /*          Hangul_SsangKiyeog ㄲ HANGUL LETTER SSANGKIYEOK */
  { 0x0EA3, 0x3133 }, /*           Hangul_KiyeogSios ㄳ HANGUL LETTER KIYEOK-SIOS */
  { 0x0EA4, 0x3134 }, /*                Hangul_Nieun ㄴ HANGUL LETTER NIEUN */
  { 0x0EA5, 0x3135 }, /*           Hangul_NieunJieuj ㄵ HANGUL LETTER NIEUN-CIEUC */
  { 0x0EA6, 0x3136 }, /*           Hangul_NieunHieuh ㄶ HANGUL LETTER NIEUN-HIEUH */
  { 0x0EA7, 0x3137 }, /*               Hangul_Dikeud ㄷ HANGUL LETTER TIKEUT */
  { 0x0EA8, 0x3138 }, /*          Hangul_SsangDikeud ㄸ HANGUL LETTER SSANGTIKEUT */
  { 0x0EA9, 0x3139 }, /*                Hangul_Rieul ㄹ HANGUL LETTER RIEUL */
  { 0x0EAA, 0x313A }, /*          Hangul_RieulKiyeog ㄺ HANGUL LETTER RIEUL-KIYEOK */
  { 0x0EAB, 0x313B }, /*           Hangul_RieulMieum ㄻ HANGUL LETTER RIEUL-MIEUM */
  { 0x0EAC, 0x313C }, /*           Hangul_RieulPieub ㄼ HANGUL LETTER RIEUL-PIEUP */
  { 0x0EAD, 0x313D }, /*            Hangul_RieulSios ㄽ HANGUL LETTER RIEUL-SIOS */
  { 0x0EAE, 0x313E }, /*           Hangul_RieulTieut ㄾ HANGUL LETTER RIEUL-THIEUTH */
  { 0x0EAF, 0x313F }, /*          Hangul_RieulPhieuf ㄿ HANGUL LETTER RIEUL-PHIEUPH */
  { 0x0EB0, 0x3140 }, /*           Hangul_RieulHieuh ㅀ HANGUL LETTER RIEUL-HIEUH */
  { 0x0EB1, 0x3141 }, /*                Hangul_Mieum ㅁ HANGUL LETTER MIEUM */
  { 0x0EB2, 0x3142 }, /*                Hangul_Pieub ㅂ HANGUL LETTER PIEUP */
  { 0x0EB3, 0x3143 }, /*           Hangul_SsangPieub ㅃ HANGUL LETTER SSANGPIEUP */
  { 0x0EB4, 0x3144 }, /*            Hangul_PieubSios ㅄ HANGUL LETTER PIEUP-SIOS */
  { 0x0EB5, 0x3145 }, /*                 Hangul_Sios ㅅ HANGUL LETTER SIOS */
  { 0x0EB6, 0x3146 }, /*            Hangul_SsangSios ㅆ HANGUL LETTER SSANGSIOS */
  { 0x0EB7, 0x3147 }, /*                Hangul_Ieung ㅇ HANGUL LETTER IEUNG */
  { 0x0EB8, 0x3148 }, /*                Hangul_Jieuj ㅈ HANGUL LETTER CIEUC */
  { 0x0EB9, 0x3149 }, /*           Hangul_SsangJieuj ㅉ HANGUL LETTER SSANGCIEUC */
  { 0x0EBA, 0x314A }, /*                Hangul_Cieuc ㅊ HANGUL LETTER CHIEUCH */
  { 0x0EBB, 0x314B }, /*               Hangul_Khieuq ㅋ HANGUL LETTER KHIEUKH */
  { 0x0EBC, 0x314C }, /*                Hangul_Tieut ㅌ HANGUL LETTER THIEUTH */
  { 0x0EBD, 0x314D }, /*               Hangul_Phieuf ㅍ HANGUL LETTER PHIEUPH */
  { 0x0EBE, 0x314E }, /*                Hangul_Hieuh ㅎ HANGUL LETTER HIEUH */
  { 0x0EBF, 0x314F }, /*                    Hangul_A ㅏ HANGUL LETTER A */
  { 0x0EC0, 0x3150 }, /*                   Hangul_AE ㅐ HANGUL LETTER AE */
  { 0x0EC1, 0x3151 }, /*                   Hangul_YA ㅑ HANGUL LETTER YA */
  { 0x0EC2, 0x3152 }, /*                  Hangul_YAE ㅒ HANGUL LETTER YAE */
  { 0x0EC3, 0x3153 }, /*                   Hangul_EO ㅓ HANGUL LETTER EO */
  { 0x0EC4, 0x3154 }, /*                    Hangul_E ㅔ HANGUL LETTER E */
  { 0x0EC5, 0x3155 }, /*                  Hangul_YEO ㅕ HANGUL LETTER YEO */
  { 0x0EC6, 0x3156 }, /*                   Hangul_YE ㅖ HANGUL LETTER YE */
  { 0x0EC7, 0x3157 }, /*                    Hangul_O ㅗ HANGUL LETTER O */
  { 0x0EC8, 0x3158 }, /*                   Hangul_WA ㅘ HANGUL LETTER WA */
  { 0x0EC9, 0x3159 }, /*                  Hangul_WAE ㅙ HANGUL LETTER WAE */
  { 0x0ECA, 0x315A }, /*                   Hangul_OE ㅚ HANGUL LETTER OE */
  { 0x0ECB, 0x315B }, /*                   Hangul_YO ㅛ HANGUL LETTER YO */
  { 0x0ECC, 0x315C }, /*                    Hangul_U ㅜ HANGUL LETTER U */
  { 0x0ECD, 0x315D }, /*                  Hangul_WEO ㅝ HANGUL LETTER WEO */
  { 0x0ECE, 0x315E }, /*                   Hangul_WE ㅞ HANGUL LETTER WE */
  { 0x0ECF, 0x315F }, /*                   Hangul_WI ㅟ HANGUL LETTER WI */
  { 0x0ED0, 0x3160 }, /*                   Hangul_YU ㅠ HANGUL LETTER YU */
  { 0x0ED1, 0x3161 }, /*                   Hangul_EU ㅡ HANGUL LETTER EU */
  { 0x0ED2, 0x3162 }, /*                   Hangul_YI ㅢ HANGUL LETTER YI */
  { 0x0ED3, 0x3163 }, /*                    Hangul_I ㅣ HANGUL LETTER I */
  { 0x0ED4, 0x11A8 }, /*             Hangul_J_Kiyeog ᆨ HANGUL JONGSEONG KIYEOK */
  { 0x0ED5, 0x11A9 }, /*        Hangul_J_SsangKiyeog ᆩ HANGUL JONGSEONG SSANGKIYEOK */
  { 0x0ED6, 0x11AA }, /*         Hangul_J_KiyeogSios ᆪ HANGUL JONGSEONG KIYEOK-SIOS */
  { 0x0ED7, 0x11AB }, /*              Hangul_J_Nieun ᆫ HANGUL JONGSEONG NIEUN */
  { 0x0ED8, 0x11AC }, /*         Hangul_J_NieunJieuj ᆬ HANGUL JONGSEONG NIEUN-CIEUC */
  { 0x0ED9, 0x11AD }, /*         Hangul_J_NieunHieuh ᆭ HANGUL JONGSEONG NIEUN-HIEUH */
  { 0x0EDA, 0x11AE }, /*             Hangul_J_Dikeud ᆮ HANGUL JONGSEONG TIKEUT */
  { 0x0EDB, 0x11AF }, /*              Hangul_J_Rieul ᆯ HANGUL JONGSEONG RIEUL */
  { 0x0EDC, 0x11B0 }, /*        Hangul_J_RieulKiyeog ᆰ HANGUL JONGSEONG RIEUL-KIYEOK */
  { 0x0EDD, 0x11B1 }, /*         Hangul_J_RieulMieum ᆱ HANGUL JONGSEONG RIEUL-MIEUM */
  { 0x0EDE, 0x11B2 }, /*         Hangul_J_RieulPieub ᆲ HANGUL JONGSEONG RIEUL-PIEUP */
  { 0x0EDF, 0x11B3 }, /*          Hangul_J_RieulSios ᆳ HANGUL JONGSEONG RIEUL-SIOS */
  { 0x0EE0, 0x11B4 }, /*         Hangul_J_RieulTieut ᆴ HANGUL JONGSEONG RIEUL-THIEUTH */
  { 0x0EE1, 0x11B5 }, /*        Hangul_J_RieulPhieuf ᆵ HANGUL JONGSEONG RIEUL-PHIEUPH */
  { 0x0EE2, 0x11B6 }, /*         Hangul_J_RieulHieuh ᆶ HANGUL JONGSEONG RIEUL-HIEUH */
  { 0x0EE3, 0x11B7 }, /*              Hangul_J_Mieum ᆷ HANGUL JONGSEONG MIEUM */
  { 0x0EE4, 0x11B8 }, /*              Hangul_J_Pieub ᆸ HANGUL JONGSEONG PIEUP */
  { 0x0EE5, 0x11B9 }, /*          Hangul_J_PieubSios ᆹ HANGUL JONGSEONG PIEUP-SIOS */
  { 0x0EE6, 0x11BA }, /*               Hangul_J_Sios ᆺ HANGUL JONGSEONG SIOS */
  { 0x0EE7, 0x11BB }, /*          Hangul_J_SsangSios ᆻ HANGUL JONGSEONG SSANGSIOS */
  { 0x0EE8, 0x11BC }, /*              Hangul_J_Ieung ᆼ HANGUL JONGSEONG IEUNG */
  { 0x0EE9, 0x11BD }, /*              Hangul_J_Jieuj ᆽ HANGUL JONGSEONG CIEUC */
  { 0x0EEA, 0x11BE }, /*              Hangul_J_Cieuc ᆾ HANGUL JONGSEONG CHIEUCH */
  { 0x0EEB, 0x11BF }, /*             Hangul_J_Khieuq ᆿ HANGUL JONGSEONG KHIEUKH */
  { 0x0EEC, 0x11C0 }, /*              Hangul_J_Tieut ᇀ HANGUL JONGSEONG THIEUTH */
  { 0x0EED, 0x11C1 }, /*             Hangul_J_Phieuf ᇁ HANGUL JONGSEONG PHIEUPH */
  { 0x0EEE, 0x11C2 }, /*              Hangul_J_Hieuh ᇂ HANGUL JONGSEONG HIEUH */
  { 0x0EEF, 0x316D }, /*     Hangul_RieulYeorinHieuh ㅭ HANGUL LETTER RIEUL-YEORINHIEUH */
  { 0x0EF0, 0x3171 }, /*    Hangul_SunkyeongeumMieum ㅱ HANGUL LETTER KAPYEOUNMIEUM */
  { 0x0EF1, 0x3178 }, /*    Hangul_SunkyeongeumPieub ㅸ HANGUL LETTER KAPYEOUNPIEUP */
  { 0x0EF2, 0x317F }, /*              Hangul_PanSios ㅿ HANGUL LETTER PANSIOS */
  { 0x0EF3, 0x3181 }, /*    Hangul_KkogjiDalrinIeung ㆁ HANGUL LETTER YESIEUNG */
  { 0x0EF4, 0x3184 }, /*   Hangul_SunkyeongeumPhieuf ㆄ HANGUL LETTER KAPYEOUNPHIEUPH */
  { 0x0EF5, 0x3186 }, /*          Hangul_YeorinHieuh ㆆ HANGUL LETTER YEORINHIEUH */
  { 0x0EF6, 0x318D }, /*                Hangul_AraeA ㆍ HANGUL LETTER ARAEA */
  { 0x0EF7, 0x318E }, /*               Hangul_AraeAE ㆎ HANGUL LETTER ARAEAE */
  { 0x0EF8, 0x11EB }, /*            Hangul_J_PanSios ᇫ HANGUL JONGSEONG PANSIOS */
  { 0x0EF9, 0x11F0 }, /*  Hangul_J_KkogjiDalrinIeung ᇰ HANGUL JONGSEONG YESIEUNG */
  { 0x0EFA, 0x11F9 }, /*        Hangul_J_YeorinHieuh ᇹ HANGUL JONGSEONG YEORINHIEUH */
  { 0x0EFF, 0x20A9 }, /*                  Korean_Won ₩ WON SIGN */
  { 0x13A4, 0x20AC }, /*                        Euro € EURO SIGN */
  { 0x13BC, 0x0152 }, /*                          OE Œ LATIN CAPITAL LIGATURE OE */
  { 0x13BD, 0x0153 }, /*                          oe œ LATIN SMALL LIGATURE OE */
  { 0x13BE, 0x0178 }, /*                  Ydiaeresis Ÿ LATIN CAPITAL LETTER Y WITH DIAERESIS */
  { 0x20AC, 0x20AC }, /*                    EuroSign € EURO SIGN */
};

/***********************************************************************
 * The following function converts ISO 10646-1 (UCS, Unicode) values to
 * their corresponding KeySym values.
 *
 * The UTF-8 -> keysym conversion will hopefully one day be provided by
 * Xlib via XmbLookupString() and should ideally not have to be
 * done in X applications. But we are not there yet.
 *
 * Author: Markus G. Kuhn <mkuhn@acm.org>, University of Cambridge,
 * June 1999
 *
 * Special thanks to Richard Verhoeven <river@win.tue.nl> for preparing
 * an initial draft of the mapping table.
 *
 * This software is in the public domain. Share and enjoy!
 ***********************************************************************/
KeySym unicode_to_keysym(wchar_t ucs) {
	int min = 0;
	int max = sizeof(keysym_unicode_table) / sizeof(struct codepair) - 1;
	int mid;

	#ifdef XK_LATIN1
	// First check for Latin-1 characters. (1:1 mapping)
	if ((ucs >= 0x0020 && ucs <= 0x007E) ||
		(ucs >= 0x00A0 && ucs <= 0x00FF)) {
		return ucs;
	}
	#endif

	// Binary search the lookup table.
	while (max >= min) {
		mid = (min + max) / 2;
		if (keysym_unicode_table[mid].unicode < ucs) {
			min = mid + 1;
		}
		else if (keysym_unicode_table[mid].unicode > ucs) {
			max = mid - 1;
		}
		else {
			// Found it.
			return keysym_unicode_table[mid].keysym;
		}
	}

	// No matching KeySym value found, return UCS2 with bit set.
	return ucs | 0x01000000;
}

/***********************************************************************
 * The following function converts KeySym values into the corresponding
 * ISO 10646-1 (UCS, Unicode) values.
 *
 * The keysym -> UTF-8 conversion will hopefully one day be provided by
 * Xlib via XLookupKeySym and should ideally not have to be done in X
 * applications. But we are not there yet.
 *
 * Author: Markus G. Kuhn <mkuhn@acm.org>, University of Cambridge,
 * June 1999
 *
 * Special thanks to Richard Verhoeven <river@win.tue.nl> for preparing
 * an initial draft of the mapping table.
 *
 * This software is in the public domain. Share and enjoy!
 ***********************************************************************/
wchar_t keysym_to_unicode(KeySym keysym) {
	int min = 0;
	int max = sizeof(keysym_unicode_table) / sizeof(struct codepair) - 1;
	int mid;

	#ifdef XK_LATIN1
    // First check for Latin-1 characters. (1:1 mapping)
	if ((keysym >= 0x0020 && keysym <= 0x007E) ||
		(keysym >= 0x00A0 && keysym <= 0x00FF)) {
		return keysym;
	}
	#endif

	// Also check for directly encoded 24-bit UCS characters.
	#if defined(XK_LATIN8) || defined(XK_ARABIC) || defined(XK_CYRILLIC) || \
		defined(XK_ARMENIAN) || defined(XK_GEORGIAN) || defined(XK_CAUCASUS) || \
		defined(XK_VIETNAMESE) || defined(XK_CURRENCY) || \
		defined(XK_MATHEMATICAL) || defined(XK_BRAILLE) || defined(XK_SINHALA)
    if ((keysym & 0xFF000000) == 0x01000000) {
		return keysym & 0x00FFFFFF;
	}
	#endif

    // Binary search in table.
    while (max >= min) {
		mid = (min + max) / 2;
		if (keysym_unicode_table[mid].keysym < keysym) {
			min = mid + 1;
		}
		else if (keysym_unicode_table[mid].keysym > keysym) {
			max = mid - 1;
		}
		else {
			// Found it.
			return keysym_unicode_table[mid].unicode;
		}
    }

    // No matching Unicode value found!
    return 0x0000;
}


/* The following code is based on vncdisplaykeymap.c under the following terms:
 *
 * Copyright (C) 2008  Anthony Liguori <anthony@codemonkey.ws>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2 as
 * published by the Free Software Foundation.
 */
uint16_t keycode_to_scancode(KeyCode keycode) {
	uint16_t scancode = VC_UNDEFINED;

	#if defined(USE_EVDEV) && defined(USE_XKB)
	unsigned short evdev_size = sizeof(evdev_scancode_table) / sizeof(evdev_scancode_table[0]);
	#endif
	unsigned short xfree86_size = sizeof(xfree86_scancode_table) / sizeof(xfree86_scancode_table[0]);

	if (keycode < 89) {
		// No conversion required.
		scancode = keycode;
	}
	#if defined(USE_EVDEV) && defined(USE_XKB)
	else if (is_evdev && keycode < evdev_size) {
		#ifdef __OPTIMIZE_SIZE__
		keycode -= 89;
		#endif
		scancode = evdev_scancode_table[keycode][0];
	}
	#endif
	else if (keycode < xfree86_size) {
		#ifdef __OPTIMIZE_SIZE__
		keycode -= 88;
		#endif

		scancode = xfree86_scancode_table[keycode][0];
	}

	return scancode;
}

KeyCode scancode_to_keycode(uint16_t scancode) {
	KeyCode keycode = 0x0000;

	#if defined(USE_EVDEV) && defined(USE_XKB)
	unsigned short evdev_size = sizeof(evdev_scancode_table) / sizeof(evdev_scancode_table[0]);
	#endif
	unsigned short xfree86_size = sizeof(xfree86_scancode_table) / sizeof(xfree86_scancode_table[0]);

	if (scancode < 89) {
		// Simple offset of 8.
		keycode = scancode;
	}
	#if defined(USE_EVDEV) && defined(USE_XKB)
	else if (scancode < 128) {
		#ifdef __OPTIMIZE_SIZE__
		scancode -= 89;
		#endif

		keycode = evdev_scancode_table[scancode][1];
	}
	else if (is_evdev && (scancode & 0x00FF) + 128 < evdev_size) {
		// If we are using evdev and xkb and evdev is enabled.
		// Offset is the lower order bits + (25 - (scancode value at index 25 & 0xFF))
		scancode = (scancode & 0x00FF) + 128;
		
		#ifdef __OPTIMIZE_SIZE__
		scancode -= 89;
		#endif

		keycode = evdev_scancode_table[scancode][1];
	}
	#endif
	else if (scancode < xfree86_size) {
		logger(LOG_LEVEL_WARN,	"%s [%u]: Xfree86 cannot produce extended scancodes at this time!\n",
				__FUNCTION__, __LINE__);

		#ifdef __UNUSED
		// scancode = (scancode & 0x00FF) + i;
		#endif
		
		#ifdef __OPTIMIZE_SIZE__
		scancode -= 88;
		#endif
		
		keycode = xfree86_scancode_table[scancode][1];
	}

	return keycode;
}

// Faster more flexible alternative to XKeycodeToKeysym...
KeySym keycode_to_keysym(KeyCode keycode, unsigned int modifier_mask) {
	KeySym keysym = NoSymbol;

	#ifdef USE_XKB
	if (keyboard_map != NULL) {
		// Get the range and number of symbols groups bound to the key.
		unsigned char info = XkbKeyGroupInfo(keyboard_map, keycode);
		unsigned int num_groups = XkbKeyNumGroups(keyboard_map, keycode);

		// Get the group.
		unsigned int group = 0x0000;
		switch (XkbOutOfRangeGroupAction(info)) {
			case XkbRedirectIntoRange:
				/* If the RedirectIntoRange flag is set, the four least significant
				 * bits of the groups wrap control specify the index of a group to
				 * which all illegal groups correspond. If the specified group is
				 * also out of range, all illegal groups map to Group1.
				 */
				group = XkbOutOfRangeGroupInfo(info);
				if (group >= num_groups) {
					group = 0;
				}
				break;

			case XkbClampIntoRange:
				/* If the ClampIntoRange flag is set, out-of-range groups correspond
				 * to the nearest legal group. Effective groups larger than the
				 * highest supported group are mapped to the highest supported group;
				 * effective groups less than Group1 are mapped to Group1 . For
				 * example, a key with two groups of symbols uses Group2 type and
				 * symbols if the global effective group is either Group3 or Group4.
				 */
				group = num_groups - 1;
				break;

			case XkbWrapIntoRange:
				/* If neither flag is set, group is wrapped into range using integer
				 * modulus. For example, a key with two groups of symbols for which
				 * groups wrap uses Group1 symbols if the global effective group is
				 * Group3 or Group2 symbols if the global effective group is Group4.
				 */
			default:
				if (num_groups != 0) {
					group %= num_groups;
				}
				break;
		}

		XkbKeyTypePtr key_type = XkbKeyKeyType(keyboard_map, keycode, group);
		unsigned int active_mods = modifier_mask & key_type->mods.mask;

		int i, level = 0;
		for (i = 0; i < key_type->map_count; i++) {
			if (key_type->map[i].active && key_type->map[i].mods.mask == active_mods) {
				level = key_type->map[i].level;
			}
		}

		keysym = XkbKeySymEntry(keyboard_map, keycode, level, group);
	}
	#else
	if (keyboard_map != NULL) {
		if (modifier_mask & Mod2Mask &&
				((keyboard_map[keycode *keysym_per_keycode + 1] >= 0xFF80 && keyboard_map[keycode *keysym_per_keycode + 1] <= 0xFFBD) ||
				(keyboard_map[keycode *keysym_per_keycode + 1] >= 0x11000000 && keyboard_map[keycode *keysym_per_keycode + 1] <= 0x1100FFFF))
			) {

			/* If the numlock modifier is on and the second KeySym is a keypad
			 * KeySym.  In this case, if the Shift modifier is on, or if the
			 * Lock modifier is on and is interpreted as ShiftLock, then the
			 * first KeySym is used, otherwise the second KeySym is used.
			 *
			 * The standard KeySyms with the prefix ``XK_KP_'' in their name are
			 * called keypad KeySyms; these are KeySyms with numeric value in
			 * the hexadecimal range 0xFF80 to 0xFFBD inclusive. In addition,
			 * vendor-specific KeySyms in the hexadecimal range 0x11000000 to
			 * 0x1100FFFF are also keypad KeySyms.
			 */


			 /* The numlock modifier is on and the second KeySym is a keypad
			  * KeySym. In this case, if the Shift modifier is on, or if the
			  * Lock modifier is on and is interpreted as ShiftLock, then the
			  * first KeySym is used, otherwise the second KeySym is used.
			  */
			if (modifier_mask & ShiftMask || (modifier_mask & LockMask && is_shift_lock)) {
				// i = 0
				keysym = keyboard_map[keycode *keysym_per_keycode];
			}
			else {
				// i = 1
				keysym = keyboard_map[keycode *keysym_per_keycode + 1];
			}
		}
		else if (modifier_mask ^ ShiftMask && modifier_mask ^ LockMask) {
			/* The Shift and Lock modifiers are both off. In this case,
			 * the first KeySym is used.
			 */
			// index = 0
			keysym = keyboard_map[keycode *keysym_per_keycode];
		}
		else if (modifier_mask ^ ShiftMask && modifier_mask & LockMask && is_caps_lock) {
			/* The Shift modifier is off, and the Lock modifier is on
			 * and is interpreted as CapsLock. In this case, the first
			 * KeySym is used, but if that KeySym is lowercase
			 * alphabetic, then the corresponding uppercase KeySym is
			 * used instead.
			 */
			// index = 0;
			keysym = keyboard_map[keycode *keysym_per_keycode];

			if (keysym >= 'a' && keysym <= 'z') {
				// keysym is an alpha char.
				KeySym lower_keysym, upper_keysym;
				XConvertCase(keysym, &lower_keysym, &upper_keysym);
				keysym = upper_keysym;
			}
		}
		else if (modifier_mask & ShiftMask && modifier_mask & LockMask && is_caps_lock) {
			/* The Shift modifier is on, and the Lock modifier is on and
			 * is interpreted as CapsLock. In this case, the second
			 * KeySym is used, but if that KeySym is lowercase
			 * alphabetic, then the corresponding uppercase KeySym is
			 * used instead.
			 */
			// index = 1
			keysym = keyboard_map[keycode *keysym_per_keycode + 1];

			if (keysym >= 'A' && keysym <= 'Z') {
				// keysym is an alpha char.
				KeySym lower_keysym, upper_keysym;
				XConvertCase(keysym, &lower_keysym, &upper_keysym);
				keysym = lower_keysym;
			}
		}
		else if (modifier_mask & ShiftMask || (modifier_mask & LockMask && is_shift_lock) || modifier_mask & (ShiftMask + LockMask)) {
			/* The Shift modifier is on, or the Lock modifier is on and
			 * is interpreted as ShiftLock, or both. In this case, the
			 * second KeySym is used.
			 */
			// index = 1
			keysym = keyboard_map[keycode *keysym_per_keycode + 1];
		}
		else {
			logger(LOG_LEVEL_ERROR,	"%s [%u]: Unable to determine the KeySym index!\n",
					__FUNCTION__, __LINE__);
		}
	}
	#endif

	return keysym;
}

void load_input_helper(Display *disp) {
	#ifdef USE_XKB
	/* The following code block is based on vncdisplaykeymap.c under the terms:
	 *
	 * Copyright (C) 2008  Anthony Liguori <anthony codemonkey ws>
	 *
	 * This program is free software; you can redistribute it and/or modify
	 * it under the terms of the GNU Lesser General Public License version 2 as
	 * published by the Free Software Foundation.
	 */
	XkbDescPtr desc = XkbGetKeyboard(disp, XkbGBN_AllComponentsMask, XkbUseCoreKbd);
	if (desc != NULL && desc->names != NULL) {
		const char *layout_name = XGetAtomName(disp, desc->names->keycodes);
		logger(LOG_LEVEL_INFO,
				"%s [%u]: Found keycode atom '%s' (%i)!\n",
				__FUNCTION__, __LINE__, layout_name,
				(unsigned int) desc->names->keycodes);

		const char *prefix_xfree86 = "xfree86_";
		#ifdef USE_EVDEV
		const char *prefix_evdev = "evdev_";
		if (strncmp(layout_name, prefix_evdev, strlen(prefix_evdev)) == 0) {
			is_evdev = true;
		} else 
		#endif
		if (strncmp(layout_name, prefix_xfree86, strlen(prefix_xfree86)) != 0) {
			logger(LOG_LEVEL_ERROR,
					"%s [%u]: Unknown keycode name '%s', please file a bug report!\n",
					__FUNCTION__, __LINE__, layout_name);
		}
		else if (layout_name == NULL) {
			logger(LOG_LEVEL_ERROR,
					"%s [%u]: X atom name failure for desc->names->keycodes!\n",
					__FUNCTION__, __LINE__);
		}

		XkbFreeClientMap(desc, XkbGBN_AllComponentsMask, True);
	}
	else {
		logger(LOG_LEVEL_ERROR,
				"%s [%u]: XkbGetKeyboard failed to locate a valid keyboard!\n",
				__FUNCTION__, __LINE__);
	}

	// Get the map.
	keyboard_map = XkbGetMap(disp, XkbAllClientInfoMask, XkbUseCoreKbd);
	#else
	// No known alternative to determine scancode mapping, assume XFree86!
	#pragma message("*** Warning: XKB support is required to accuratly determine keyboard scancodes!")
	#pragma message("... Assuming XFree86 keyboard layout.")

	logger(LOG_LEVEL_WARN,
				"%s [%u]: Using XFree86 keyboard layout.\n",
				__FUNCTION__, __LINE__);
	logger(LOG_LEVEL_WARN,
				"%s [%u]: XKB support is required to accurately determine keyboard characters!\n",
				__FUNCTION__, __LINE__);

	int minKeyCode, maxKeyCode;
	XDisplayKeycodes(disp, &minKeyCode, &maxKeyCode);

	keyboard_map = XGetKeyboardMapping(disp, minKeyCode, (maxKeyCode - minKeyCode + 1), &keysym_per_keycode);
	if (keyboard_map) {
		XModifierKeymap *modifierMap = XGetModifierMapping(disp);

		if (modifierMap) {
			/* The Lock modifier is interpreted as CapsLock when the KeySym
			 * named XK_Caps_Lock is attached to some KeyCode and that KeyCode
			 * is attached to the Lock modifier. The Lock modifier is
			 * interpreted as ShiftLock when the KeySym named XK_Shift_Lock is
			 * attached to some KeyCode and that KeyCode is attached to the Lock
			 * modifier. If the Lock modifier could be interpreted as both
			 * CapsLock and ShiftLock, the CapsLock interpretation is used.
			 */

			KeyCode capsLock = XKeysymToKeycode(disp, XK_Caps_Lock);
			KeyCode shiftLock = XKeysymToKeycode(disp, XK_Shift_Lock);
			keysym_per_keycode--;

			// Loop over the modifier map to find out if/where shift and caps locks are set.
			for (int i = LockMapIndex; i < LockMapIndex + modifierMap->max_keypermod && !is_caps_lock; i++) {
				if (capsLock != 0 && modifierMap->modifiermap[i] == capsLock) {
					is_caps_lock = true;
					is_shift_lock = false;
				}
				else if (shiftLock != 0 && modifierMap->modifiermap[i] == shiftLock) {
					is_shift_lock = true;
				}
			}

			XFree(modifierMap);
		}
		else {
			XFree(keyboard_map);

			logger(LOG_LEVEL_ERROR,
					"%s [%u]: Unable to get modifier mapping table!\n",
					__FUNCTION__, __LINE__);
		}
	}
	else {
		logger(LOG_LEVEL_ERROR,
				"%s [%u]: Unable to get keyboard mapping table!\n",
				__FUNCTION__, __LINE__);
	}
	#endif
}

void unload_input_helper() {
	if (keyboard_map) {
		#ifdef USE_XKB
		XkbFreeClientMap(keyboard_map, XkbAllClientInfoMask, true);
		#ifdef USE_EVDEV
		is_evdev = false;
		#endif
		#else
		XFree(keyboard_map);
		#endif
	}
}
