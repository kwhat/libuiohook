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

#include <stdbool.h>
#include <stdint.h>

#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include <X11/ap_keysym.h>
#include <X11/DECkeysym.h>
#include <X11/HPkeysym.h>
#include <X11/Sunkeysym.h>
#include <X11/XF86keysym.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifndef osfXK_Prior
#define osfXK_Prior 0x1004FF55
#endif

#ifndef osfXK_Next
#define osfXK_Next 0x1004FF56
#endif

#include <X11/XKBlib.h>
static XkbDescPtr keyboard_map;

#include "input_helper.h"
#include "logger.h"

#define BUTTON_TABLE_MAX 256

static unsigned char *mouse_button_table;
Display *helper_disp;  // Where do we open this display?  FIXME Use the ctrl display via init param

static uint16_t modifier_mask;

static const uint32_t uiocode_keysym_table[][2] = {
    /* idx       { keycode,                 scancode                  }, */
    /*   0 */    { VC_ESCAPE,                XK_Escape,               }, // 0x000
    /*   1 */    { VC_ESCAPE,                osfXK_Escape             }, // 0x001 HP OSF KeySym in HPkeysym.h

    // Begin Function Keys
    /*   2 */    { VC_F1,                    XK_F1                    }, // 0x002
    /*   3 */    { VC_F2,                    XK_F2                    }, // 0x003
    /*   4 */    { VC_F3,                    XK_F3                    }, // 0x004
    /*   5 */    { VC_F4,                    XK_F4                    }, // 0x005
    /*   6 */    { VC_F5,                    XK_F5                    }, // 0x006
    /*   7 */    { VC_F6,                    XK_F6                    }, // 0x007
    /*   8 */    { VC_F7,                    XK_F7                    }, // 0x008
    /*   9 */    { VC_F8,                    XK_F8                    }, // 0x009
    /*  10 */    { VC_F9,                    XK_F9                    }, // 0x00A
    /*  11 */    { VC_F10,                   XK_F10                   }, // 0x00B
    /*  12 */    { VC_F11,                   XK_F11                   }, // 0x00C
    /*  13 */    { VC_F11,                   SunXK_F36                }, // 0x00D Labeled F11 in Sunkeysym.h
    /*  14 */    { VC_F12,                   XK_F12                   }, // 0x00E
    /*  15 */    { VC_F12,                   SunXK_F37                }, // 0x00F Labeled F12 in Sunkeysym.h

    /*  16 */    { VC_F13,                   XK_F13                   }, // 0x010
    /*  17 */    { VC_F14,                   XK_F14                   }, // 0x011
    /*  18 */    { VC_F15,                   XK_F15                   }, // 0x012
    /*  19 */    { VC_F16,                   XK_F16                   }, // 0x013
    /*  20 */    { VC_F17,                   XK_F17                   }, // 0x014
    /*  21 */    { VC_F18,                   XK_F18                   }, // 0x015
    /*  22 */    { VC_F19,                   XK_F19                   }, // 0x016
    /*  23 */    { VC_F20,                   XK_F20                   }, // 0x017
    /*  24 */    { VC_F21,                   XK_F21                   }, // 0x018
    /*  25 */    { VC_F22,                   XK_F22                   }, // 0x019
    /*  26 */    { VC_F23,                   XK_F23                   }, // 0x01A
    /*  27 */    { VC_F24,                   XK_F24                   }, // 0x01B
    // End Function Keys


    // Begin Alphanumeric Zone
    /*  28 */    { VC_BACK_QUOTE,            XK_grave                 }, // 0x01C

    /*  29 */    { VC_0,                     XK_0                     }, // 0x01D
    /*  30 */    { VC_1,                     XK_1                     }, // 0x01E
    /*  31 */    { VC_2,                     XK_2                     }, // 0x01F
    /*  32 */    { VC_3,                     XK_3                     }, // 0x020
    /*  33 */    { VC_4,                     XK_4                     }, // 0x021
    /*  34 */    { VC_5,                     XK_5                     }, // 0x022
    /*  35 */    { VC_6,                     XK_6                     }, // 0x023
    /*  36 */    { VC_7,                     XK_7                     }, // 0x024
    /*  37 */    { VC_8,                     XK_8                     }, // 0x025
    /*  38 */    { VC_9,                     XK_9                     }, // 0x026

    /*  39 */    { VC_MINUS,                 XK_minus                 }, // 0x027
    /*  40 */    { VC_PLUS,                  XK_plus                  }, // 0x028
    /*  41 */    { VC_EQUALS,                XK_equal                 }, // 0x029
    /*  42 */    { VC_ASTERISK,              XK_asterisk              }, // 0x02A

    /*  43 */    { VC_AT,                    XK_at                    }, // 0x02B
    /*  44 */    { VC_AMPERSAND,             XK_ampersand             }, // 0x02C
    /*  45 */    { VC_DOLLAR,                XK_dollar                }, // 0x02D
    /*  46 */    { VC_EXCLAMATION_MARK,      XK_exclam                }, // 0x02E
    /*  47 */    { VC_EXCLAMATION_DOWN,      XK_exclamdown            }, // 0x02F

    /*  48 */    { VC_BACKSPACE,             XK_BackSpace             }, // 0x030
    /*  49 */    { VC_BACKSPACE,             osfXK_BackSpace          }, // 0x031 HP OSF KeySym in HPkeysym.h

    /*  50 */    { VC_TAB,                   XK_Tab                   }, // 0x032
    /*  51 */    { VC_TAB,                   XK_ISO_Left_Tab          }, // 0x033
    /*  52 */    { VC_CAPS_LOCK,             XK_Caps_Lock             }, // 0x034
    /*  53 */    { VC_CAPS_LOCK,             XK_Shift_Lock            }, // 0x035

    /*  54 */    { VC_A,                     XK_a                     }, // 0x036
    /*  55 */    { VC_B,                     XK_b                     }, // 0x037
    /*  56 */    { VC_C,                     XK_c                     }, // 0x038
    /*  57 */    { VC_D,                     XK_d                     }, // 0x039
    /*  58 */    { VC_E,                     XK_e                     }, // 0x03A
    /*  59 */    { VC_F,                     XK_f                     }, // 0x03B
    /*  60 */    { VC_G,                     XK_g                     }, // 0x03C
    /*  61 */    { VC_H,                     XK_h                     }, // 0x03D
    /*  62 */    { VC_I,                     XK_i                     }, // 0x03E
    /*  63 */    { VC_J,                     XK_j                     }, // 0x03F
    /*  64 */    { VC_K,                     XK_k                     }, // 0x040
    /*  65 */    { VC_L,                     XK_l                     }, // 0x041
    /*  66 */    { VC_M,                     XK_m                     }, // 0x042
    /*  67 */    { VC_N,                     XK_n                     }, // 0x043
    /*  68 */    { VC_O,                     XK_o                     }, // 0x044
    /*  69 */    { VC_P,                     XK_p                     }, // 0x045
    /*  70 */    { VC_Q,                     XK_q                     }, // 0x046
    /*  71 */    { VC_R,                     XK_r                     }, // 0x047
    /*  72 */    { VC_S,                     XK_s                     }, // 0x048
    /*  73 */    { VC_T,                     XK_t                     }, // 0x049
    /*  74 */    { VC_U,                     XK_u                     }, // 0x04A
    /*  75 */    { VC_V,                     XK_v                     }, // 0x04B
    /*  76 */    { VC_W,                     XK_w                     }, // 0x04C
    /*  77 */    { VC_X,                     XK_x                     }, // 0x04D
    /*  78 */    { VC_Y,                     XK_y                     }, // 0x04E
    /*  79 */    { VC_Z,                     XK_z                     }, // 0x04F

    /*  80 */    { VC_OPEN_BRACKET,          XK_bracketleft           }, // 0x050
    /*  81 */    { VC_CLOSE_BRACKET,         XK_bracketright          }, // 0x051
    /*  82 */    { VC_BACK_SLASH,            XK_backslash             }, // 0x052

    /*  83 */    { VC_COLON,                 XK_colon                 }, // 0x053
    /*  84 */    { VC_SEMICOLON,             XK_semicolon             }, // 0x054
    /*  85 */    { VC_QUOTE,                 XK_apostrophe            }, // 0x055
    /*  86 */    { VC_QUOTEDBL,              XK_quotedbl              }, // 0x056
    /*  87 */    { VC_ENTER,                 XK_Return,               }, // 0x057
    /*  88 */    { VC_ENTER,                 XK_Linefeed,             }, // 0x058

    /*  89 */    { VC_LESS,                  XK_less                  }, // 0x059
    /*  90 */    { VC_GREATER,               XK_greater               }, // 0x05A
    /*  91 */    { VC_COMMA,                 XK_comma                 }, // 0x05B
    /*  92 */    { VC_PERIOD,                XK_period                }, // 0x05C
    /*  93 */    { VC_SLASH,                 XK_slash                 }, // 0x05D
    /*  94 */    { VC_NUMBER_SIGN,           XK_numbersign            }, // 0x05E

    /*  95 */    { VC_OPEN_BRACE,            XK_braceleft             }, // 0x05F
    /*  96 */    { VC_CLOSE_BRACE,           XK_braceright            }, // 0x060

    /*  97 */    { VC_OPEN_PARENTHESIS,      XK_parenleft             }, // 0x061
    /*  98 */    { VC_CLOSE_PARENTHESIS,     XK_parenright            }, // 0x062

    /*  99 */    { VC_SPACE,                 XK_space                 }, // 0x063
    // End Alphanumeric Zone


    // Begin Edit Key Zone
    /* 100 */    { VC_PRINT_SCREEN,          XK_Print                 }, // 0x064
    /* 101 */    { VC_PRINT_SCREEN,          SunXK_Print_Screen       }, // 0x065 Same as XK_Print in Sunkeysym.h FIXME Remove
    /* 102 */    { VC_PRINT_SCREEN,          SunXK_Sys_Req            }, // 0x066 SysReq should be the same as Print Screen
    /* 103 */    { VC_SCROLL_LOCK,           XK_Scroll_Lock,          }, // 0x067
    /* 104 */    { VC_PAUSE,                 XK_Pause                 }, // 0x068
    /* 105 */    { VC_CANCEL,                XK_Cancel                }, // 0x069
    /* 106 */    { VC_CANCEL,                osfXK_Cancel             }, // 0x06A HP OSF KeySym in HPkeysym.h
    /* 107 */    { VC_INSERT,                XK_Insert                }, // 0x06B
    /* 108 */    { VC_INSERT,                osfXK_Insert             }, // 0x06C HP OSF KeySym in HPkeysym.h
    /* 109 */    { VC_DELETE,                XK_Delete                }, // 0x06D
    /* 110 */    { VC_DELETE,                osfXK_Delete             }, // 0x06E HP OSF KeySym in HPkeysym.h
    /* 111 */    { VC_HOME,                  XK_Home                  }, // 0x06F
    /* 112 */    { VC_END,                   XK_End                   }, // 0x070
    /* 113 */    { VC_END,                   osfXK_EndLine            }, // 0x071 HP OSF KeySym in HPkeysym.h
    /* 114 */    { VC_PAGE_UP,               XK_Page_Up               }, // 0x072
    /* 115 */    { VC_PAGE_UP,               XK_Prior                 }, // 0x073
    /* 116 */    { VC_PAGE_UP,               osfXK_PageUp             }, // 0x074 HP OSF KeySym in HPkeysym.h
    /* 117 */    { VC_PAGE_UP,               osfXK_Prior              }, // 0x075 HP OSF KeySym in HPkeysym.h
    /* 118 */    { VC_PAGE_DOWN,             XK_Page_Down             }, // 0x076
    /* 119 */    { VC_PAGE_DOWN,             XK_Next                  }, // 0x077
    /* 120 */    { VC_PAGE_DOWN,             osfXK_PageDown           }, // 0x078 HP OSF KeySym in HPkeysym.h
    /* 121 */    { VC_PAGE_DOWN,             osfXK_Next               }, // 0x079 HP OSF KeySym in HPkeysym.h
    // End Edit Key Zone


    // Begin Cursor Key Zone
    /* 122 */    { VC_UP,                    XK_Up                    }, // 0x07A
    /* 123 */    { VC_UP,                    osfXK_Up                 }, // 0x07B HP OSF KeySym in HPkeysym.h
    /* 124 */    { VC_LEFT,                  XK_Left                  }, // 0x07C
    /* 125 */    { VC_LEFT,                  osfXK_Left               }, // 0x07D HP OSF KeySym in HPkeysym.h
    /* 126 */    { VC_BEGIN,                 XK_Begin                 }, // 0x07E
    /* 127 */    { VC_RIGHT,                 XK_Right                 }, // 0x07F
    /* 128 */    { VC_RIGHT,                 osfXK_Right              }, // 0x080 HP OSF KeySym in HPkeysym.h
    /* 129 */    { VC_DOWN,                  XK_Down                  }, // 0x081
    /* 130 */    { VC_DOWN,                  osfXK_Down               }, // 0x082 HP OSF KeySym in HPkeysym.h
    // End Cursor Key Zone


    // Begin Numeric Zone
    /* 131 */    { VC_NUM_LOCK,              XK_Num_Lock              }, // 0x083
    /* 132 */    { VC_KP_CLEAR,              XK_Clear,                }, // 0x084
    /* 133 */    { VC_KP_CLEAR,              osfXK_Clear              }, // 0x085 HP OSF KeySym in HPkeysym.h

    /* 134 */    { VC_KP_DIVIDE,             XK_KP_Divide             }, // 0x086
    /* 135 */    { VC_KP_MULTIPLY,           XK_KP_Multiply           }, // 0x087
    /* 136 */    { VC_KP_SUBTRACT,           XK_KP_Subtract           }, // 0x088
    /* 137 */    { VC_KP_EQUALS,             XK_KP_Equal              }, // 0x089
    /* 138 */    { VC_KP_ADD,                XK_KP_Add                }, // 0x08A
    /* 139 */    { VC_KP_ENTER,              XK_KP_Enter              }, // 0x08B
    /* 140 */    { VC_KP_DECIMAL,            XK_KP_Decimal            }, // 0x08C
    /* 141 */    { VC_KP_SEPARATOR,          XK_KP_Separator          }, // 0x08D

    /* 142 */    { VC_KP_0,                  XK_KP_0                  }, // 0x08E
    /* 143 */    { VC_KP_1,                  XK_KP_1                  }, // 0x08F
    /* 144 */    { VC_KP_2,                  XK_KP_2                  }, // 0x090
    /* 145 */    { VC_KP_3,                  XK_KP_3                  }, // 0x091
    /* 146 */    { VC_KP_4,                  XK_KP_4                  }, // 0x092
    /* 147 */    { VC_KP_5,                  XK_KP_5                  }, // 0x093
    /* 148 */    { VC_KP_6,                  XK_KP_6                  }, // 0x094
    /* 149 */    { VC_KP_7,                  XK_KP_7                  }, // 0x095
    /* 150 */    { VC_KP_8,                  XK_KP_8                  }, // 0x096
    /* 151 */    { VC_KP_9,                  XK_KP_9                  }, // 0x097

    /* 152 */    { VC_KP_END,                XK_KP_End                }, // 0x098
    /* 153 */    { VC_KP_DOWN,               XK_KP_Down               }, // 0x099
    /* 154 */    { VC_KP_PAGE_DOWN,          XK_KP_Page_Down          }, // 0x09A
    /* 155 */    { VC_KP_PAGE_DOWN,          XK_KP_Next               }, // 0x09B
    /* 156 */    { VC_KP_LEFT,               XK_KP_Left               }, // 0x09C
    /* 157 */    { VC_KP_BEGIN,              XK_KP_Begin,             }, // 0x09D
    /* 158 */    { VC_KP_RIGHT,              XK_KP_Right              }, // 0x09E
    /* 159 */    { VC_KP_HOME,               XK_KP_Home               }, // 0x09F
    /* 160 */    { VC_KP_UP,                 XK_KP_Up                 }, // 0x0A0
    /* 161 */    { VC_KP_PAGE_UP,            XK_KP_Page_Up            }, // 0x0A1
    /* 162 */    { VC_KP_PAGE_UP,            XK_KP_Prior              }, // 0x0A2
    /* 163 */    { VC_KP_INSERT,             XK_KP_Insert             }, // 0x0A3
    /* 164 */    { VC_KP_DELETE,             XK_KP_Delete             }, // 0x0A4
    // End Numeric Zone


    // Begin Modifier and Control Keys
    /* 165 */    { VC_SHIFT_L,               XK_Shift_L               }, // 0x0A5
    /* 166 */    { VC_SHIFT_R,               XK_Shift_R               }, // 0x0A6
    /* 167 */    { VC_CONTROL_L,             XK_Control_L             }, // 0x0A7
    /* 168 */    { VC_CONTROL_R,             XK_Control_R             }, // 0x0A8
    /* 169 */    { VC_ALT_L,                 XK_Alt_L                 }, // 0x0A9
    /* 170 */    { VC_ALT_R,                 XK_Alt_R                 }, // 0x0AA
    /* 171 */    { VC_ALT_GRAPH,             XK_ISO_Level3_Shift      }, // 0x0AB
    /* 172 */    { VC_META_L,                XK_Meta_L                }, // 0x0AC
    /* 173 */    { VC_META_R,                XK_Meta_R                }, // 0x0AD
    /* 174 */    { VC_CONTEXT_MENU,          XK_Menu                  }, // 0x0AE
    // End Modifier and Control Keys


    // Begin Shortcut Keys
    /* 175 */    { VC_POWER,                 XF86XK_PowerOff          }, // 0x0AF
    /* 176 */    { VC_SLEEP,                 XF86XK_Sleep             }, // 0x0B0
    /* 177 */    { VC_WAKE,                  XF86XK_WakeUp            }, // 0x0B1

    /* 178 */    { VC_MEDIA_PLAY,            XF86XK_AudioPlay         }, // 0x0B2
    /* 179 */    { VC_MEDIA_STOP,            XF86XK_AudioStop         }, // 0x0B3
    /* 180 */    { VC_MEDIA_PREVIOUS,        XF86XK_AudioPrev         }, // 0x0B4
    /* 181 */    { VC_MEDIA_NEXT,            XF86XK_AudioNext         }, // 0x0B5
    /* 182 */    { VC_MEDIA_SELECT,          XF86XK_Select            }, // 0x0B6
    /* 183 */    { VC_MEDIA_EJECT,           XF86XK_Eject             }, // 0x0B7

    /* 184 */    { VC_VOLUME_MUTE,           XF86XK_AudioMute         }, // 0x0B8
    /* 185 */    { VC_VOLUME_MUTE,           SunXK_AudioMute          }, // 0x0B9
    /* 186 */    { VC_VOLUME_DOWN,           XF86XK_AudioLowerVolume  }, // 0x0BA
    /* 187 */    { VC_VOLUME_DOWN,           SunXK_AudioLowerVolume   }, // 0x0BB
    /* 188 */    { VC_VOLUME_UP,             XF86XK_AudioRaiseVolume  }, // 0x0BC
    /* 189 */    { VC_VOLUME_UP,             SunXK_AudioRaiseVolume   }, // 0x0BD

    /* 190 */    { VC_APP_BROWSER,           XF86XK_WWW               }, // 0x0BE
    /* 191 */    { VC_APP_CALCULATOR,        XF86XK_Calculator        }, // 0x0BF
    /* 192 */    { VC_APP_MAIL,              XF86XK_Mail              }, // 0x0C0
    /* 193 */    { VC_APP_MUSIC,             XF86XK_Music             }, // 0x0C1
    /* 194 */    { VC_APP_PICTURES,          XF86XK_Pictures          }, // 0x0C2

    /* 195 */    { VC_BROWSER_SEARCH,        XF86XK_Search            }, // 0x0C3
    /* 196 */    { VC_BROWSER_HOME,          XF86XK_HomePage          }, // 0x0C4
    /* 197 */    { VC_BROWSER_BACK,          XF86XK_Back              }, // 0x0C5
    /* 198 */    { VC_BROWSER_FORWARD,       XF86XK_Forward           }, // 0x0C6
    /* 199 */    { VC_BROWSER_STOP,          XF86XK_Stop              }, // 0x0C7
    /* 200 */    { VC_BROWSER_REFRESH,       XF86XK_Refresh           }, // 0x0C8
    /* 201 */    { VC_BROWSER_FAVORITES,     XF86XK_Favorites         }, // 0x0C9
    // End Shortcut Keys


    // Begin European Language Keys
    /* 202 */    { VC_CIRCUMFLEX,            XK_asciicircum           }, // 0x0CA

    /* 203 */    { VC_DEAD_GRAVE,            XK_dead_grave            }, // 0x0CB
    /* 204 */    { VC_DEAD_GRAVE,            SunXK_FA_Grave           }, // 0x0CC
    /* 205 */    { VC_DEAD_GRAVE,            DXK_grave_accent         }, // 0x0CD DEC private keysym in DECkeysym.h
    /* 206 */    { VC_DEAD_GRAVE,            hpXK_mute_grave          }, // 0x0CE HP OSF KeySym in HPkeysym.h

    /* 207 */    { VC_DEAD_ACUTE,            XK_dead_acute            }, // 0x0CF
    /* 208 */    { VC_DEAD_ACUTE,            SunXK_FA_Acute           }, // 0x0D0
    /* 209 */    { VC_DEAD_ACUTE,            DXK_acute_accent         }, // 0x0D1 DEC private keysym in DECkeysym.h
    /* 210 */    { VC_DEAD_ACUTE,            hpXK_mute_acute          }, // 0x0D2 HP OSF KeySym in HPkeysym.h

    /* 211 */    { VC_DEAD_CIRCUMFLEX,       XK_dead_circumflex       }, // 0x0D3
    /* 212 */    { VC_DEAD_CIRCUMFLEX,       SunXK_FA_Circum          }, // 0x0D4
    /* 213 */    { VC_DEAD_CIRCUMFLEX,       DXK_circumflex_accent    }, // 0x0D5 DEC private keysym in DECkeysym.h
    /* 214 */    { VC_DEAD_CIRCUMFLEX,       hpXK_mute_asciicircum    }, // 0x0D6 HP OSF KeySym in HPkeysym.h

    /* 215 */    { VC_DEAD_TILDE,            XK_dead_tilde            }, // 0x0D7
    /* 216 */    { VC_DEAD_TILDE,            SunXK_FA_Tilde           }, // 0x0D8
    /* 217 */    { VC_DEAD_TILDE,            DXK_tilde                }, // 0x0D9 DEC private keysym in DECkeysym.h
    /* 218 */    { VC_DEAD_TILDE,            hpXK_mute_asciitilde     }, // 0x0DA HP OSF KeySym in HPkeysym.h

    /* 219 */    { VC_DEAD_MACRON,           XK_dead_macron           }, // 0x0DB
    /* 220 */    { VC_DEAD_BREVE,            XK_dead_breve            }, // 0x0DC
    /* 221 */    { VC_DEAD_ABOVEDOT,         XK_dead_abovedot         }, // 0x0DD

    /* 222 */    { VC_DEAD_DIAERESIS,        XK_dead_diaeresis        }, // 0x0DE
    /* 223 */    { VC_DEAD_DIAERESIS,        SunXK_FA_Diaeresis       }, // 0x0DF
    /* 224 */    { VC_DEAD_DIAERESIS,        DXK_diaeresis            }, // 0x0E0 DEC private keysym in DECkeysym.h
    /* 225 */    { VC_DEAD_DIAERESIS,        hpXK_mute_diaeresis      }, // 0x0E1 HP OSF KeySym in HPkeysym.h

    /* 226 */    { VC_DEAD_ABOVERING,        XK_dead_abovering        }, // 0x0E2
    /* 227 */    { VC_DEAD_ABOVERING,        DXK_ring_accent          }, // 0x0E3 DEC private keysym in DECkeysym.h
    /* 228 */    { VC_DEAD_DOUBLEACUTE,      XK_dead_doubleacute      }, // 0x0E4
    /* 229 */    { VC_DEAD_CARON,            XK_dead_caron            }, // 0x0E5

    /* 230 */    { VC_DEAD_CEDILLA,          XK_dead_cedilla          }, // 0x0E6
    /* 231 */    { VC_DEAD_CEDILLA,          SunXK_FA_Cedilla         }, // 0x0E7
    /* 232 */    { VC_DEAD_CEDILLA,          DXK_cedilla_accent       }, // 0x0E8 DEC private keysym in DECkeysym.h

    /* 233 */    { VC_DEAD_OGONEK,           XK_dead_ogonek           }, // 0x0E9
    /* 234 */    { VC_DEAD_IOTA,             XK_dead_iota             }, // 0x0EA
    /* 235 */    { VC_DEAD_VOICED_SOUND,     XK_dead_voiced_sound     }, // 0x0EB
    /* 236 */    { VC_DEAD_SEMIVOICED_SOUND, XK_dead_semivoiced_sound }, // 0x0EC
    // End European Language Keys


    // Begin Asian Language Keys
    /* 237 */    { VC_KATAKANA,              XK_Katakana              }, // 0x0ED
    /* 238 */    { VC_KANA,                  XK_Kana_Shift            }, // 0x0EE
    /* 239 */    { VC_KANA_LOCK,             XK_Kana_Lock             }, // 0x0EF

    /* 240 */    { VC_KANJI,                 XK_Kanji                 }, // 0x0F0
    /* 241 */    { VC_HIRAGANA,              XK_Hiragana              }, // 0x0F1

    /* 242 */    { VC_ACCEPT,                XK_Execute               }, // 0x0F2 Type 5c Japanese keyboard: kakutei
    /* 243 */    { VC_CONVERT,               XK_Kanji                 }, // 0x0F3 Type 5c Japanese keyboard: henkan
    /* 244 */    { VC_COMPOSE,               XK_Multi_key             }, // 0x0F4
    /* 245 */    { VC_INPUT_METHOD_ON_OFF,   XK_Henkan_Mode           }, // 0x0F5 Type 5c Japanese keyboard: nihongo

    /* 246 */    { VC_ALL_CANDIDATES,        XK_Zen_Koho              }, // 0x0F6
    /* 247 */    { VC_ALPHANUMERIC,          XK_Eisu_Shift            }, // 0x0F7
    /* 248 */    { VC_ALPHANUMERIC,          XK_Eisu_toggle           }, // 0x0F8
    /* 249 */    { VC_CODE_INPUT,            XK_Kanji_Bangou          }, // 0x0F9
    /* 250 */    { VC_FULL_WIDTH,            XK_Zenkaku               }, // 0x0FA
    /* 251 */    { VC_HALF_WIDTH,            XK_Hankaku               }, // 0x0FB
    /* 252 */    { VC_NONCONVERT,            XK_Muhenkan              }, // 0x0FC
    /* 253 */    { VC_PREVIOUS_CANDIDATE,    XK_Mae_Koho              }, // 0x0FD
    /* 254 */    { VC_ROMAN_CHARACTERS,      XK_Romaji                }, // 0x0FE

    /* 255 */    { VC_UNDERSCORE,            XK_underscore            }, // 0x0FF
    // End Asian Language Keys


    // Begin Sun Keys
    /* 256 */    { VC_SUN_HELP,              XK_Help                  }, // 0x100
    /* 257 */    { VC_SUN_HELP,              osfXK_Help               }, // 0x101

    /* 258 */    { VC_SUN_STOP,              XK_L1                    }, // 0x102

    /* 259 */    { VC_SUN_PROPS,             SunXK_Props              }, // 0x103
    /* 260 */    { VC_SUN_PROPS,             XK_L3                    }, // 0x104

    /* 261 */    { VC_SUN_FRONT,             SunXK_Front              }, // 0x105
    /* 262 */    { VC_SUN_OPEN,              SunXK_Open               }, // 0x106

    /* 263 */    { VC_SUN_FIND,              XK_Find                  }, // 0x107
    /* 264 */    { VC_SUN_FIND,              XK_L9                    }, // 0x108

    /* 265 */    { VC_SUN_AGAIN,             XK_Redo                  }, // 0x109
    /* 266 */    { VC_SUN_AGAIN,             XK_L2                    }, // 0x10A

    /* 267 */    { VC_SUN_UNDO,              XK_Undo                  }, // 0x10B
    /* 268 */    { VC_SUN_UNDO,              XK_L4                    }, // 0x10C
    /* 269 */    { VC_SUN_UNDO,              osfXK_Undo               }, // 0x10D

    /* 270 */    { VC_SUN_COPY,              XK_L6                    }, // 0x10E
    /* 271 */    { VC_SUN_COPY,              apXK_Copy                }, // 0x10F
    /* 272 */    { VC_SUN_COPY,              SunXK_Copy               }, // 0x110
    /* 273 */    { VC_SUN_COPY,              osfXK_Copy               }, // 0x111

    /* 274 */    { VC_SUN_PASTE,             XK_L8                    }, // 0x112
    /* 275 */    { VC_SUN_PASTE,             SunXK_Paste              }, // 0x113
    /* 276 */    { VC_SUN_PASTE,             apXK_Paste               }, // 0x114
    /* 277 */    { VC_SUN_PASTE,             osfXK_Paste              }, // 0x115

    /* 278 */    { VC_SUN_CUT,               XK_L10                   }, // 0x116
    /* 279 */    { VC_SUN_CUT,               SunXK_Cut                }, // 0x117
    /* 280 */    { VC_SUN_CUT,               apXK_Cut                 }, // 0x118
    /* 281 */    { VC_SUN_CUT,               osfXK_Cut                }, // 0x119
    // End Sun Keys

    /* 282 */    { VC_UNDEFINED,             NoSymbol                 }  // 0x11A
};


uint16_t keysym_to_uiocode(KeySym keysym) {
    uint16_t uiocode = VC_UNDEFINED;

    for (unsigned int i = 0; i < sizeof(uiocode_keysym_table) / sizeof(uiocode_keysym_table[0]); i++) {
        if (keysym == uiocode_keysym_table[i][1]) {
            uiocode = uiocode_keysym_table[i][0];
            break;
        }
    }

    if ((get_modifiers() & MASK_NUM_LOCK) == 0) {
        switch (uiocode) {
            case VC_KP_SEPARATOR:
            case VC_KP_1:
            case VC_KP_2:
            case VC_KP_3:
            case VC_KP_4:
            case VC_KP_5:
            case VC_KP_6:
            case VC_KP_7:
            case VC_KP_8:
            case VC_KP_0:
            case VC_KP_9:
                uiocode |= 0xEE00;
                break;
        }
    }

    return uiocode;
}

KeyCode uiocode_to_keycode(uint16_t uiocode) {
    KeyCode keycode = 0x0000;
    KeySym keysym = NoSymbol;

    for (unsigned int i = 0; i < sizeof(uiocode_keysym_table) / sizeof(uiocode_keysym_table[0]); i++) {
        if (uiocode == uiocode_keysym_table[i][0]) {
            keysym = uiocode_keysym_table[i][1];

            // It doesn't matter if this succeeds, we won't find another match in the table so always exist.
            keycode = XKeysymToKeycode(helper_disp, keysym);
            break;
        }
    }

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
}

/* Based on mappings from _XWireToEvent in Xlibinit.c */
void wire_data_to_event(XRecordInterceptData *recorded_data, XEvent *x_event) {
    if (recorded_data->category == XRecordFromServer) {
        XRecordDatum *data = (XRecordDatum *) recorded_data->data;
        switch (recorded_data->category) {
            //case XRecordFromClient: // TODO Should we be listening for Client Events?
            case XRecordFromServer:
                x_event->type = data->event.u.u.type;
                ((XAnyEvent *) x_event)->display = helper_disp;
                ((XAnyEvent *) x_event)->send_event = (bool) (data->event.u.u.type & 0x80);

                switch (data->type) {
                    case KeyPress:
                    case KeyRelease:
                        ((XKeyEvent *) x_event)->root           = data->event.u.keyButtonPointer.root;
                        ((XKeyEvent *) x_event)->window         = data->event.u.keyButtonPointer.event;
                        ((XKeyEvent *) x_event)->subwindow      = data->event.u.keyButtonPointer.child;
                        ((XKeyEvent *) x_event)->time           = data->event.u.keyButtonPointer.time;
                        ((XKeyEvent *) x_event)->x              = cvtINT16toInt(data->event.u.keyButtonPointer.eventX);
                        ((XKeyEvent *) x_event)->y              = cvtINT16toInt(data->event.u.keyButtonPointer.eventY);
                        ((XKeyEvent *) x_event)->x_root         = cvtINT16toInt(data->event.u.keyButtonPointer.rootX);
                        ((XKeyEvent *) x_event)->y_root         = cvtINT16toInt(data->event.u.keyButtonPointer.rootY);
                        ((XKeyEvent *) x_event)->state          = data->event.u.keyButtonPointer.state;
                        ((XKeyEvent *) x_event)->same_screen    = data->event.u.keyButtonPointer.sameScreen;
                        ((XKeyEvent *) x_event)->keycode        = data->event.u.u.detail;
                        break;

                    case ButtonPress:
                    case ButtonRelease:
                        ((XButtonEvent *) x_event)->root        = data->event.u.keyButtonPointer.root;
                        ((XButtonEvent *) x_event)->window      = data->event.u.keyButtonPointer.event;
                        ((XButtonEvent *) x_event)->subwindow   = data->event.u.keyButtonPointer.child;
                        ((XButtonEvent *) x_event)->time        = data->event.u.keyButtonPointer.time;
                        ((XButtonEvent *) x_event)->x           = cvtINT16toInt(data->event.u.keyButtonPointer.eventX);
                        ((XButtonEvent *) x_event)->y           = cvtINT16toInt(data->event.u.keyButtonPointer.eventY);
                        ((XButtonEvent *) x_event)->x_root      = cvtINT16toInt(data->event.u.keyButtonPointer.rootX);
                        ((XButtonEvent *) x_event)->y_root      = cvtINT16toInt(data->event.u.keyButtonPointer.rootY);
                        ((XButtonEvent *) x_event)->state       = data->event.u.keyButtonPointer.state;
                        ((XButtonEvent *) x_event)->same_screen = data->event.u.keyButtonPointer.sameScreen;
                        ((XButtonEvent *) x_event)->button      = data->event.u.u.detail;
                        break;

                    case MotionNotify:
                        ((XMotionEvent *) x_event)->root        = data->event.u.keyButtonPointer.root;
                        ((XMotionEvent *) x_event)->window      = data->event.u.keyButtonPointer.event;
                        ((XMotionEvent *) x_event)->subwindow   = data->event.u.keyButtonPointer.child;
                        ((XMotionEvent *) x_event)->time        = data->event.u.keyButtonPointer.time;
                        ((XMotionEvent *) x_event)->x           = cvtINT16toInt(data->event.u.keyButtonPointer.eventX);
                        ((XMotionEvent *) x_event)->y           = cvtINT16toInt(data->event.u.keyButtonPointer.eventY);
                        ((XMotionEvent *) x_event)->x_root      = cvtINT16toInt(data->event.u.keyButtonPointer.rootX);
                        ((XMotionEvent *) x_event)->y_root      = cvtINT16toInt(data->event.u.keyButtonPointer.rootY);
                        ((XMotionEvent *) x_event)->state       = data->event.u.keyButtonPointer.state;
                        ((XMotionEvent *) x_event)->same_screen = data->event.u.keyButtonPointer.sameScreen;
                        ((XMotionEvent *) x_event)->is_hint     = data->event.u.u.detail;
                        break;
                }
                break;
        }
    }
}

uint8_t button_map_lookup(uint8_t button) {
    unsigned int map_button = button;

    if (helper_disp != NULL) {
        if (mouse_button_table != NULL) {
            int map_size = XGetPointerMapping(helper_disp, mouse_button_table, BUTTON_TABLE_MAX);
            if (map_button > 0 && map_button <= map_size) {
                map_button = mouse_button_table[map_button -1];
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

bool enable_key_repeat() {
    // Attempt to setup detectable autorepeat.
    // NOTE: is_auto_repeat is NOT stdbool!
    Bool is_auto_repeat = False;

    // Enable detectable auto-repeat.
    XkbSetDetectableAutoRepeat(helper_disp, True, &is_auto_repeat);

    return is_auto_repeat;
}

size_t event_to_unicode(XKeyEvent *x_event, wchar_t *surrogate, size_t length, KeySym *keysym) {
    XIC xic = NULL;
    XIM xim = NULL;

    // KeyPress events can use Xutf8LookupString but KeyRelease events cannot.
    if (x_event->type == KeyPress) {
        XSetLocaleModifiers("");
        xim = XOpenIM(helper_disp, NULL, NULL, NULL);
        if (xim == NULL) {
            // fallback to internal input method
            XSetLocaleModifiers("@im=none");
            xim = XOpenIM(helper_disp, NULL, NULL, NULL);
        }

        if (xim != NULL) {
            Window root_default = XDefaultRootWindow(helper_disp);
            xic = XCreateIC(xim,
                XNInputStyle,   XIMPreeditNothing | XIMStatusNothing,
                XNClientWindow, root_default,
                XNFocusWindow,  root_default,
                NULL);

            if (xic == NULL) {
                logger(LOG_LEVEL_WARN, "%s [%u]: XCreateIC() failed!\n",
                        __FUNCTION__, __LINE__);
            }
        } else {
            logger(LOG_LEVEL_WARN, "%s [%u]: XOpenIM() failed!\n",
                    __FUNCTION__, __LINE__);
        }
    }

    size_t count = 0;
    char buffer[5] = {};
    
    if (xic != NULL) {
        count = Xutf8LookupString(xic, x_event, buffer, sizeof(buffer), keysym, NULL);
        XDestroyIC(xic);
    } else {
        count = XLookupString(x_event, buffer, sizeof(buffer), keysym, NULL);
    }

    if (xim != NULL) {
        XCloseIM(xim);
    }

    // If we produced a string and we have a buffer, convert to 16-bit surrogate pairs.
    if (count > 0) {
        if (length == 0 || surrogate == NULL) {
            count = 0;
        } else {
            // TODO Can we just replace all this with `count = mbstowcs(surrogate, buffer, count);`?
            // See https://en.wikipedia.org/wiki/UTF-8#Examples
            const uint8_t utf8_bitmask_table[] = {
                0x3F, // 00111111, non-first (if > 1 byte)
                0x7F, // 01111111, first (if 1 byte)
                0x1F, // 00011111, first (if 2 bytes)
                0x0F, // 00001111, first (if 3 bytes)
                0x07  // 00000111, first (if 4 bytes)
            };

            uint32_t codepoint = utf8_bitmask_table[count] & buffer[0];
            for (unsigned int i = 1; i < count; i++) {
                codepoint = (codepoint << 6) | (utf8_bitmask_table[0] & buffer[i]);
            }

            if (codepoint <= 0xFFFF) {
                count = 1;
                surrogate[0] = codepoint;
            } else if (length > 1) {
                // if codepoint > 0xFFFF, split into lead (high) / trail (low) surrogate ranges
                // See https://unicode.org/faq/utf_bom.html#utf16-4
                const uint32_t lead_offset = 0xD800 - (0x10000 >> 10);

                count = 2;
                surrogate[0] = lead_offset + (codepoint >> 10); // lead,  first  [0]
                surrogate[1] = 0xDC00 + (codepoint & 0x3FF);    // trail, second [1]
            } else {
                count = 0;
                logger(LOG_LEVEL_WARN, "%s [%u]: Surrogate buffer overflow detected!\n",
                        __FUNCTION__, __LINE__);
            }
        }

    }

    return count;
}

int load_input_helper() {
    // Setup memory for mouse button mapping.
    mouse_button_table = malloc(sizeof(unsigned char) * BUTTON_TABLE_MAX);
    if (mouse_button_table == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to allocate memory for mouse button map!\n",
                __FUNCTION__, __LINE__);

        return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }

    initialize_modifiers();

    return UIOHOOK_SUCCESS;
}

void unload_input_helper() {
    if (mouse_button_table != NULL) {
        free(mouse_button_table);
        mouse_button_table = NULL;
    }
}
