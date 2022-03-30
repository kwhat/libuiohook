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
#include <stdlib.h>
#include <string.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>

#ifdef USE_EVDEV
#include <linux/input.h>
static bool is_evdev = false;
#endif

#include <X11/XKBlib.h>
static XkbDescPtr keyboard_map;

#include "logger.h"

#define BUTTON_MAP_MAX 256

static unsigned char *mouse_button_map;
Display *helper_disp;  // Where do we open this display?  FIXME Use the ctrl display via init param

static uint16_t modifier_mask;

/* The following two tables are based on QEMU's x_keymap.c, under the following
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
#ifdef USE_EVDEV
/* This table is generated based off the evdev -> scancode mapping above
 * and the keycode mappings in the following files:
 *    /usr/include/linux/input.h
 *    /usr/share/X11/xkb/keycodes/evdev
 *
 * NOTE This table only works for Linux.
 */
static const uint16_t evdev_scancode_table[][2] = {
    /* idx        { keycode,                scancode                },     idx  evdev code */
    /*   0 */    { VC_UNDEFINED,            0x00                    }, /* 0x00    KEY_RESERVED */
    /*   1 */    { VC_UNDEFINED,            0x09                    }, /* 0x01    KEY_ESC */
    /*   2 */    { VC_UNDEFINED,            0x0A                    }, /* 0x02    KEY_1 */
    /*   3 */    { VC_UNDEFINED,            0x0B                    }, /* 0x03    KEY_2 */
    /*   4 */    { VC_UNDEFINED,            0x0C                    }, /* 0x04    KEY_3 */
    /*   5 */    { VC_UNDEFINED,            0x0D                    }, /* 0x05    KEY_4 */
    /*   6 */    { VC_UNDEFINED,            0x0E                    }, /* 0x06    KEY_5 */
    /*   7 */    { VC_UNDEFINED,            0x0F                    }, /* 0x07    KEY_6 */
    /*   8 */    { VC_UNDEFINED,            0x10                    }, /* 0x08    KEY_7 */
    /*   9 */    { VC_ESCAPE,               0x11                    }, /* 0x09    KEY_8 */
    /*  10 */    { VC_1,                    0x12                    }, /* 0x0A    KEY_9 */
    /*  11 */    { VC_2,                    0x13                    }, /* 0x0B    KEY_0 */
    /*  12 */    { VC_3,                    0x14                    }, /* 0x0C    KEY_MINUS */
    /*  13 */    { VC_4,                    0x15                    }, /* 0x0D    KEY_EQUAL */
    /*  14 */    { VC_5,                    0x16                    }, /* 0x0E    KEY_BACKSPACE */
    /*  15 */    { VC_6,                    0x17                    }, /* 0x0F    KEY_TAB */
    /*  16 */    { VC_7,                    0x18                    }, /* 0x10    KEY_Q */
    /*  17 */    { VC_8,                    0x19                    }, /* 0x11    KEY_W */
    /*  18 */    { VC_9,                    0x1A                    }, /* 0x12    KEY_E */
    /*  19 */    { VC_0,                    0x1B                    }, /* 0x13    KEY_T */
    /*  20 */    { VC_MINUS,                0x1C                    }, /* 0x14    KEY_R */
    /*  21 */    { VC_EQUALS,               0x1D                    }, /* 0x15    KEY_Y */
    /*  22 */    { VC_BACKSPACE,            0x1E                    }, /* 0x16    KEY_U */
    /*  23 */    { VC_TAB,                  0x1F                    }, /* 0x17    KEY_I */
    /*  24 */    { VC_Q,                    0x20                    }, /* 0x18    KEY_O */
    /*  25 */    { VC_W,                    0x21                    }, /* 0x19    KEY_P */
    /*  26 */    { VC_E,                    0x22                    }, /* 0x1A    KEY_LEFTBRACE */
    /*  27 */    { VC_R,                    0x23                    }, /* 0x1B    KEY_RIGHTBRACE */
    /*  28 */    { VC_T,                    0x24                    }, /* 0x1C    KEY_ENTER */
    /*  29 */    { VC_Y,                    0x25                    }, /* 0x1D    KEY_LEFTCTRL */
    /*  30 */    { VC_U,                    0x26                    }, /* 0x1E    KEY_A */
    /*  31 */    { VC_I,                    0x27                    }, /* 0x1F    KEY_S */
    /*  32 */    { VC_O,                    0x28                    }, /* 0x20    KEY_D */
    /*  33 */    { VC_P,                    0x29                    }, /* 0x21    KEY_F */
    /*  34 */    { VC_OPEN_BRACKET,         0x2A                    }, /* 0x22    KEY_G */
    /*  35 */    { VC_CLOSE_BRACKET,        0x2B                    }, /* 0x23    KEY_H */
    /*  36 */    { VC_ENTER,                0x2C                    }, /* 0x24    KEY_J */
    /*  37 */    { VC_CONTROL_L,            0x2D                    }, /* 0x25    KEY_K */
    /*  38 */    { VC_A,                    0x2E                    }, /* 0x26    KEY_L */
    /*  39 */    { VC_S,                    0x2F                    }, /* 0x27    KEY_SEMICOLON */
    /*  40 */    { VC_D,                    0x30                    }, /* 0x28    KEY_APOSTROPHE */
    /*  41 */    { VC_F,                    0x31                    }, /* 0x29    KEY_GRAVE */
    /*  42 */    { VC_G,                    0x32                    }, /* 0x2A    KEY_LEFTSHIFT */
    /*  43 */    { VC_H,                    0x33                    }, /* 0x2B    KEY_BACKSLASH */
    /*  44 */    { VC_J,                    0x34                    }, /* 0x2C    KEY_Z */
    /*  45 */    { VC_K,                    0x35                    }, /* 0x2D    KEY_X */
    /*  46 */    { VC_L,                    0x36                    }, /* 0x2E    KEY_C */
    /*  47 */    { VC_SEMICOLON,            0x37                    }, /* 0x2F    KEY_V */
    /*  48 */    { VC_QUOTE,                0x38                    }, /* 0x30    KEY_B */
    /*  49 */    { VC_BACKQUOTE,            0x39                    }, /* 0x31    KEY_N */
    /*  50 */    { VC_SHIFT_L,              0x3A                    }, /* 0x32    KEY_M */
    /*  51 */    { VC_BACK_SLASH,           0x3B                    }, /* 0x33    KEY_COMMA */
    /*  52 */    { VC_Z,                    0x3C                    }, /* 0x34    KEY_DOT */
    /*  53 */    { VC_X,                    0x3D                    }, /* 0x35    KEY_SLASH */
    /*  54 */    { VC_C,                    0x3E                    }, /* 0x36    KEY_RIGHTSHIFT */
    /*  55 */    { VC_V,                    0x3F                    }, /* 0x37    KEY_KPASTERISK */
    /*  56 */    { VC_B,                    0x40                    }, /* 0x38    KEY_LEFTALT */
    /*  57 */    { VC_N,                    0x41                    }, /* 0x39    KEY_SPACE */
    /*  58 */    { VC_M,                    0x42                    }, /* 0x3A    KEY_CAPSLOCK */
    /*  59 */    { VC_COMMA,                0x43                    }, /* 0x3B    KEY_F1 */
    /*  60 */    { VC_PERIOD,               0x44                    }, /* 0x3C    KEY_F2 */
    /*  61 */    { VC_SLASH,                0x45                    }, /* 0x3D    KEY_F3 */
    /*  62 */    { VC_SHIFT_R,              0x46                    }, /* 0x3E    KEY_F4 */
    /*  63 */    { VC_KP_MULTIPLY,          0x47                    }, /* 0x3F    KEY_F5 */
    /*  64 */    { VC_ALT_L,                0x48                    }, /* 0x40    KEY_F6 */
    /*  65 */    { VC_SPACE,                0x49                    }, /* 0x41    KEY_F7 */
    /*  66 */    { VC_CAPS_LOCK,            0x4A                    }, /* 0x42    KEY_F8 */
    /*  67 */    { VC_F1,                   0x4B                    }, /* 0x43    KEY_F9 */
    /*  68 */    { VC_F2,                   0x4C                    }, /* 0x44    KEY_F10 */
    /*  69 */    { VC_F3,                   0x4D                    }, /* 0x45    KEY_NUMLOCK */
    /*  70 */    { VC_F4,                   0x4E                    }, /* 0x46    KEY_SCROLLLOCK */
    /*  71 */    { VC_F5,                   0x4F                    }, /* 0x47    KEY_KP7 */
    /*  72 */    { VC_F6,                   0x50                    }, /* 0x48    KEY_KP8 */
    /*  73 */    { VC_F7,                   0x51                    }, /* 0x49    KEY_KP9 */
    /*  74 */    { VC_F8,                   0x52                    }, /* 0x4A    KEY_KPMINUS */
    /*  75 */    { VC_F9,                   0x53                    }, /* 0x4B    KEY_KP4 */
    /*  76 */    { VC_F10,                  0x54                    }, /* 0x4C    KEY_KP5 */
    /*  77 */    { VC_NUM_LOCK,             0x55                    }, /* 0x4D    KEY_KP6 */
    /*  78 */    { VC_SCROLL_LOCK,          0x56                    }, /* 0x4E    KEY_KPPLUS */
    /*  79 */    { VC_KP_7,                 0x57                    }, /* 0x4F    KEY_KP1 */
    /*  80 */    { VC_KP_8,                 0x58                    }, /* 0x50    KEY_KP2 */
    /*  81 */    { VC_KP_9,                 0x59                    }, /* 0x51    KEY_KP3 */
    /*  82 */    { VC_KP_SUBTRACT,          0x5A                    }, /* 0x52    KEY_KP0 */
    /*  83 */    { VC_KP_4,                 0x5B                    }, /* 0x53    KEY_KPDOT */
    /*  84 */    { VC_KP_5,                 0x00                    }, /* 0x54    */
    /*  85 */    { VC_KP_6,                 0x00                    }, /* 0x55    TODO [KEY_ZENKAKUHANKAKU][0] == [VC_?][1] */
    /*  86 */    { VC_KP_ADD,               0x00                    }, /* 0x56    TODO [KEY_102ND][0] == [VC_?][1] */
    /*  87 */    { VC_KP_1,                 0x5F                    }, /* 0x57    KEY_F11 */
    /*  88 */    { VC_KP_2,                 0x60                    }, /* 0x58    KEY_F12 */
    /*  89 */    { VC_KP_3,                 0x00                    }, /* 0x59    TODO [KEY_RO][0] == [VC_?][1] */
    /*  90 */    { VC_KP_0,                 0x00                    }, /* 0x5A */
    /*  91 */    { VC_KP_SEPARATOR,         0xBF                    }, /* 0x5B    KEY_F13 */
    /*  92 */    { VC_UNDEFINED,            0xC0                    }, /* 0x5C    KEY_F14 */
    /*  93 */    { VC_UNDEFINED,            0xC1                    }, /* 0x5D    KEY_F15 */
    /*  94 */    { VC_UNDEFINED,            0x00                    }, /* 0x5E    TODO [KEY_MUHENKAN][0] == [VC_?][1] */
    /*  95 */    { VC_F11,                  0x00                    }, /* 0x5F */
    /*  96 */    { VC_F12,                  0x00                    }, /* 0x60 */

    /* First 97 chars are identical to XFree86!                                */

    /*  97 */    { VC_UNDEFINED,            0x00                    }, /* 0x61 */
    /*  98 */    { VC_KATAKANA,             0x00                    }, /* 0x62 */
    /*  99 */    { VC_HIRAGANA,             0xC2                    }, /* 0x63    KEY_F16 */
    /* 100 */    { VC_KANJI,                0xC3                    }, /* 0x64    KEY_F17 */
    /* 101 */    { VC_UNDEFINED,            0xC4                    }, /* 0x65    KEY_F18 */
    /* 102 */    { VC_UNDEFINED,            0xC5                    }, /* 0x66    KEY_F19 */
    /* 103 */    { VC_KP_COMMA,             0xC6                    }, /* 0x67    KEY_F20 */
    /* 104 */    { VC_KP_ENTER,             0xC7                    }, /* 0x68    KEY_F21 */
    /* 105 */    { VC_CONTROL_R,            0xC8                    }, /* 0x69    KEY_F22 */
    /* 106 */    { VC_KP_DIVIDE,            0xC9                    }, /* 0x6A    KEY_F23 */
    /* 107 */    { VC_PRINTSCREEN,          0xCA                    }, /* 0x6B    KEY_F24 */
    /* 108 */    { VC_ALT_R,                0x00                    }, /* 0x6C */
    /* 109 */    { VC_UNDEFINED,            0x00                    }, /* 0x6D */
    /* 110 */    { VC_HOME,                 0x00                    }, /* 0x6E */
    /* 111 */    { VC_UP,                   0x00                    }, /* 0x6F */
    /* 112 */    { VC_PAGE_UP,              0x62                    }, /* 0x70    KEY_KATAKANA */
    /* 113 */    { VC_LEFT,                 0x00                    }, /* 0x71 */
    /* 114 */    { VC_RIGHT,                0x00                    }, /* 0x72 */
    /* 115 */    { VC_END,                  0x00                    }, /* 0x73    TODO KEY_? = [VC_UNDERSCORE][1] */
    /* 116 */    { VC_DOWN,                 0x00                    }, /* 0x74    TODO KEY_? = [VC_FURIGANA][1] */
    /* 117 */    { VC_PAGE_DOWN,            0x00                    }, /* 0x75 */
    /* 118 */    { VC_INSERT,               0x00                    }, /* 0x76    TODO [KEY_KPPLUSMINUS][0] = [VC_?][1] */
    /* 119 */    { VC_DELETE,               0x00                    }, /* 0x77 */
    /* 120 */    { VC_UNDEFINED,            0x00                    }, /* 0x78    TODO [KEY_SCALE][0] = [VC_?][1] */
    /* 121 */    { VC_VOLUME_MUTE,          0x64                    }, /* 0x79    KEY_HENKAN */
    /* 122 */    { VC_VOLUME_DOWN,          0x00                    }, /* 0x7A */
    /* 123 */    { VC_VOLUME_UP,            0x63                    }, /* 0x7B    KEY_HIRAGANA */
    /* 124 */    { VC_POWER,                0x00                    }, /* 0x7C */
    /* 125 */    { VC_KP_EQUALS,            0x84                    }, /* 0x7D    KEY_YEN */
    /* 126 */    { VC_UNDEFINED,            0x67                    }, /* 0x7E    KEY_KPJPCOMMA */
    /* 127 */    { VC_PAUSE,                0x00                    }, /* 0x7F */

    /*            No Offset                Offset (i & 0x007F) + 128            */

    /* 128 */    { VC_UNDEFINED,            0                       }, /* 0x80 */
    /* 129 */    { VC_UNDEFINED,            0                       }, /* 0x81 */
    /* 130 */    { VC_UNDEFINED,            0                       }, /* 0x82 */
    /* 131 */    { VC_UNDEFINED,            0                       }, /* 0x83 */
    /* 132 */    { VC_YEN,                  0                       }, /* 0x84 */
    /* 133 */    { VC_META_L,               0                       }, /* 0x85 */
    /* 134 */    { VC_META_R,               0                       }, /* 0x86 */
    /* 135 */    { VC_CONTEXT_MENU,         0                       }, /* 0x87 */
    /* 136 */    { VC_SUN_STOP,             0                       }, /* 0x88 */
    /* 137 */    { VC_SUN_AGAIN,            0                       }, /* 0x89 */
    /* 138 */    { VC_SUN_PROPS,            0                       }, /* 0x8A */
    /* 139 */    { VC_SUN_UNDO,             0                       }, /* 0x8B */
    /* 140 */    { VC_SUN_FRONT,            0                       }, /* 0x8C */
    /* 141 */    { VC_SUN_COPY,             0x7D                    }, /* 0x8D    KEY_KPEQUAL */
    /* 142 */    { VC_SUN_OPEN,             0                       }, /* 0x8E */
    /* 143 */    { VC_SUN_INSERT,           0                       }, /* 0x8F */
    /* 144 */    { VC_SUN_FIND,             0                       }, /* 0x90 */
    /* 145 */    { VC_SUN_CUT,              0                       }, /* 0x91 */
    /* 146 */    { VC_SUN_HELP,             0                       }, /* 0x92 */
    /* 147 */    { VC_UNDEFINED,            0                       }, /* 0x93 */
    /* 148 */    { VC_APP_CALCULATOR,       0                       }, /* 0x94 */
    /* 149 */    { VC_UNDEFINED,            0                       }, /* 0x95 */
    /* 150 */    { VC_SLEEP,                0                       }, /* 0x96 */
    /* 151 */    { VC_UNDEFINED,            0                       }, /* 0x97 */
    /* 152 */    { VC_UNDEFINED,            0                       }, /* 0x98 */
    /* 153 */    { VC_UNDEFINED,            0                       }, /* 0x99 */
    /* 154 */    { VC_UNDEFINED,            0                       }, /* 0x9A */
    /* 155 */    { VC_UNDEFINED,            0                       }, /* 0x9B */
    /* 156 */    { VC_UNDEFINED,            0x68                    }, /* 0x9C    KEY_KPENTER */
    /* 157 */    { VC_UNDEFINED,            0x69                    }, /* 0x9D    KEY_RIGHTCTRL */
    /* 158 */    { VC_UNDEFINED,            0                       }, /* 0x9E */
    /* 159 */    { VC_UNDEFINED,            0                       }, /* 0x9F */
    /* 160 */    { VC_UNDEFINED,            0x79                    }, /* 0xA0    KEY_MUTE */
    /* 161 */    { VC_UNDEFINED,            0x94                    }, /* 0xA1    KEY_CALC */
    /* 162 */    { VC_UNDEFINED,            0xA7                    }, /* 0xA2    KEY_FORWARD */
    /* 163 */    { VC_UNDEFINED,            0                       }, /* 0xA3 */
    /* 164 */    { VC_UNDEFINED,            0                       }, /* 0xA4 */
    /* 165 */    { VC_UNDEFINED,            0                       }, /* 0xA5 */
    /* 166 */    { VC_APP_MAIL,             0                       }, /* 0xA6 */
    /* 167 */    { VC_MEDIA_PLAY,           0                       }, /* 0xA7 */
    /* 168 */    { VC_UNDEFINED,            0                       }, /* 0xA8 */
    /* 169 */    { VC_UNDEFINED,            0                       }, /* 0xA9 */
    /* 170 */    { VC_UNDEFINED,            0                       }, /* 0xAA */
    /* 171 */    { VC_UNDEFINED,            0                       }, /* 0xAB */
    /* 172 */    { VC_UNDEFINED,            0                       }, /* 0xAC */
    /* 173 */    { VC_UNDEFINED,            0                       }, /* 0xAD */
    /* 174 */    { VC_UNDEFINED,            0x7A                    }, /* 0xAE    KEY_VOLUMEDOWN */
    /* 175 */    { VC_UNDEFINED,            0                       }, /* 0xAF */
    /* 176 */    { VC_UNDEFINED,            0x7B                    }, /* 0xB0    KEY_VOLUMEUP */
    /* 177 */    { VC_UNDEFINED,            0x00                    }, /* 0xB1 */
    /* 178 */    { VC_UNDEFINED,            0xBA                    }, /* 0xB2    KEY_SCROLLUP */
    /* 179 */    { VC_UNDEFINED,            0x00                    }, /* 0xB3 */
    /* 180 */    { VC_UNDEFINED,            0x00                    }, /* 0xB4 */
    /* 181 */    { VC_UNDEFINED,            0x6A                    }, /* 0xB5    KEY_KPSLASH */
    /* 182 */    { VC_UNDEFINED,            0x00                    }, /* 0xB6 */
    /* 183 */    { VC_UNDEFINED,            0x6B                    }, /* 0xB7    KEY_SYSRQ */
    /* 184 */    { VC_UNDEFINED,            0x6C                    }, /* 0xB8    KEY_RIGHTALT */
    /* 185 */    { VC_UNDEFINED,            0x00                    }, /* 0xB9 */
    /* 186 */    { VC_BROWSER_HOME,         0x00                    }, /* 0xBA */
    /* 187 */    { VC_UNDEFINED,            0x00                    }, /* 0xBB */
    /* 188 */    { VC_UNDEFINED,            0x00                    }, /* 0xBC */
    /* 189 */    { VC_UNDEFINED,            0x00                    }, /* 0xBD */
    /* 190 */    { VC_UNDEFINED,            0x00                    }, /* 0xBE */
    /* 191 */    { VC_F13,                  0x00                    }, /* 0xBF */
    /* 192 */    { VC_F14,                  0x00                    }, /* 0xC0 */
    /* 193 */    { VC_F15,                  0x00                    }, /* 0xC1 */
    /* 194 */    { VC_F16,                  0x00                    }, /* 0xC2 */
    /* 195 */    { VC_F17,                  0x00                    }, /* 0xC3 */
    /* 196 */    { VC_F18,                  0x00                    }, /* 0xC4 */
    /* 197 */    { VC_F19,                  0x7F                    }, /* 0xC5    KEY_PAUSE */
    /* 198 */    { VC_F20,                  0x00                    }, /* 0xC6 */
    /* 199 */    { VC_F21,                  0x6E                    }, /* 0xC7    KEY_HOME */
    /* 200 */    { VC_F22,                  0x6F                    }, /* 0xC8    KEY_UP */
    /* 201 */    { VC_F23,                  0x70                    }, /* 0xC9    KEY_PAGEUP */
    /* 202 */    { VC_F24,                  0x00                    }, /* 0xCA */
    /* 203 */    { VC_UNDEFINED,            0x71                    }, /* 0xCB    KEY_LEFT */
    /* 204 */    { VC_UNDEFINED,            0x00                    }, /* 0xCC */
    /* 205 */    { VC_UNDEFINED,            0x72                    }, /* 0xCD    KEY_RIGHT */
    /* 206 */    { VC_UNDEFINED,            0x00                    }, /* 0xCE */
    /* 207 */    { VC_UNDEFINED,            0x73                    }, /* 0xCF    KEY_END */
    /* 208 */    { VC_UNDEFINED,            0x74                    }, /* 0xD0    KEY_DOWN */
    /* 209 */    { VC_UNDEFINED,            0x75                    }, /* 0xD1    KEY_PAGEDOWN */
    /* 210 */    { VC_UNDEFINED,            0x76                    }, /* 0xD2    KEY_INSERT */
    /* 211 */    { VC_UNDEFINED,            0x77                    }, /* 0xD3    KEY_DELETE */
    /* 212 */    { VC_UNDEFINED,            0x00                    }, /* 0xD4 */
    /* 213 */    { VC_UNDEFINED,            0x00                    }, /* 0xD5 */
    /* 214 */    { VC_UNDEFINED,            0x00                    }, /* 0xD6 */
    /* 215 */    { VC_UNDEFINED,            0x00                    }, /* 0xD7 */
    /* 216 */    { VC_UNDEFINED,            0x00                    }, /* 0xD8 */
    /* 217 */    { VC_UNDEFINED,            0x00                    }, /* 0xD9 */
    /* 218 */    { VC_UNDEFINED,            0x00                    }, /* 0xDA */
    /* 219 */    { VC_UNDEFINED,            0x85                    }, /* 0xDB    KEY_LEFTMETA */
    /* 220 */    { VC_UNDEFINED,            0x86                    }, /* 0xDC    KEY_RIGHTMETA */
    /* 221 */    { VC_UNDEFINED,            0x87                    }, /* 0xDD    KEY_COMPOSE */
    /* 222 */    { VC_UNDEFINED,            0x7C                    }, /* 0xDE    KEY_POWER */
    /* 223 */    { VC_UNDEFINED,            0x96                    }, /* 0xDF    KEY_SLEEP */
    /* 224 */    { VC_UNDEFINED,            0x00                    }, /* 0xE0 */
    /* 225 */    { VC_BROWSER_SEARCH,       0x00                    }, /* 0xE1 */
    /* 226 */    { VC_LESSER_GREATER,       0x00                    }, /* 0xE2 */
    /* 227 */    { VC_UNDEFINED,            0x00                    }, /* 0xE3 */
    /* 228 */    { VC_UNDEFINED,            0x00                    }, /* 0xE4 */
    /* 229 */    { VC_UNDEFINED,            0xE1                    }, /* 0xE5    KEY_SEARCH */
    /* 230 */    { VC_UNDEFINED,            0x00                    }, /* 0xE6 */
    /* 231 */    { VC_UNDEFINED,            0x00                    }, /* 0xE7 */
    /* 232 */    { VC_UNDEFINED,            0x00                    }, /* 0xE8 */
    /* 233 */    { VC_UNDEFINED,            0x00                    }, /* 0xE9 */
    /* 234 */    { VC_UNDEFINED,            0x00                    }, /* 0xEA */
    /* 235 */    { VC_UNDEFINED,            0x00                    }, /* 0xEB */
    /* 236 */    { VC_UNDEFINED,            0xA6                    }, /* 0xEC    KEY_BACK */
    /* 237 */    { VC_UNDEFINED,            0x00                    }, /* 0xED */
    /* 238 */    { VC_UNDEFINED,            0x00                    }, /* 0xEE */
    /* 239 */    { VC_UNDEFINED,            0x00                    }, /* 0xEF */
    /* 240 */    { VC_UNDEFINED,            0x00                    }, /* 0xF0 */
    /* 241 */    { VC_UNDEFINED,            0x00                    }, /* 0xF1 */
    /* 242 */    { VC_UNDEFINED,            0x00                    }, /* 0xF2 */
    /* 243 */    { VC_UNDEFINED,            0x00                    }, /* 0xF3 */
    /* 244 */    { VC_UNDEFINED,            0x8E                    }, /* 0xF4    KEY_OPEN */
    /* 245 */    { VC_UNDEFINED,            0x92                    }, /* 0xF5    KEY_HELP */
    /* 246 */    { VC_UNDEFINED,            0x8A                    }, /* 0xF6    KEY_PROPS */
    /* 247 */    { VC_UNDEFINED,            0x8C                    }, /* 0xF7    KEY_FRONT */
    /* 248 */    { VC_UNDEFINED,            0x88                    }, /* 0xF8    KEY_STOP */
    /* 249 */    { VC_UNDEFINED,            0x89                    }, /* 0xF9    KEY_AGAIN */
    /* 250 */    { VC_UNDEFINED,            0x8B                    }, /* 0xFA    KEY_UNDO */
    /* 251 */    { VC_UNDEFINED,            0x91                    }, /* 0xFB    KEY_CUT */
    /* 252 */    { VC_UNDEFINED,            0x8D                    }, /* 0xFC    KEY_COPY */
    /* 253 */    { VC_UNDEFINED,            0x8F                    }, /* 0xFD    KEY_PASTE */
    /* 254 */    { VC_UNDEFINED,            0x90                    }, /* 0xFE    KEY_FIND */
    /* 255 */    { VC_UNDEFINED,            0x00                    }, /* 0xFF */
};
#endif


/* This table is generated based off the xfree86 -> scancode mapping above
 * and the keycode mappings in the following files:
 *        /usr/share/X11/xkb/keycodes/xfree86
 *
 * TODO Everything after 157 needs to be populated with scancodes for media
 * controls and internet keyboards.
 */
static const uint16_t xfree86_scancode_table[][2] = {
    /* idx        { keycode,                scancode                }, */
    /*   0 */    { VC_UNDEFINED,              0    /* <MDSW>  */    },    // Unused
    /*   1 */    { VC_UNDEFINED,              9    /* <ESC>   */    },    //
    /*   2 */    { VC_UNDEFINED,             10    /* <AE01>  */    },    //
    /*   3 */    { VC_UNDEFINED,             11    /* <AE02>  */    },    //
    /*   4 */    { VC_UNDEFINED,             12    /* <AE03>  */    },    //
    /*   5 */    { VC_UNDEFINED,             13    /* <AE04>  */    },    //
    /*   6 */    { VC_UNDEFINED,             14    /* <AE05>  */    },    //
    /*   7 */    { VC_UNDEFINED,             15    /* <AE06>  */    },    //
    /*   8 */    { VC_UNDEFINED,             16    /* <AE07>  */    },    //
    /*   9 */    { VC_ESCAPE,                17    /* <AE08>  */    },    //
    /*  10 */    { VC_1,                     18    /* <AE009> */    },    //
    /*  11 */    { VC_2,                     19    /* <AE010> */    },    //
    /*  12 */    { VC_3,                     20    /* <AE011> */    },    //
    /*  13 */    { VC_4,                     21    /* <AE012> */    },    //
    /*  14 */    { VC_5,                     22    /* <BKSP>  */    },    //
    /*  15 */    { VC_6,                     23    /* <TAB>   */    },    //
    /*  16 */    { VC_7,                     24    /* <AD01>  */    },    //
    /*  17 */    { VC_8,                     25    /* <AD02>  */    },    //
    /*  18 */    { VC_9,                     26    /* <AD03>  */    },    //
    /*  19 */    { VC_0,                     27    /* <AD04>  */    },    //
    /*  20 */    { VC_MINUS,                 28    /* <AD05>  */    },    //
    /*  21 */    { VC_EQUALS,                29    /* <AD06>  */    },    //
    /*  22 */    { VC_BACKSPACE,             30    /* <AD07>  */    },    //
    /*  23 */    { VC_TAB,                   31    /* <AD08>  */    },    //
    /*  24 */    { VC_Q,                     32    /* <AD09>  */    },    //
    /*  25 */    { VC_W,                     33    /* <AD10>  */    },    //
    /*  26 */    { VC_E,                     34    /* <AD11>  */    },    //
    /*  27 */    { VC_R,                     35    /* <AD12>  */    },    //
    /*  28 */    { VC_T,                     36    /* <RTRN>  */    },    //
    /*  29 */    { VC_Y,                     37    /* <LCTL>  */    },    //
    /*  30 */    { VC_U,                     38    /* <AC01>  */    },  //
    /*  31 */    { VC_I,                     39    /* <AC02>  */    },    //
    /*  32 */    { VC_O,                     40    /* <AC03>  */    },    //
    /*  33 */    { VC_P,                     41    /* <AC04>  */    },    //
    /*  34 */    { VC_OPEN_BRACKET,          42    /* <AC05>  */    },    //
    /*  35 */    { VC_CLOSE_BRACKET,         43    /* <AC06>  */    },    //
    /*  36 */    { VC_ENTER,                 44    /* <AC07>  */    },    //
    /*  37 */    { VC_CONTROL_L,             45    /* <AC08>  */    },    //
    /*  38 */    { VC_A,                     46    /* <AC09>  */    },    //
    /*  39 */    { VC_S,                     47    /* <AC10>  */    },    //
    /*  40 */    { VC_D,                     48    /* <AC11>  */    },    //
    /*  41 */    { VC_F,                     49    /* <TLDE>  */    },    //
    /*  42 */    { VC_G,                     50    /* <LFSH>  */    },    //
    /*  43 */    { VC_H,                     51    /* <BKSL>  */    },    //
    /*  44 */    { VC_J,                     52    /* <AB01>  */    },    //
    /*  45 */    { VC_K,                     53    /* <AB02>  */    },    //
    /*  46 */    { VC_L,                     54    /* <AB03>  */    },    //
    /*  47 */    { VC_SEMICOLON,             55    /* <AB04>  */    },    //
    /*  48 */    { VC_QUOTE,                 56    /* <AB05>  */    },    //
    /*  49 */    { VC_BACKQUOTE,             57    /* <AB06>  */    },    //
    /*  50 */    { VC_SHIFT_L,               58    /* <AB07>  */    },    //
    /*  51 */    { VC_BACK_SLASH,            59    /* <AB08>  */    },    //
    /*  52 */    { VC_Z,                     60    /* <AB09>  */    },    //
    /*  53 */    { VC_X,                     61    /* <AB10>  */    },    //
    /*  54 */    { VC_C,                     62    /* <RTSH>  */    },    //
    /*  55 */    { VC_V,                     63    /* <KPMU>  */    },    //
    /*  56 */    { VC_B,                     64    /* <LALT>  */    },    //
    /*  57 */    { VC_N,                     65    /* <SPCE>  */    },    //
    /*  58 */    { VC_M,                     66    /* <CAPS>  */    },    //
    /*  59 */    { VC_COMMA,                 67    /* <FK01>  */    },    //
    /*  60 */    { VC_PERIOD,                68    /* <FK02>  */    },    //
    /*  61 */    { VC_SLASH,                 69    /* <FK03>  */    },    //
    /*  62 */    { VC_SHIFT_R,               70    /* <FK04>  */    },    //
    /*  63 */    { VC_KP_MULTIPLY,           71    /* <FK05>  */    },    //
    /*  64 */    { VC_ALT_L,                 72    /* <FK06>  */    },    //
    /*  65 */    { VC_SPACE,                 73    /* <FK07>  */    },    //
    /*  66 */    { VC_CAPS_LOCK,             74    /* <FK08>  */    },    //
    /*  67 */    { VC_F1,                    75    /* <FK09>  */    },    //
    /*  68 */    { VC_F2,                    76    /* <FK10>  */    },    //
    /*  69 */    { VC_F3,                    77    /* <NMLK>  */    },    //
    /*  70 */    { VC_F4,                    78    /* <SCLK>  */    },    //
    /*  71 */    { VC_F5,                    79    /* <KP7>   */    },    //
    /*  72 */    { VC_F6,                    80    /* <KP8>   */    },    //
    /*  73 */    { VC_F7,                    81    /* <KP9>   */    },    //
    /*  74 */    { VC_F8,                    82    /* <KPSU>  */    },    //
    /*  75 */    { VC_F9,                    83    /* <KP4>   */    },    //
    /*  76 */    { VC_F10,                   84    /* <KP5>   */    },    //
    /*  77 */    { VC_NUM_LOCK,              85    /* <KP6>   */    },    //
    /*  78 */    { VC_SCROLL_LOCK,           86    /* <KPAD>  */    },    //
    /*  79 */    { VC_KP_7,                  87    /* <KP1>   */    },    //
    /*  80 */    { VC_KP_8,                  88    /* <KP2>   */    },    //
    /*  81 */    { VC_KP_9,                  89    /* <KP3>   */    },    //
    /*  82 */    { VC_KP_SUBTRACT,           90    /* <KP0>   */    },    //
    /*  83 */    { VC_KP_4,                  91    /* <KPDL>  */    },    //
    /*  84 */    { VC_KP_5,                   0                     },    //
    /*  85 */    { VC_KP_6,                   0                     },    //
    /*  86 */    { VC_KP_ADD,                 0                     },    //
    /*  87 */    { VC_KP_1,                  95    /* <FK11>  */    },    //
    /*  88 */    { VC_KP_2,                  96    /* <FK12>  */    },
    /*  89 */    { VC_KP_3,                   0                     },
    /*  90 */    { VC_KP_0,                   0                     },
    /*  91 */    { VC_KP_SEPARATOR,         118    /* <FK13>  */    },
    /*  92 */    { VC_UNDEFINED,            119    /* <FK14>  */    },
    /*  93 */    { VC_UNDEFINED,            120    /* <FK15>  */    },
    /*  94 */    { VC_UNDEFINED,              0                     },
    /*  95 */    { VC_F11,                    0                     },
    /*  96 */    { VC_F12,                    0                     },

    /* First 97 chars are identical to XFree86!                                */

    /*  97 */    { VC_HOME,                   0                     },
    /*  98 */    { VC_UP,                     0                     },
    /*  99 */    { VC_PAGE_UP,              121    /* <FK16>  */    },
    /* 100 */    { VC_LEFT,                 122    /* <FK17>  */    },
    /* 101 */    { VC_UNDEFINED,              0                     },     // TODO lower brightness key?
    /* 102 */    { VC_RIGHT,                  0                     },
    /* 103 */    { VC_END,                    0                     },
    /* 104 */    { VC_DOWN,                   0                     },
    /* 105 */    { VC_PAGE_DOWN,              0                     },
    /* 106 */    { VC_INSERT,                 0                     },
    /* 107 */    { VC_DELETE,                 0                     },
    /* 108 */    { VC_KP_ENTER,               0                     },
    /* 109 */    { VC_CONTROL_R,              0                     },
    /* 110 */    { VC_PAUSE,                  0                     },
    /* 111 */    { VC_PRINTSCREEN,            0                     },
    /* 112 */    { VC_KP_DIVIDE,              0                     },
    /* 113 */    { VC_ALT_R,                  0                     },
    /* 114 */    { VC_UNDEFINED,              0                     },    // VC_BREAK?
    /* 115 */    { VC_META_L,                 0                     },
    /* 116 */    { VC_META_R,                 0                     },
    /* 117 */    { VC_CONTEXT_MENU,           0                     },
    /* 118 */    { VC_F13,                    0                     },
    /* 119 */    { VC_F14,                    0                     },
    /* 120 */    { VC_F15,                    0                     },
    /* 121 */    { VC_F16,                    0                     },
    /* 122 */    { VC_F17,                    0                     },
    /* 123 */    { VC_UNDEFINED,              0                     },    // <KPDC>    FIXME What is this key?
    /* 124 */    { VC_UNDEFINED,              0                     },    // <LVL3>    Never Generated
    /* 125 */    { VC_UNDEFINED,            133    /* <AE13>  */    },    // <ALT>    Never Generated
    /* 126 */    { VC_KP_EQUALS,              0                     },
    /* 127 */    { VC_UNDEFINED,              0                     },    // <SUPR>    Never Generated
    /* 128 */    { VC_UNDEFINED,              0                     },    // <HYPR>    Never Generated
    /* 129 */    { VC_UNDEFINED,              0                     },    // <XFER>    Henkan
    /* 130 */    { VC_UNDEFINED,              0                     },    // <I02>    Some extended Internet key
    /* 131 */    { VC_UNDEFINED,              0                     },    // <NFER>    Muhenkan
    /* 132 */    { VC_UNDEFINED,              0                     },    // <I04>
    /* 133 */    { VC_YEN,                    0                     },    // <AE13>
    /* 134 */    { VC_UNDEFINED,              0                     },    // <I06>
    /* 135 */    { VC_UNDEFINED,              0                     },    // <I07>
    /* 136 */    { VC_UNDEFINED,              0                     },    // <I08>
    /* 137 */    { VC_UNDEFINED,              0                     },    // <I09>
    /* 138 */    { VC_UNDEFINED,              0                     },    // <I0A>
    /* 139 */    { VC_UNDEFINED,              0                     },    // <I0B>
    /* 140 */    { VC_UNDEFINED,              0                     },    // <I0C>
    /* 141 */    { VC_UNDEFINED,            126                     },    // <I0D>
    /* 142 */    { VC_UNDEFINED,              0                     },    // <I0E>
    /* 143 */    { VC_UNDEFINED,              0                     },    // <I0F>
    /* 144 */    { VC_UNDEFINED,              0                     },    // <I10>
    /* 145 */    { VC_UNDEFINED,              0                     },    // <I11>
    /* 146 */    { VC_UNDEFINED,              0                     },    // <I12>
    /* 147 */    { VC_UNDEFINED,              0                     },    // <I13>
    /* 148 */    { VC_UNDEFINED,              0                     },    // <I14>
    /* 149 */    { VC_UNDEFINED,              0                     },    // <I15>
    /* 150 */    { VC_UNDEFINED,              0                     },    // <I16>
    /* 151 */    { VC_UNDEFINED,              0                     },    // <I17>
    /* 152 */    { VC_UNDEFINED,              0                     },    // <I18>
    /* 153 */    { VC_UNDEFINED,              0                     },    // <I19>
    /* 154 */    { VC_UNDEFINED,              0                     },    // <I1A>
    /* 155 */    { VC_UNDEFINED,              0                     },    // <I1B>
    /* 156 */    { VC_UNDEFINED,            108    /* <KPEN>  */    },    // <I1C>    Never Generated
    /* 157 */    { VC_UNDEFINED,            109    /* <RCTL>  */    },    // <I1D>
    /* 158 */    { VC_UNDEFINED,              0                     },    // <I1E>
    /* 159 */    { VC_UNDEFINED,              0                     },    // <I1F>
    /* 160 */    { VC_UNDEFINED,              0                     },    // <I20>
    /* 161 */    { VC_UNDEFINED,              0                     },    // <I21>
    /* 162 */    { VC_UNDEFINED,              0                     },    // <I22>
    /* 163 */    { VC_UNDEFINED,              0                     },    // <I23>
    /* 164 */    { VC_UNDEFINED,              0                     },    // <I24>
    /* 165 */    { VC_UNDEFINED,              0                     },    // <I25>
    /* 166 */    { VC_UNDEFINED,              0                     },    // <I26>
    /* 167 */    { VC_UNDEFINED,              0                     },    // <I27>
    /* 168 */    { VC_UNDEFINED,              0                     },    // <I28>
    /* 169 */    { VC_UNDEFINED,              0                     },    // <I29>
    /* 170 */    { VC_UNDEFINED,              0                     },    // <I2A>    <K5A>
    /* 171 */    { VC_UNDEFINED,              0                     },    // <I2B>
    /* 172 */    { VC_UNDEFINED,              0                     },    // <I2C>
    /* 173 */    { VC_UNDEFINED,              0                     },    // <I2D>
    /* 174 */    { VC_UNDEFINED,              0                     },    // <I2E>
    /* 175 */    { VC_UNDEFINED,              0                     },    // <I2F>
    /* 176 */    { VC_UNDEFINED,              0                     },    // <I30>
    /* 177 */    { VC_UNDEFINED,              0                     },    // <I31>
    /* 178 */    { VC_UNDEFINED,              0                     },    // <I32>
    /* 179 */    { VC_UNDEFINED,              0                     },    // <I33>
    /* 180 */    { VC_UNDEFINED,              0                     },    // <I34>
    /* 181 */    { VC_UNDEFINED,            112                     },    // <I35>    <K5B>
    /* 182 */    { VC_UNDEFINED,              0                     },    // <I36>    <K5D>
    /* 183 */    { VC_UNDEFINED,            111                     },    // <I37>    <K5E>
    /* 184 */    { VC_UNDEFINED,            113                     },    // <I38>    <K5F>
    /* 185 */    { VC_UNDEFINED,              0                     },    // <I39>
    /* 186 */    { VC_UNDEFINED,              0                     },    // <I3A>
    /* 187 */    { VC_UNDEFINED,              0                     },    // <I3B>
    /* 188 */    { VC_UNDEFINED,              0                     },    // <I3C>
    /* 189 */    { VC_UNDEFINED,              0                     },    // <I3D>    <K62>
    /* 190 */    { VC_UNDEFINED,              0                     },    // <I3E>    <K63>
    /* 191 */    { VC_UNDEFINED,              0                     },    // <I3F>    <K64>
    /* 192 */    { VC_UNDEFINED,              0                     },    // <I40>    <K65>
    /* 193 */    { VC_UNDEFINED,              0                     },    // <I41>    <K66>
    /* 194 */    { VC_UNDEFINED,              0                     },    // <I42>
    /* 195 */    { VC_UNDEFINED,              0                     },    // <I43>
    /* 196 */    { VC_UNDEFINED,              0                     },    // <I44>    // 114 <BRK>?
    /* 197 */    { VC_UNDEFINED,            110                     },    // <I45>
    /* 198 */    { VC_UNDEFINED,              0                     },    // <I46>    <K67>
    /* 199 */    { VC_UNDEFINED,             97    /* <HOME>   */   },    // <I47>    <K68>
    /* 200 */    { VC_UNDEFINED,             98                     },    // <I48>    <K69>
    /* 201 */    { VC_UNDEFINED,             99                     },    // <I49>    <K6A>
    /* 202 */    { VC_UNDEFINED,              0                     },    // <I4A
    /* 203 */    { VC_UNDEFINED,            100                     },    // <I4B>    <K6B>
    /* 204 */    { VC_UNDEFINED,              0                     },    // <I4C>    <K6C>
    /* 205 */    { VC_UNDEFINED,            102                     },    // <I4D>    <K6D>
    /* 206 */    { VC_UNDEFINED,              0                     },    // <I4E>    <K6E>
    /* 207 */    { VC_UNDEFINED,            103                     },    // <I4F>    <K6F>
    /* 208 */    { VC_UNDEFINED,            104                     },    // <I50>    <K70>
    /* 209 */    { VC_UNDEFINED,            105                     },    // <I51>    <K71>
    /* 210 */    { VC_UNDEFINED,            106                     },    // <I52>    <K72>
    /* 211 */    { VC_UNDEFINED,            107                     },    // <I53>    <K73>
    /* 212 */    { VC_UNDEFINED,              0                     },    // <I54>
    /* 213 */    { VC_UNDEFINED,              0                     },    // <I55>
    /* 214 */    { VC_UNDEFINED,              0                     },    // <I56>
    /* 215 */    { VC_UNDEFINED,              0                     },    // <I57>
    /* 216 */    { VC_UNDEFINED,              0                     },    // <I58>
    /* 217 */    { VC_UNDEFINED,              0                     },    // <I59>
    /* 218 */    { VC_UNDEFINED,              0                     },    // <I5A>
    /* 219 */    { VC_UNDEFINED,            115    /* <LWIN>   */   },    // <I5B>    <K74>
    /* 220 */    { VC_UNDEFINED,            116    /* <RWIN>   */   },    // <I5C>    <K75>
    /* 221 */    { VC_UNDEFINED,            117    /* <MENU>   */   },    // <I5D>    <K76>
    /* 222 */    { VC_UNDEFINED,              0                     },    // <I5E>
    /* 223 */    { VC_UNDEFINED,              0                     },    // <I5F>
    /* 224 */    { VC_UNDEFINED,              0                     },    // <I60>
    /* 225 */    { VC_UNDEFINED,              0                     },    // <I61>
    /* 226 */    { VC_UNDEFINED,              0                     },    // <I62>
    /* 227 */    { VC_UNDEFINED,              0                     },    // <I63>
    /* 228 */    { VC_UNDEFINED,              0                     },    // <I64>
    /* 229 */    { VC_UNDEFINED,              0                     },    // <I65>
    /* 230 */    { VC_UNDEFINED,              0                     },    // <I66>
    /* 231 */    { VC_UNDEFINED,              0                     },    // <I67>
    /* 232 */    { VC_UNDEFINED,              0                     },    // <I68>
    /* 233 */    { VC_UNDEFINED,              0                     },    // <I69>
    /* 234 */    { VC_UNDEFINED,              0                     },    // <I6A>
    /* 235 */    { VC_UNDEFINED,              0                     },    // <I6B>
    /* 236 */    { VC_UNDEFINED,              0                     },    // <I6C>
    /* 237 */    { VC_UNDEFINED,              0                     },    // <I6D>
    /* 238 */    { VC_UNDEFINED,              0                     },    // <I6E>
    /* 239 */    { VC_UNDEFINED,              0                     },    // <I6F>
    /* 240 */    { VC_UNDEFINED,              0                     },    // <I70>
    /* 241 */    { VC_UNDEFINED,              0                     },    // <I71>
    /* 242 */    { VC_UNDEFINED,              0                     },    // <I72>
    /* 243 */    { VC_UNDEFINED,              0                     },    // <I73>
    /* 244 */    { VC_UNDEFINED,              0                     },    // <I74>
    /* 245 */    { VC_UNDEFINED,              0                     },    // <I75>
    /* 246 */    { VC_UNDEFINED,              0                     },    // <I76>
    /* 247 */    { VC_UNDEFINED,              0                     },    // <I77>
    /* 248 */    { VC_UNDEFINED,              0                     },    // <I78>
    /* 249 */    { VC_UNDEFINED,              0                     },    // <I79>
    /* 250 */    { VC_UNDEFINED,              0                     },    // <I7A>
    /* 251 */    { VC_UNDEFINED,              0                     },    // <I7B>
    /* 252 */    { VC_UNDEFINED,              0                     },    // <I7C>
    /* 253 */    { VC_UNDEFINED,              0                     },    // <I7D>
    /* 254 */    { VC_UNDEFINED,              0                     },    // <I7E>
    /* 255 */    { VC_UNDEFINED,              0                     },    // <I7F>
 };


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

    #ifdef USE_EVDEV
    // Check to see if evdev is available.
    if (is_evdev) {
        unsigned short evdev_size = sizeof(evdev_scancode_table) / sizeof(evdev_scancode_table[0]);

        // NOTE scancodes < 97 appear to be identical between Evdev and XFree86.
        if (keycode < evdev_size) {
            // For scancode < 97, a simple scancode - 8 offest could be applied,
            // but math is generally slower than memory and we cannot save any
            // extra space in the lookup table due to binary padding.
            scancode = evdev_scancode_table[keycode][0];
        }
    } else {
    #endif
        // Evdev was unavailable, fallback to XFree86.
        unsigned short xfree86_size = sizeof(xfree86_scancode_table) / sizeof(xfree86_scancode_table[0]);

        // NOTE scancodes < 97 appear to be identical between Evdev and XFree86.
        if (keycode < xfree86_size) {
            // For scancode < 97, a simple scancode - 8 offest could be applied,
            // but math is generally slower than memory and we cannot save any
            // extra space in the lookup table due to binary padding.
            scancode = xfree86_scancode_table[keycode][0];
        }
    #ifdef USE_EVDEV
    }
    #endif

    return scancode;
}

KeyCode scancode_to_keycode(uint16_t scancode) {
    KeyCode keycode = 0x0000;

    #ifdef USE_EVDEV
    // Check to see if evdev is available.
    if (is_evdev) {
        unsigned short evdev_size = sizeof(evdev_scancode_table) / sizeof(evdev_scancode_table[0]);

        // NOTE scancodes < 97 appear to be identical between Evdev and XFree86.
        if (scancode < 128) {
            // For scancode < 97, a simple scancode + 8 offest could be applied,
            // but math is generally slower than memory and we cannot save any
            // extra space in the lookup table due to binary padding.
            keycode = evdev_scancode_table[scancode][1];
        }
        else {
            // Offset is the lower order bits + 128
            scancode = (scancode & 0x007F) | 0x80;

            if (scancode < evdev_size) {
                keycode = evdev_scancode_table[scancode][1];
            }
        }
    } else {
    #endif
        // Evdev was unavailable, fallback to XFree86.
        unsigned short xfree86_size = sizeof(xfree86_scancode_table) / sizeof(xfree86_scancode_table[0]);

        // NOTE scancodes < 97 appear to be identical between Evdev and XFree86.
        if (scancode < 128) {
            // For scancode < 97, a simple scancode + 8 offest could be applied,
            // but math is generally slower than memory and we cannot save any
            // extra space in the lookup table due to binary padding.
            keycode = xfree86_scancode_table[scancode][1];
        } else {
            // Offset: lower order bits + 128 (If no size optimization!)
            scancode = (scancode & 0x007F) | 0x80;

            if (scancode < xfree86_size) {
                keycode = xfree86_scancode_table[scancode][1];
            }
        }
    #ifdef USE_EVDEV
    }
    #endif

    return keycode;
}

// Set the native modifier mask for future events.
void set_modifier_mask(uint16_t mask) {
    modifier_mask |= mask;
}

// Unset the native modifier mask for future events.
void unset_modifier_mask(uint16_t mask) {
    modifier_mask &= ~mask;
}

// Get the current native modifier mask state.
uint16_t get_modifiers() {
    return modifier_mask;
}

// Initialize the modifier lock masks.
static void initialize_locks() {
    unsigned int led_mask = 0x00;
    if (XkbGetIndicatorState(helper_disp, XkbUseCoreKbd, &led_mask) == Success) {
        if (led_mask & 0x01) {
            set_modifier_mask(MASK_CAPS_LOCK);
        } else {
            unset_modifier_mask(MASK_CAPS_LOCK);
        }

        if (led_mask & 0x02) {
            set_modifier_mask(MASK_NUM_LOCK);
        } else {
            unset_modifier_mask(MASK_NUM_LOCK);
        }

        if (led_mask & 0x04) {
            set_modifier_mask(MASK_SCROLL_LOCK);
        } else {
            unset_modifier_mask(MASK_SCROLL_LOCK);
        }
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: XkbGetIndicatorState failed to get current led mask!\n",
                __FUNCTION__, __LINE__);
    }
}

// Initialize the modifier mask to the current modifiers.
static void initialize_modifiers() {
    modifier_mask = 0x0000;

    KeyCode keycode;
    char keymap[32];
    XQueryKeymap(helper_disp, keymap);

    Window unused_win;
    int unused_int;
    unsigned int mask;
    if (XQueryPointer(helper_disp, DefaultRootWindow(helper_disp), &unused_win, &unused_win, &unused_int, &unused_int, &unused_int, &unused_int, &mask)) {
        if (mask & ShiftMask) {
            keycode = XKeysymToKeycode(helper_disp, XK_Shift_L);
            if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_SHIFT_L); }
            keycode = XKeysymToKeycode(helper_disp, XK_Shift_R);
            if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_SHIFT_R); }
        }
        if (mask & ControlMask) {
            keycode = XKeysymToKeycode(helper_disp, XK_Control_L);
            if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_CTRL_L);  }
            keycode = XKeysymToKeycode(helper_disp, XK_Control_R);
            if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_CTRL_R);  }
        }
        if (mask & Mod1Mask) {
            keycode = XKeysymToKeycode(helper_disp, XK_Alt_L);
            if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_ALT_L);   }
            keycode = XKeysymToKeycode(helper_disp, XK_Alt_R);
            if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_ALT_R);   }
        }
        if (mask & Mod4Mask) {
            keycode = XKeysymToKeycode(helper_disp, XK_Super_L);
            if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_META_L);  }
            keycode = XKeysymToKeycode(helper_disp, XK_Super_R);
            if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_META_R);  }
        }

        if (mask & Button1Mask) { set_modifier_mask(MASK_BUTTON1); }
        if (mask & Button2Mask) { set_modifier_mask(MASK_BUTTON2); }
        if (mask & Button3Mask) { set_modifier_mask(MASK_BUTTON3); }
        if (mask & Button4Mask) { set_modifier_mask(MASK_BUTTON4); }
        if (mask & Button5Mask) { set_modifier_mask(MASK_BUTTON5); }
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: XQueryPointer failed to get current modifiers!\n",
                __FUNCTION__, __LINE__);

        keycode = XKeysymToKeycode(helper_disp, XK_Shift_L);
        if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_SHIFT_L); }
        keycode = XKeysymToKeycode(helper_disp, XK_Shift_R);
        if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_SHIFT_R); }
        keycode = XKeysymToKeycode(helper_disp, XK_Control_L);
        if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_CTRL_L);  }
        keycode = XKeysymToKeycode(helper_disp, XK_Control_R);
        if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_CTRL_R);  }
        keycode = XKeysymToKeycode(helper_disp, XK_Alt_L);
        if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_ALT_L);   }
        keycode = XKeysymToKeycode(helper_disp, XK_Alt_R);
        if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_ALT_R);   }
        keycode = XKeysymToKeycode(helper_disp, XK_Super_L);
        if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_META_L);  }
        keycode = XKeysymToKeycode(helper_disp, XK_Super_R);
        if (keymap[keycode / 8] & (1 << (keycode % 8))) { set_modifier_mask(MASK_META_R);  }
    }

    initialize_locks();
}

#ifdef USE_EPOCH_TIME
uint64_t get_unix_timestamp() {
    struct timeval system_time;

    // Get the local system time in UTC.
    gettimeofday(&system_time, NULL);

    // Convert the local system time to a Unix epoch in MS.
    uint64_t timestamp = (system_time.tv_sec * 1000) + (system_time.tv_usec / 1000);

    return timestamp;
}
#endif

unsigned int button_map_lookup(unsigned int button) {
    unsigned int map_button = button;

    if (helper_disp != NULL) {
        if (mouse_button_map != NULL) {
            int map_size = XGetPointerMapping(helper_disp, mouse_button_map, BUTTON_MAP_MAX);
            if (map_button > 0 && map_button <= map_size) {
                map_button = mouse_button_map[map_button -1];
            }
        } else {
            logger(LOG_LEVEL_WARN, "%s [%u]: Mouse button map memory is unavailable!\n",
                    __FUNCTION__, __LINE__);
        }
    } else {
        logger(LOG_LEVEL_WARN, "%s [%u]: XDisplay helper_disp is unavailable!\n",
                __FUNCTION__, __LINE__);
    }

    // X11 numbers buttons 2 & 3 backwards from other platforms so we normalize them.
    if      (map_button == Button2) { map_button = Button3; }
    else if (map_button == Button3) { map_button = Button2; }

    return map_button;
}

void load_input_helper() {
    /*
    int min_keycodes, max_keycodes;
    XDisplayKeycodes(helper_disp, &min_keycodes, &max_keycodes);

    int keysyms_per_keycode;
    KeySym *keysym_map = XGetKeyboardMapping(helper_disp, min_keycodes, (max_keycodes - min_keycodes + 1), &keysyms_per_keycode);

    unsigned int event_mask = ShiftMask | LockMask;
    KeySym keysym = KeyCodeToKeySym(display, keycode, event_mask);
    printf("KeySym: %s\n", XKeysymToString(keysym));

    XFree(keysym_map);
    */

    // Setup memory for mouse button mapping.
    mouse_button_map = malloc(sizeof(unsigned char) * BUTTON_MAP_MAX);
    if (mouse_button_map == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to allocate memory for mouse button map!\n",
                __FUNCTION__, __LINE__);

        //return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }

    /* The following code block is based on vncdisplaykeymap.c under the terms:
     *
     * Copyright (C) 2008  Anthony Liguori <anthony codemonkey ws>
     *
     * This program is free software; you can redistribute it and/or modify
     * it under the terms of the GNU Lesser General Public License version 2 as
     * published by the Free Software Foundation.
     */
    XkbDescPtr desc = XkbGetKeyboard(helper_disp, XkbGBN_AllComponentsMask, XkbUseCoreKbd);
    if (desc != NULL && desc->names != NULL) {
        const char *layout_name = XGetAtomName(helper_disp, desc->names->keycodes);
        logger(LOG_LEVEL_DEBUG, "%s [%u]: Found keycode atom '%s' (%i)!\n",
                __FUNCTION__, __LINE__, layout_name, (unsigned int) desc->names->keycodes);

        const char *prefix_xfree86 = "xfree86_";
        #ifdef USE_EVDEV
        const char *prefix_evdev = "evdev_";
        if (strncmp(layout_name, prefix_evdev, strlen(prefix_evdev)) == 0) {
            is_evdev = true;
        } else
        #endif
        if (strncmp(layout_name, prefix_xfree86, strlen(prefix_xfree86)) != 0) {
            logger(LOG_LEVEL_ERROR, "%s [%u]: Unknown keycode name '%s', please file a bug report!\n",
                    __FUNCTION__, __LINE__, layout_name);
        } else if (layout_name == NULL) {
            logger(LOG_LEVEL_ERROR, "%s [%u]: X atom name failure for desc->names->keycodes!\n",
                    __FUNCTION__, __LINE__);
        }

        XkbFreeClientMap(desc, XkbGBN_AllComponentsMask, True);
    } else {
        logger(LOG_LEVEL_ERROR, "%s [%u]: XkbGetKeyboard failed to locate a valid keyboard!\n",
                __FUNCTION__, __LINE__);
    }

    // Get the map.
    keyboard_map = XkbGetMap(helper_disp, XkbAllClientInfoMask, XkbUseCoreKbd);
}

void unload_input_helper() {
    if (keyboard_map != NULL) {
        XkbFreeClientMap(keyboard_map, XkbAllClientInfoMask, true);
        #ifdef USE_EVDEV
        is_evdev = false;
        #endif
    }

    if (mouse_button_map != NULL) {
        free(mouse_button_map);
        mouse_button_map = NULL;
    }
}
