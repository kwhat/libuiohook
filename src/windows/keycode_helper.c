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
#include "logger.h"


static const uint16_t vkcode_uiocode_table[] = {
    // Indexed by Windows virtual-key code.
    /*   0 */ VC_UNDEFINED,                     // 0x00
    /*   1 */ MOUSE_BUTTON1,                    // 0x01
    /*   2 */ MOUSE_BUTTON2,                    // 0x02
    /*   3 */ VC_UNDEFINED,                     // 0x03 VK_CANCEL
    /*   4 */ MOUSE_BUTTON3,                    // 0x04
    /*   5 */ MOUSE_BUTTON4,                    // 0x05
    /*   6 */ MOUSE_BUTTON5,                    // 0x06
    /*   7 */ VC_UNDEFINED,                     // 0x07                        Undefined
    /*   8 */ VC_BACKSPACE,                     // 0x08 VK_BACK
    /*   9 */ VC_TAB,                           // 0x09 VK_TAB
    /*  10 */ VC_UNDEFINED,                     // 0x0A                        Reserved
    /*  11 */ VC_UNDEFINED,                     // 0x0B                        Reserved
    /*  12 */ VC_CLEAR,                         // 0x0C VK_CLEAR
    /*  13 */ VC_ENTER,                         // 0x0D VK_RETURN
    /*  14 */ VC_UNDEFINED,                     // 0x0E                        Undefined
    /*  15 */ VC_UNDEFINED,                     // 0x0F                        Undefined
    /*  16 */ VC_SHIFT_L,                       // 0x10 VK_SHIFT
    /*  17 */ VC_CONTROL_L,                     // 0x11 VK_CONTROL
    /*  18 */ VC_ALT_L,                         // 0x12 VK_MENU                ALT key
    /*  19 */ VC_PAUSE,                         // 0x13 VK_PAUSE
    /*  20 */ VC_CAPS_LOCK,                     // 0x14 VK_CAPITAL             CAPS LOCK key
    /*  21 */ VC_KATAKANA,                      // 0x15 VK_KANA                IME Kana mode
    /*  22 */ VC_UNDEFINED,                     // 0x16                        Undefined
    /*  23 */ VC_UNDEFINED,                     // 0x17 VK_JUNJA               IME Junja mode
    /*  24 */ VC_UNDEFINED,                     // 0x18 VK_FINAL
    /*  25 */ VC_KANJI,                         // 0x19 VK_KANJI / VK_HANJA    IME Kanji / Hanja mode
    /*  26 */ VC_UNDEFINED,                     // 0x1A Undefined
    /*  27 */ VC_ESCAPE,                        // 0x1B    VK_ESCAPE           ESC key
    /*  28 */ VC_UNDEFINED,                     // 0x1C VK_CONVERT             IME convert// 0x1C
    /*  29 */ VC_UNDEFINED,                     // 0x1D VK_NONCONVERT          IME nonconvert
    /*  30 */ VC_UNDEFINED,                     // 0x1E VK_ACCEPT              IME accept
    /*  31 */ VC_UNDEFINED,                     // 0x1F VK_MODECHANGE          IME mode change request
    /*  32 */ VC_SPACE,                         // 0x20 VK_SPACE               SPACEBAR
    /*  33 */ VC_PAGE_UP,                       // 0x21 VK_PRIOR               PAGE UP key
    /*  34 */ VC_PAGE_DOWN,                     // 0x22 VK_NEXT                PAGE DOWN key
    /*  35 */ VC_END,                           // 0x23 VK_END                 END key
    /*  36 */ VC_HOME,                          // 0x24 VK_HOME                HOME key
    /*  37 */ VC_LEFT,                          // 0x25 VK_LEFT                LEFT ARROW key
    /*  38 */ VC_UP,                            // 0x26 VK_UP                  UP ARROW key
    /*  39 */ VC_RIGHT,                         // 0x27 VK_RIGHT               RIGHT ARROW key
    /*  40 */ VC_DOWN,                          // 0x28 VK_DOWN                DOWN ARROW key
    /*  41 */ VC_UNDEFINED,                     // 0x29 VK_SELECT              SELECT key
    /*  42 */ VC_UNDEFINED,                     // 0x2A VK_PRINT               PRINT key
    /*  43 */ VC_UNDEFINED,                     // 0x2B VK_EXECUTE             EXECUTE key
    /*  44 */ VC_PRINTSCREEN,                   // 0x2C VK_SNAPSHOT            PRINT SCREEN key
    /*  45 */ VC_INSERT,                        // 0x2D VK_INSERT              INS key
    /*  46 */ VC_DELETE,                        // 0x2E VK_DELETE              DEL key
    /*  47 */ VC_UNDEFINED,                     // 0x2F VK_HELP                HELP key
    /*  48 */ VC_0,                             // 0x30                        0 key
    /*  49 */ VC_1,                             // 0x31                        1 key
    /*  50 */ VC_2,                             // 0x32                        2 key
    /*  51 */ VC_3,                             // 0x33                        3 key
    /*  52 */ VC_4,                             // 0x34                        4 key
    /*  53 */ VC_5,                             // 0x35                        5 key
    /*  54 */ VC_6,                             // 0x36                        6 key
    /*  55 */ VC_7,                             // 0x37                        7 key
    /*  56 */ VC_8,                             // 0x38                        8 key
    /*  57 */ VC_9,                             // 0x39                        9 key
    /*  58 */ VC_UNDEFINED,                     // 0x3A                        Undefined
    /*  59 */ VC_UNDEFINED,                     // 0x3B                        Undefined
    /*  60 */ VC_UNDEFINED,                     // 0x3C                        Undefined
    /*  61 */ VC_UNDEFINED,                     // 0x3D                        Undefined
    /*  62 */ VC_UNDEFINED,                     // 0x3E                        Undefined
    /*  63 */ VC_UNDEFINED,                     // 0x3F                        Undefined
    /*  64 */ VC_UNDEFINED,                     // 0x40                        Undefined
    /*  65 */ VC_A,                             // 0x41                        A key
    /*  66 */ VC_B,                             // 0x42                        B key
    /*  67 */ VC_C,                             // 0x43                        C key
    /*  68 */ VC_D,                             // 0x44                        D key
    /*  69 */ VC_E,                             // 0x45                        E key
    /*  70 */ VC_F,                             // 0x46                        F key
    /*  71 */ VC_G,                             // 0x47                        G key
    /*  72 */ VC_H,                             // 0x48                        H key
    /*  73 */ VC_I,                             // 0x49                        I key
    /*  74 */ VC_J,                             // 0x4A                        J key
    /*  75 */ VC_K,                             // 0x4B                        K key
    /*  76 */ VC_L,                             // 0x4C                        L key
    /*  77 */ VC_M,                             // 0x4D                        M key
    /*  78 */ VC_N,                             // 0x4E                        N key
    /*  79 */ VC_O,                             // 0x4F                        O key
    /*  80 */ VC_P,                             // 0x50                        P key
    /*  81 */ VC_Q,                             // 0x51                        Q key
    /*  82 */ VC_R,                             // 0x52                        R key
    /*  83 */ VC_S,                             // 0x53                        S key
    /*  84 */ VC_T,                             // 0x54                        T key
    /*  85 */ VC_U,                             // 0x55                        U key
    /*  86 */ VC_V,                             // 0x56                        V key
    /*  87 */ VC_W,                             // 0x57                        W key
    /*  88 */ VC_X,                             // 0x58                        X key
    /*  89 */ VC_Y,                             // 0x59                        Y key
    /*  90 */ VC_Z,                             // 0x5A                        Z key
    /*  91 */ VC_META_L,                        // 0x5B VK_LWIN                Left Windows key (Natural keyboard)
    /*  92 */ VC_META_R,                        // 0x5C VK_RWIN                Right Windows key (Natural keyboard)
    /*  93 */ VC_CONTEXT_MENU,                  // 0x5D VK_APPS                Applications key (Natural keyboard)
    /*  94 */ VC_UNDEFINED,                     // 0x5E Reserved
    /*  95 */ VC_SLEEP,                         // 0x5F VK_SLEEP               Computer Sleep key
    /*  96 */ VC_KP_0,                          // 0x60 VK_NUMPAD0             Numeric keypad 0 key
    /*  97 */ VC_KP_1,                          // 0x61 VK_NUMPAD1             Numeric keypad 1 key
    /*  98 */ VC_KP_2,                          // 0x62 VK_NUMPAD2             Numeric keypad 2 key
    /*  99 */ VC_KP_3,                          // 0x63 VK_NUMPAD3             Numeric keypad 3 key
    /* 100 */ VC_KP_4,                          // 0x64 VK_NUMPAD4             Numeric keypad 4 key
    /* 101 */ VC_KP_5,                          // 0x65 VK_NUMPAD5             Numeric keypad 5 key
    /* 102 */ VC_KP_6,                          // 0x66 VK_NUMPAD6             Numeric keypad 6 key
    /* 103 */ VC_KP_7,                          // 0x67 VK_NUMPAD7             Numeric keypad 7 key
    /* 104 */ VC_KP_8,                          // 0x68 VK_NUMPAD8             Numeric keypad 8 key
    /* 105 */ VC_KP_9,                          // 0x69 VK_NUMPAD9             Numeric keypad 9 key
    /* 106 */ VC_KP_MULTIPLY,                   // 0x6A VK_MULTIPLY            Multiply key
    /* 107 */ VC_KP_ADD,                        // 0x6B VK_ADD                 Add key
    /* 108 */ VC_UNDEFINED,                     // 0x6C VK_SEPARATOR           Separator key
    /* 109 */ VC_KP_SUBTRACT,                   // 0x6D VK_SUBTRACT            Subtract key
    /* 110 */ VC_KP_SEPARATOR,                  // 0x6E VK_DECIMAL             Decimal key
    /* 111 */ VC_KP_DIVIDE,                     // 0x6F VK_DIVIDE              Divide key
    /* 112 */ VC_F1,                            // 0x70 VK_F1                  F1 key
    /* 113 */ VC_F2,                            // 0x71 VK_F2                  F2 key
    /* 114 */ VC_F3,                            // 0x72 VK_F3                  F3 key
    /* 115 */ VC_F4,                            // 0x73 VK_F4                  F4 key
    /* 116 */ VC_F5,                            // 0x74 VK_F5                  F5 key
    /* 117 */ VC_F6,                            // 0x75 VK_F6                  F6 key
    /* 118 */ VC_F7,                            // 0x76 VK_F7                  F7 key
    /* 119 */ VC_F8,                            // 0x77 VK_F8                  F8 key
    /* 120 */ VC_F9,                            // 0x78 VK_F9                  F9 key
    /* 121 */ VC_F10,                           // 0x79 VK_F10                 F10 key
    /* 122 */ VC_F11,                           // 0x7A VK_F11                 F11 key
    /* 123 */ VC_F12,                           // 0x7B VK_F12                 F12 key
    /* 124 */ VC_F13,                           // 0x7C VK_F13                 F13 key
    /* 125 */ VC_F14,                           // 0x7D VK_F14                 F14 key
    /* 126 */ VC_F15,                           // 0x7E VK_F15                 F15 key
    /* 127 */ VC_F16,                           // 0x7F VK_F16                 F16 key


    /* 128 */ VC_F17,                         // 0x80 VK_F17                 F17 key
    /* 129 */ VC_F18,                           // 0x81 VK_F18                 F18 key
    /* 130 */ VC_F19,                           // 0x82 VK_F19                 F19 key
    /* 131 */ VC_F20,                           // 0x83 VK_F20                 F20 key
    /* 132 */ VC_F21,                           // 0x84 VK_F21                 F21 key
    /* 133 */ VC_F22,                           // 0x85 VK_F22                 F22 key
    /* 134 */ VC_F23,                           // 0x86 VK_F23                 F23 key
    /* 135 */ VC_F24,                           // 0x87 VK_F24                 F24 key
    /* 136 */ VC_UNDEFINED,                     // 0x88                        Unassigned
    /* 137 */ VC_UNDEFINED,                     // 0x89                        Unassigned
    /* 138 */ VC_UNDEFINED,                     // 0x8A                        Unassigned
    /* 139 */ VC_UNDEFINED,                     // 0x8B                        Unassigned
    /* 140 */ VC_UNDEFINED,                     // 0x8C                        Unassigned
    /* 141 */ VC_UNDEFINED,                     // 0x8D                        Unassigned
    /* 142 */ VC_UNDEFINED,                     // 0x8E                        Unassigned
    /* 143 */ VC_UNDEFINED,                     // 0x8F                        Unassigned
    /* 144 */ VC_NUM_LOCK,                      // 0x90 VK_NUMLOCK             NUM LOCK key
    /* 145 */ VC_SCROLL_LOCK,                   // 0x91 VK_SCROLL              SCROLL LOCK key
    /* 146 */ VC_UNDEFINED,                     // 0x92                        OEM specific
    /* 147 */ VC_UNDEFINED,                     // 0x93                        OEM specific
    /* 148 */ VC_UNDEFINED,                     // 0x94                        OEM specific
    /* 149 */ VC_UNDEFINED,                     // 0x95                        OEM specific
    /* 150 */ VC_UNDEFINED,                     // 0x96                        OEM specific
    /* 151 */ VC_UNDEFINED,                     // 0x97                        Unassigned
    /* 152 */ VC_UNDEFINED,                     // 0x98                        Unassigned
    /* 153 */ VC_UNDEFINED,                     // 0x99                        Unassigned
    /* 154 */ VC_UNDEFINED,                     // 0x9A                        Unassigned
    /* 155 */ VC_UNDEFINED,                     // 0x9B                        Unassigned
    /* 156 */ VC_UNDEFINED,                     // 0x9C                        Unassigned
    /* 157 */ VC_UNDEFINED,                     // 0x9D                        Unassigned
    /* 158 */ VC_UNDEFINED,                     // 0x9E                        Unassigned
    /* 159 */ VC_UNDEFINED,                     // 0x9F                        Unassigned
    /* 160 */ VC_SHIFT_L,                       // 0xA0 VK_LSHIFT              Left SHIFT key
    /* 161 */ VC_SHIFT_R,                       // 0xA1 VK_RSHIFT              Right SHIFT key
    /* 162 */ VC_CONTROL_L,                     // 0xA2 VK_LCONTROL            Left CONTROL key
    /* 163 */ VC_CONTROL_R,                     // 0xA3 VK_RCONTROL            Right CONTROL key
    /* 164 */ VC_ALT_L,                         // 0xA4 VK_LMENU               Left MENU key
    /* 165 */ VC_ALT_R,                         // 0xA5 VK_RMENU               Right MENU key
    /* 166 */ VC_BROWSER_BACK,                  // 0xA6 VK_BROWSER_BACK        Browser Back key
    /* 167 */ VC_BROWSER_FORWARD,               // 0xA7 VK_BROWSER_FORWARD     Browser Forward key
    /* 168 */ VC_BROWSER_REFRESH,               // 0xA8 VK_BROWSER_REFRESH     Browser Refresh key
    /* 169 */ VC_BROWSER_STOP,                  // 0xA9 VK_BROWSER_STOP        Browser Stop key
    /* 170 */ VC_BROWSER_SEARCH,                // 0xAA VK_BROWSER_SEARCH      Browser Search key
    /* 171 */ VC_BROWSER_FAVORITES,             // 0xAB VK_BROWSER_FAVORITES   Browser Favorites key
    /* 172 */ VC_BROWSER_HOME,                  // 0xAC VK_BROWSER_HOME        Browser Start and Home key
    /* 173 */ VC_VOLUME_MUTE,                   // 0xAD VK_VOLUME_MUTE         Volume Mute key
    /* 174 */ VC_VOLUME_DOWN,                   // 0xAE VK_VOLUME_DOWN         Volume Down key
    /* 175 */ VC_VOLUME_UP,                     // 0xAF VK_VOLUME_UP           Volume Up key
    /* 176 */ VC_MEDIA_NEXT,                    // 0xB0 VK_MEDIA_NEXT_TRACK    Next Track key
    /* 177 */ VC_MEDIA_PREVIOUS,                // 0xB1 VK_MEDIA_PREV_TRACK    Previous Track key
    /* 178 */ VC_MEDIA_STOP,                    // 0xB2 VK_MEDIA_STOP          Stop Media key
    /* 179 */ VC_MEDIA_PLAY,                    // 0xB3 VK_MEDIA_PLAY_PAUSE    Play/Pause Media key
    /* 180 */ VC_UNDEFINED,                     // 0xB4 VK_LAUNCH_MAIL         Start Mail key
    /* 181 */ VC_MEDIA_SELECT,                  // 0xB5 VK_LAUNCH_MEDIA_SELECT Select Media key
    /* 182 */ VC_APP_MAIL,                      // 0xB6 VK_LAUNCH_APP1         Start Application 1 key
    /* 183 */ VC_APP_CALCULATOR,                // 0xB7 VK_LAUNCH_APP2         Start Application 2 key
    /* 184 */ VC_UNDEFINED,                     // 0xB8                        Reserved
    /* 185 */ VC_UNDEFINED,                     // 0xB9                        Reserved
    /* 186 */ VC_SEMICOLON,                     // 0xBA VK_OEM_1               Varies by keyboard. For the US standard keyboard, the ';:' key
    /* 187 */ VC_EQUALS,                        // 0xBB VK_OEM_PLUS            For any country/region, the '+' key
    /* 188 */ VC_COMMA,                         // 0xBC VK_OEM_COMMA           For any country/region, the ',' key
    /* 189 */ VC_MINUS,                         // 0xBD VK_OEM_MINUS           For any country/region, the '-' key
    /* 190 */ VC_PERIOD,                        // 0xBE VK_OEM_PERIOD          For any country/region, the '.' key
    /* 191 */ VC_SLASH,                         // 0xBF VK_OEM_2               Varies by keyboard. For the US standard keyboard, the '/?' key
    /* 192 */ VC_BACKQUOTE,                     // 0xC0 VK_OEM_3               Varies by keyboard. For the US standard keyboard, the '`~' key
    /* 193 */ VC_UNDEFINED,                     // 0xC1                        Reserved
    /* 194 */ VC_UNDEFINED,                     // 0xC2                        Reserved
    /* 195 */ VC_UNDEFINED,                     // 0xC3                        Reserved
    /* 196 */ VC_UNDEFINED,                     // 0xC4                        Reserved
    /* 197 */ VC_UNDEFINED,                     // 0xC5                        Reserved
    /* 198 */ VC_UNDEFINED,                     // 0xC6                        Reserved
    /* 199 */ VC_UNDEFINED,                     // 0xC7                        Reserved
    /* 200 */ VC_UNDEFINED,                     // 0xC8                        Reserved
    /* 201 */ VC_UNDEFINED,                     // 0xC9                        Reserved
    /* 202 */ VC_UNDEFINED,                     // 0xCA                        Reserved
    /* 203 */ VC_UNDEFINED,                     // 0xCB                        Reserved
    /* 204 */ VC_UNDEFINED,                     // 0xCC                        Reserved
    /* 205 */ VC_UNDEFINED,                     // 0xCD                        Reserved
    /* 206 */ VC_UNDEFINED,                     // 0xCE                        Reserved
    /* 207 */ VC_UNDEFINED,                     // 0xCF                        Reserved
    /* 208 */ VC_UNDEFINED,                     // 0xD0                        Reserved
    /* 209 */ VC_UNDEFINED,                     // 0xD1                        Reserved
    /* 210 */ VC_UNDEFINED,                     // 0xD2                        Reserved
    /* 211 */ VC_UNDEFINED,                     // 0xD3                        Reserved
    /* 212 */ VC_UNDEFINED,                     // 0xD4                        Reserved
    /* 213 */ VC_UNDEFINED,                     // 0xD5                        Reserved
    /* 214 */ VC_UNDEFINED,                     // 0xD6                        Reserved
    /* 215 */ VC_UNDEFINED,                     // 0xD7                        Reserved
    /* 216 */ VC_UNDEFINED,                     // 0xD8                        Unassigned
    /* 217 */ VC_UNDEFINED,                     // 0xD9                        Unassigned
    /* 218 */ VC_UNDEFINED,                     // 0xDA                        Unassigned
    /* 219 */ VC_OPEN_BRACKET,                  // 0xDB VK_OEM_4               Varies by keyboard. For the US standard keyboard, the '[{' key
    /* 220 */ VC_BACK_SLASH,                    // 0xDC VK_OEM_5               Varies by keyboard. For the US standard keyboard, the '\|' key
    /* 221 */ VC_CLOSE_BRACKET,                 // 0xDD VK_OEM_6               Varies by keyboard. For the US standard keyboard, the ']}' key
    /* 222 */ VC_QUOTE,                         // 0xDE VK_OEM_7               Varies by keyboard. For the US standard keyboard, the 'single-quote/double-quote' key
    /* 223 */ VC_UNDEFINED,                     // 0xDF VK_OEM_8               Varies by keyboard.
    /* 224 */ VC_UNDEFINED,                     // 0xE0                        Reserved
    /* 225 */ VC_UNDEFINED,                     // 0xE1                        OEM specific
    /* 226 */ VC_UNDEFINED,                     // 0xE2 VK_OEM_102             Either the angle bracket key or the backslash key on the RT 102-key keyboard
    /* 227 */ VC_UNDEFINED,                     // 0xE3                        OEM specific
    /* 228 */ VC_UNDEFINED,                     // 0xE4    VC_APP_PICTURES     OEM specific
    /* 229 */ VC_APP_PICTURES,                  // 0xE5 VK_PROCESSKEY          IME PROCESS key
    /* 230 */ VC_APP_MUSIC,                     // 0xE6                        OEM specific
    /* 231 */ VC_UNDEFINED,                     // 0xE7 VK_PACKET              Used to pass Unicode characters as if they were keystrokes. The VK_PACKET key is the low word of a 32-bit Virtual Key value used for non-keyboard input methods.
    /* 232 */ VC_UNDEFINED,                     // 0xE8                        Unassigned
    /* 233 */ VC_UNDEFINED,                     // 0xE9                        OEM specific
    /* 234 */ VC_UNDEFINED,                     // 0xEA                        OEM specific
    /* 235 */ VC_UNDEFINED,                     // 0xEB                        OEM specific
    /* 236 */ VC_UNDEFINED,                     // 0xEC                        OEM specific
    /* 237 */ VC_UNDEFINED,                     // 0xED                        OEM specific
    /* 238 */ VC_UNDEFINED,                     // 0xEE                        OEM specific
    /* 239 */ VC_UNDEFINED,                     // 0xEF                        OEM specific
    /* 240 */ VC_UNDEFINED,                     // 0xF0                        OEM specific
    /* 241 */ VC_UNDEFINED,                     // 0xF1                        OEM specific
    /* 242 */ VC_UNDEFINED,                     // 0xF2                        OEM specific
    /* 243 */ VC_UNDEFINED,                     // 0xF3                        OEM specific
    /* 244 */ VC_UNDEFINED,                     // 0xF4                        OEM specific
    /* 245 */ VC_UNDEFINED,                     // 0xF5                        OEM specific
    /* 246 */ VC_UNDEFINED,                     // 0xF6 VK_ATTN                Attn key
    /* 247 */ VC_UNDEFINED,                     // 0xF7 VK_CRSEL               CrSel key
    /* 248 */ VC_UNDEFINED,                     // 0xF8 VK_EXSEL               ExSel key
    /* 249 */ VC_UNDEFINED,                     // 0xF9 VK_EREOF               Erase EOF key
    /* 250 */ VC_UNDEFINED,                     // 0xFA VK_PLAY                Play key
    /* 251 */ VC_UNDEFINED,                     // 0xFB VK_ZOOM                Zoom key
    /* 252 */ VC_UNDEFINED,                     // 0xFC VK_NONAME              Reserved
    /* 253 */ VC_UNDEFINED,                     // 0xFD
    /* 254 */ VC_KP_CLEAR,                      // 0xFE VK_OEM_CLEAR           Clear key
    /* 255 */ VC_UNDEFINED,                     // 0xFF                        Unassigned
};

uint16_t vkcode_to_uiocode(DWORD vk_code, DWORD flags) {
    uint16_t uiocode = VC_UNDEFINED;

    // Check the vk_code is in range.
    // NOTE vk_code >= 0 is assumed because DWORD is unsigned.
    if (vk_code < sizeof(vkcode_uiocode_table) / sizeof(vkcode_uiocode_table[0])) {
        uiocode = vkcode_uiocode_table[vk_code];

        // Check the extended key flag as defined at:
        // https://learn.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input#extended-key-flag
        if (flags & LLKHF_EXTENDED) {
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Using extended lookup for vk_code: %li\n",
                    __FUNCTION__, __LINE__, vk_code);

            switch (vk_code) {
                case VK_RETURN:
                    uiocode = VC_KP_ENTER;
                    break;
            }
        } else {
            logger(LOG_LEVEL_DEBUG, "%s [%u]: Using normal lookup for vk_code: %li\n",
                    __FUNCTION__, __LINE__, vk_code);

            switch (vk_code) {
                case VK_PRIOR:
                case VK_NEXT:
                case VK_END:
                case VK_HOME:
                case VK_LEFT:
                case VK_UP:
                case VK_RIGHT:
                case VK_DOWN:

                case VK_INSERT:
                case VK_DELETE:
                    uiocode |= 0xEE00;
                    break;
            }
        }
    }

    return uiocode;
}

DWORD uiocode_to_vkcode(uint16_t uiocode) {
    // Generic and left/right modifier virtual keys share forward mappings.
    // Prefer the explicit side when posting a uiohook modifier key.
    switch (uiocode) {
        case VC_UNDEFINED: return 0;
        case VC_SHIFT_L:   return VK_LSHIFT;
        case VC_SHIFT_R:   return VK_RSHIFT;
        case VC_CONTROL_L: return VK_LCONTROL;
        case VC_CONTROL_R: return VK_RCONTROL;
        case VC_ALT_L:     return VK_LMENU;
        case VC_ALT_R:     return VK_RMENU;
        case VC_KP_ENTER:  return VK_RETURN;
    }

    // Keypad navigation uses the same Windows VK codes as the dedicated keys;
    // post_event.c supplies the extended-key distinction when injecting them.
    if ((uiocode & 0xFF00) == 0xEE00) {
        uiocode &= 0x00FF;
    }

    // Public VC_* values are not scan-code indices. Find the code in the
    // forward table rather than indexing the obsolete scan-code reverse map.
    for (DWORD vkcode = 0; vkcode < sizeof(vkcode_uiocode_table) / sizeof(vkcode_uiocode_table[0]); vkcode++) {
        if (vkcode_uiocode_table[vkcode] == uiocode) {
            return vkcode;
        }
    }

    return 0;
}
