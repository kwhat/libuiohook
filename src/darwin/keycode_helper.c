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

#include <uiohook.h>

#include "keycode_helper.h"

static bool mouse_dragged = false;
static uint16_t modifier_mask;

// Indexed by macOS hardware key code. Public VC_* values are not indices.
static const uint16_t keycode_uiocode_table[] = {
    /*   0 */ VC_A,
    /*   1 */ VC_S,
    /*   2 */ VC_D,
    /*   3 */ VC_F,
    /*   4 */ VC_H,
    /*   5 */ VC_G,
    /*   6 */ VC_Z,
    /*   7 */ VC_X,
    /*   8 */ VC_C,
    /*   9 */ VC_V,
    /*  10 */ VC_UNDEFINED,
    /*  11 */ VC_B,
    /*  12 */ VC_Q,
    /*  13 */ VC_W,
    /*  14 */ VC_E,
    /*  15 */ VC_R,
    /*  16 */ VC_Y,
    /*  17 */ VC_T,
    /*  18 */ VC_1,
    /*  19 */ VC_2,
    /*  20 */ VC_3,
    /*  21 */ VC_4,
    /*  22 */ VC_6,
    /*  23 */ VC_5,
    /*  24 */ VC_EQUALS,
    /*  25 */ VC_9,
    /*  26 */ VC_7,
    /*  27 */ VC_MINUS,
    /*  28 */ VC_8,
    /*  29 */ VC_0,
    /*  30 */ VC_CLOSE_BRACKET,
    /*  31 */ VC_O,
    /*  32 */ VC_U,
    /*  33 */ VC_OPEN_BRACKET,
    /*  34 */ VC_I,
    /*  35 */ VC_P,
    /*  36 */ VC_ENTER,
    /*  37 */ VC_L,
    /*  38 */ VC_J,
    /*  39 */ VC_QUOTE,
    /*  40 */ VC_K,
    /*  41 */ VC_SEMICOLON,
    /*  42 */ VC_BACK_SLASH,
    /*  43 */ VC_COMMA,
    /*  44 */ VC_SLASH,
    /*  45 */ VC_N,
    /*  46 */ VC_M,
    /*  47 */ VC_PERIOD,
    /*  48 */ VC_TAB,
    /*  49 */ VC_SPACE,
    /*  50 */ VC_BACKQUOTE,
    /*  51 */ VC_BACKSPACE,
    /*  52 */ VC_UNDEFINED,
    /*  53 */ VC_ESCAPE,
    /*  54 */ VC_META_R,
    /*  55 */ VC_META_L,
    /*  56 */ VC_SHIFT_L,
    /*  57 */ VC_CAPS_LOCK,
    /*  58 */ VC_ALT_L,
    /*  59 */ VC_CONTROL_L,
    /*  60 */ VC_SHIFT_R,
    /*  61 */ VC_ALT_R,
    /*  62 */ VC_CONTROL_R,
    /*  63 */ VC_UNDEFINED,
    /*  64 */ VC_F17,
    /*  65 */ VC_KP_SEPARATOR,
    /*  66 */ VC_UNDEFINED,
    /*  67 */ VC_KP_MULTIPLY,
    /*  68 */ VC_UNDEFINED,
    /*  69 */ VC_KP_ADD,
    /*  70 */ VC_UNDEFINED,
    /*  71 */ VC_NUM_LOCK,
    /*  72 */ VC_VOLUME_UP,
    /*  73 */ VC_VOLUME_DOWN,
    /*  74 */ VC_VOLUME_MUTE,
    /*  75 */ VC_KP_DIVIDE,
    /*  76 */ VC_KP_ENTER,
    /*  77 */ VC_UNDEFINED,
    /*  78 */ VC_KP_SUBTRACT,
    /*  79 */ VC_F18,
    /*  80 */ VC_F19,
    /*  81 */ VC_KP_EQUALS,
    /*  82 */ VC_KP_0,
    /*  83 */ VC_KP_1,
    /*  84 */ VC_KP_2,
    /*  85 */ VC_KP_3,
    /*  86 */ VC_KP_4,
    /*  87 */ VC_KP_5,
    /*  88 */ VC_KP_6,
    /*  89 */ VC_KP_7,
    /*  90 */ VC_F20,
    /*  91 */ VC_KP_8,
    /*  92 */ VC_KP_9,
    /*  93 */ VC_YEN,
    /*  94 */ VC_UNDERSCORE,
    /*  95 */ VC_KP_COMMA,
    /*  96 */ VC_F5,
    /*  97 */ VC_F6,
    /*  98 */ VC_F7,
    /*  99 */ VC_F3,
    /* 100 */ VC_F8,
    /* 101 */ VC_F9,
    /* 102 */ VC_UNDEFINED,
    /* 103 */ VC_F11,
    /* 104 */ VC_KATAKANA,
    /* 105 */ VC_F13,
    /* 106 */ VC_F16,
    /* 107 */ VC_F14,
    /* 108 */ VC_UNDEFINED,
    /* 109 */ VC_F10,
    /* 110 */ VC_UNDEFINED,
    /* 111 */ VC_F12,
    /* 112 */ VC_UNDEFINED,
    /* 113 */ VC_F15,
    /* 114 */ VC_INSERT,
    /* 115 */ VC_HOME,
    /* 116 */ VC_PAGE_UP,
    /* 117 */ VC_DELETE,
    /* 118 */ VC_F4,
    /* 119 */ VC_END,
    /* 120 */ VC_F2,
    /* 121 */ VC_PAGE_DOWN,
    /* 122 */ VC_F1,
    /* 123 */ VC_LEFT,
    /* 124 */ VC_RIGHT,
    /* 125 */ VC_DOWN,
    /* 126 */ VC_UP,
    /* 127 */ VC_UNDEFINED,
    /* 128 */ VC_UNDEFINED,
    /* 129 */ VC_UNDEFINED,
    /* 130 */ VC_UNDEFINED,
    /* 131 */ VC_UNDEFINED,
    /* 132 */ VC_UNDEFINED,
    /* 133 */ VC_UNDEFINED,
    /* 134 */ VC_UNDEFINED,
    /* 135 */ VC_UNDEFINED,
    /* 136 */ VC_UNDEFINED,
    /* 137 */ VC_UNDEFINED,
    /* 138 */ VC_UNDEFINED,
    /* 139 */ VC_UNDEFINED,
    /* 140 */ VC_UNDEFINED,
    /* 141 */ VC_UNDEFINED,
    /* 142 */ VC_UNDEFINED,
    /* 143 */ VC_UNDEFINED,
    /* 144 */ VC_UNDEFINED,
    /* 145 */ VC_UNDEFINED,
    /* 146 */ VC_UNDEFINED,
    /* 147 */ VC_UNDEFINED,
    /* 148 */ VC_UNDEFINED,
    /* 149 */ VC_UNDEFINED,
    /* 150 */ VC_UNDEFINED,
    /* 151 */ VC_UNDEFINED,
    /* 152 */ VC_UNDEFINED,
    /* 153 */ VC_UNDEFINED,
    /* 154 */ VC_UNDEFINED,
    /* 155 */ VC_UNDEFINED,
    /* 156 */ VC_UNDEFINED,
    /* 157 */ VC_UNDEFINED,
    /* 158 */ VC_UNDEFINED,
    /* 159 */ VC_UNDEFINED,
    /* 160 */ VC_UNDEFINED,
    /* 161 */ VC_UNDEFINED,
    /* 162 */ VC_UNDEFINED,
    /* 163 */ VC_UNDEFINED,
    /* 164 */ VC_UNDEFINED,
    /* 165 */ VC_UNDEFINED,
    /* 166 */ VC_UNDEFINED,
    /* 167 */ VC_UNDEFINED,
    /* 168 */ VC_UNDEFINED,
    /* 169 */ VC_UNDEFINED,
    /* 170 */ VC_UNDEFINED,
    /* 171 */ VC_UNDEFINED,
    /* 172 */ VC_UNDEFINED,
    /* 173 */ VC_UNDEFINED,
    /* 174 */ VC_UNDEFINED,
    /* 175 */ VC_UNDEFINED,
    /* 176 */ VC_UNDEFINED,
    /* 177 */ VC_UNDEFINED,
    /* 178 */ VC_UNDEFINED,
    /* 179 */ VC_UNDEFINED,
    /* 180 */ VC_UNDEFINED,
    /* 181 */ VC_UNDEFINED,
    /* 182 */ VC_UNDEFINED,
    /* 183 */ VC_UNDEFINED,
    /* 184 */ VC_UNDEFINED,
    /* 185 */ VC_UNDEFINED,
    /* 186 */ VC_UNDEFINED,
    /* 187 */ VC_UNDEFINED,
    /* 188 */ VC_UNDEFINED,
    /* 189 */ VC_UNDEFINED,
    /* 190 */ VC_UNDEFINED,
    /* 191 */ VC_UNDEFINED,
    /* 192 */ VC_UNDEFINED,
    /* 193 */ VC_UNDEFINED,
    /* 194 */ VC_UNDEFINED,
    /* 195 */ VC_UNDEFINED,
    /* 196 */ VC_UNDEFINED,
    /* 197 */ VC_UNDEFINED,
    /* 198 */ VC_UNDEFINED,
    /* 199 */ VC_UNDEFINED,
    /* 200 */ VC_UNDEFINED,
    /* 201 */ VC_UNDEFINED,
    /* 202 */ VC_UNDEFINED,
    /* 203 */ VC_UNDEFINED,
    /* 204 */ VC_UNDEFINED,
    /* 205 */ VC_UNDEFINED,
    /* 206 */ VC_UNDEFINED,
    /* 207 */ VC_UNDEFINED,
    /* 208 */ VC_UNDEFINED,
    /* 209 */ VC_UNDEFINED,
    /* 210 */ VC_UNDEFINED,
    /* 211 */ VC_UNDEFINED,
    /* 212 */ VC_UNDEFINED,
    /* 213 */ VC_UNDEFINED,
    /* 214 */ VC_UNDEFINED,
    /* 215 */ VC_UNDEFINED,
    /* 216 */ VC_UNDEFINED,
    /* 217 */ VC_UNDEFINED,
    /* 218 */ VC_UNDEFINED,
    /* 219 */ VC_UNDEFINED,
    /* 220 */ VC_UNDEFINED,
    /* 221 */ VC_UNDEFINED,
    /* 222 */ VC_UNDEFINED,
    /* 223 */ VC_UNDEFINED,
    /* 224 */ VC_UNDEFINED,
    /* 225 */ VC_UNDEFINED,
    /* 226 */ VC_LESSER_GREATER,
    /* 227 */ VC_UNDEFINED,
    /* 228 */ VC_UNDEFINED,
    /* 229 */ VC_UNDEFINED,
    /* 230 */ VC_POWER,
    /* 231 */ VC_UNDEFINED,
    /* 232 */ VC_UNDEFINED,
    /* 233 */ VC_UNDEFINED,
    /* 234 */ VC_UNDEFINED,
    /* 235 */ VC_UNDEFINED,
    /* 236 */ VC_UNDEFINED,
    /* 237 */ VC_UNDEFINED,
    /* 238 */ VC_MEDIA_EJECT,
    /* 239 */ VC_UNDEFINED,
    /* 240 */ VC_MEDIA_PLAY,
    /* 241 */ VC_MEDIA_NEXT,
    /* 242 */ VC_MEDIA_PREVIOUS,
    /* 243 */ VC_UNDEFINED,
    /* 244 */ VC_UNDEFINED,
    /* 245 */ VC_UNDEFINED,
    /* 246 */ VC_UNDEFINED,
    /* 247 */ VC_UNDEFINED,
    /* 248 */ VC_UNDEFINED,
    /* 249 */ VC_UNDEFINED,
    /* 250 */ VC_UNDEFINED,
    /* 251 */ VC_UNDEFINED,
    /* 252 */ VC_UNDEFINED,
    /* 253 */ VC_UNDEFINED,
    /* 254 */ VC_UNDEFINED,
    /* 255 */ VC_UNDEFINED,
};

uint16_t keycode_to_uiocode(uint16_t keycode) {
    if (keycode < sizeof(keycode_uiocode_table) / sizeof(keycode_uiocode_table[0])) {
        return keycode_uiocode_table[keycode];
    }
    return VC_UNDEFINED;
}

uint16_t uiocode_to_keycode(uint16_t uiocode) {
    // Unknown entries in the forward table must never become valid keycodes.
    if (uiocode != VC_UNDEFINED) {
        for (uint16_t keycode = 0; keycode < sizeof(keycode_uiocode_table) / sizeof(keycode_uiocode_table[0]); keycode++) {
            if (keycode_uiocode_table[keycode] == uiocode) {
                return keycode;
            }
        }
    }
    return DARWIN_KEYCODE_UNDEFINED;
}

bool is_mouse_dragged() {
    return mouse_dragged;
}

void set_mouse_dragged(bool dragged) {
    mouse_dragged = dragged;
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

