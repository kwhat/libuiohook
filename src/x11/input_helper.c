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

#ifdef USE_EPOCH_TIME
#include <sys/time.h>
#endif

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

static const uint32_t keysym_vcode_table[][2] = {
    { VC_ESCAPE,                XK_Escape,               },
    { VC_ESCAPE,                osfXK_Escape             }, // HP OSF KeySym in HPkeysym.h

    // Begin Function Keys
    { VC_F1,                    XK_F1                    },
    { VC_F2,                    XK_F2                    },
    { VC_F3,                    XK_F3                    },
    { VC_F4,                    XK_F4                    },
    { VC_F5,                    XK_F5                    },
    { VC_F6,                    XK_F6                    },
    { VC_F7,                    XK_F7                    },
    { VC_F8,                    XK_F8                    },
    { VC_F9,                    XK_F9                    },
    { VC_F10,                   XK_F10                   },
    { VC_F11,                   XK_F11                   },
    { VC_F11,                   SunXK_F36                }, // Labeled F11 in Sunkeysym.h
    { VC_F12,                   XK_F12                   },
    { VC_F12,                   SunXK_F37                }, // Labeled F12 in Sunkeysym.h

    { VC_F13,                   XK_F13                   },
    { VC_F14,                   XK_F14                   },
    { VC_F15,                   XK_F15                   },
    { VC_F16,                   XK_F16                   },
    { VC_F17,                   XK_F17                   },
    { VC_F18,                   XK_F18                   },
    { VC_F19,                   XK_F19                   },
    { VC_F20,                   XK_F20                   },
    { VC_F21,                   XK_F21                   },
    { VC_F22,                   XK_F22                   },
    { VC_F23,                   XK_F23                   },
    { VC_F24,                   XK_F24                   },
    // End Function Keys


    // Begin Alphanumeric Zone
    { VC_BACK_QUOTE,            XK_grave                 },

    { VC_0,                     XK_0                     },
    { VC_1,                     XK_1                     },
    { VC_2,                     XK_2                     },
    { VC_3,                     XK_3                     },
    { VC_4,                     XK_4                     },
    { VC_5,                     XK_5                     },
    { VC_6,                     XK_6                     },
    { VC_7,                     XK_7                     },
    { VC_8,                     XK_8                     },
    { VC_9,                     XK_9                     },

    { VC_MINUS,                 XK_minus                 },
    { VC_PLUS,                  XK_plus                  },
    { VC_EQUALS,                XK_equal                 },
    { VC_ASTERISK,              XK_asterisk              },

    { VC_AT,                    XK_at                    },
    { VC_AMPERSAND,             XK_ampersand             },
    { VC_DOLLAR,                XK_dollar                },
    { VC_EXCLAMATION_MARK,      XK_exclam                },
    { VC_EXCLAMATION_DOWN,      XK_exclamdown            },

    { VC_BACKSPACE,             XK_BackSpace             },
    { VC_BACKSPACE,             osfXK_BackSpace          }, // HP OSF KeySym in HPkeysym.h

    { VC_TAB,                   XK_Tab                   },
    { VC_TAB,                   XK_ISO_Left_Tab          },
    { VC_CAPS_LOCK,             XK_Caps_Lock             },
    { VC_CAPS_LOCK,             XK_Shift_Lock            },

    { VC_A,                     XK_a                     },
    { VC_B,                     XK_b                     },
    { VC_C,                     XK_c                     },
    { VC_D,                     XK_d                     },
    { VC_E,                     XK_e                     },
    { VC_F,                     XK_f                     },
    { VC_G,                     XK_g                     },
    { VC_H,                     XK_h                     },
    { VC_I,                     XK_i                     },
    { VC_J,                     XK_j                     },
    { VC_K,                     XK_k                     },
    { VC_L,                     XK_l                     },
    { VC_M,                     XK_m                     },
    { VC_N,                     XK_n                     },
    { VC_O,                     XK_o                     },
    { VC_P,                     XK_p                     },
    { VC_Q,                     XK_q                     },
    { VC_R,                     XK_r                     },
    { VC_S,                     XK_s                     },
    { VC_T,                     XK_t                     },
    { VC_U,                     XK_u                     },
    { VC_V,                     XK_v                     },
    { VC_W,                     XK_w                     },
    { VC_X,                     XK_x                     },
    { VC_Y,                     XK_y                     },
    { VC_Z,                     XK_z                     },

    { VC_OPEN_BRACKET,          XK_bracketleft           },
    { VC_CLOSE_BRACKET,         XK_bracketright          },
    { VC_BACK_SLASH,            XK_backslash             },

    { VC_COLON,                 XK_colon                 },
    { VC_SEMICOLON,             XK_semicolon             },
    { VC_QUOTE,                 XK_apostrophe            },
    { VC_QUOTEDBL,              XK_quotedbl              },
    { VC_ENTER,                 XK_Return,               },
    { VC_ENTER,                 XK_Linefeed,             },

    { VC_LESS,                  XK_less                  },
    { VC_GREATER,               XK_greater               },
    { VC_COMMA,                 XK_comma                 },
    { VC_PERIOD,                XK_period                },
    { VC_SLASH,                 XK_slash                 },
    { VC_NUMBER_SIGN,           XK_numbersign            },

    { VC_OPEN_BRACE,            XK_braceleft             },
    { VC_CLOSE_BRACE,           XK_braceright            },

    { VC_OPEN_PARENTHESIS,      XK_parenleft             },
    { VC_CLOSE_PARENTHESIS,     XK_parenright            },

    { VC_SPACE,                 XK_space                 },
    // End Alphanumeric Zone


    // Begin Edit Key Zone
    { VC_PRINT_SCREEN,          XK_Print                 },
    { VC_PRINT_SCREEN,          SunXK_Print_Screen       }, // Same as XK_Print in Sunkeysym.h
    { VC_PRINT_SCREEN,          SunXK_Sys_Req            }, // SysReq should be the same as Print Screen
    { VC_SCROLL_LOCK,           XK_Scroll_Lock,          },
    { VC_PAUSE,                 XK_Pause                 },
    { VC_CANCEL,                XK_Cancel                },
    { VC_CANCEL,                osfXK_Cancel             }, // HP OSF KeySym in HPkeysym.h
    { VC_INSERT,                XK_Insert                },
    { VC_INSERT,                osfXK_Insert             }, // HP OSF KeySym in HPkeysym.h
    { VC_DELETE,                XK_Delete                },
    { VC_DELETE,                osfXK_Delete             }, // HP OSF KeySym in HPkeysym.h
    { VC_HOME,                  XK_Home                  },
    { VC_END,                   XK_End                   },
    { VC_END,                   osfXK_EndLine            }, // HP OSF KeySym in HPkeysym.h
    { VC_PAGE_UP,               XK_Page_Up               },
    { VC_PAGE_UP,               XK_Prior                 },
    { VC_PAGE_UP,               osfXK_PageUp             }, // HP OSF KeySym in HPkeysym.h
    { VC_PAGE_UP,               osfXK_Prior              }, // HP OSF KeySym in HPkeysym.h
    { VC_PAGE_DOWN,             XK_Page_Down             },
    { VC_PAGE_DOWN,             XK_Next                  },
    { VC_PAGE_DOWN,             osfXK_PageDown           }, // HP OSF KeySym in HPkeysym.h
    { VC_PAGE_DOWN,             osfXK_Next               }, // HP OSF KeySym in HPkeysym.h
    // End Edit Key Zone


    // Begin Cursor Key Zone
    { VC_UP,                    XK_Up                    },
    { VC_UP,                    osfXK_Up                 }, // HP OSF KeySym in HPkeysym.h
    { VC_LEFT,                  XK_Left                  },
    { VC_LEFT,                  osfXK_Left               }, // HP OSF KeySym in HPkeysym.h
    { VC_BEGIN,                 XK_Begin                 },
    { VC_RIGHT,                 XK_Right                 },
    { VC_RIGHT,                 osfXK_Right              }, // HP OSF KeySym in HPkeysym.h
    { VC_DOWN,                  XK_Down                  },
    { VC_DOWN,                  osfXK_Down               }, // HP OSF KeySym in HPkeysym.h
    // End Cursor Key Zone


    // Begin Numeric Zone
    { VC_NUM_LOCK,              XK_Num_Lock              },
    { VC_KP_CLEAR,              XK_Clear,                },
    { VC_KP_CLEAR,              osfXK_Clear              }, // HP OSF KeySym in HPkeysym.h

    { VC_KP_DIVIDE,             XK_KP_Divide             },
    { VC_KP_MULTIPLY,           XK_KP_Multiply           },
    { VC_KP_SUBTRACT,           XK_KP_Subtract           },
    { VC_KP_EQUALS,             XK_KP_Equal              },
    { VC_KP_ADD,                XK_KP_Add                },
    { VC_KP_ENTER,              XK_KP_Enter              },
    { VC_KP_DECIMAL,            XK_KP_Decimal            },
    { VC_KP_SEPARATOR,          XK_KP_Separator          },

    /* TODO Implement
    { VC_KP_SPACE,              XK_KP_Space  },
    { VC_KP_TAB,                XK_KP_Tab      },
    */

    { VC_KP_0,                  XK_KP_0                  },
    { VC_KP_1,                  XK_KP_1                  },
    { VC_KP_2,                  XK_KP_2                  },
    { VC_KP_3,                  XK_KP_3                  },
    { VC_KP_4,                  XK_KP_4                  },
    { VC_KP_5,                  XK_KP_5                  },
    { VC_KP_6,                  XK_KP_6                  },
    { VC_KP_7,                  XK_KP_7                  },
    { VC_KP_8,                  XK_KP_8                  },
    { VC_KP_9,                  XK_KP_9                  },

    { VC_KP_END,                XK_KP_End                },
    { VC_KP_DOWN,               XK_KP_Down               },
    { VC_KP_PAGE_DOWN,          XK_KP_Page_Down          },
    { VC_KP_PAGE_DOWN,          XK_KP_Next               },
    { VC_KP_LEFT,               XK_KP_Left               },
    { VC_KP_BEGIN,              XK_KP_Begin,             },
    { VC_KP_RIGHT,              XK_KP_Right              },
    { VC_KP_HOME,               XK_KP_Home               },
    { VC_KP_UP,                 XK_KP_Up                 },
    { VC_KP_PAGE_UP,            XK_KP_Page_Up            },
    { VC_KP_PAGE_UP,            XK_KP_Prior              },
    { VC_KP_INSERT,             XK_KP_Insert             },
    { VC_KP_DELETE,             XK_KP_Delete             },
    // End Numeric Zone


    // Begin Modifier and Control Keys
    { VC_SHIFT_L,               XK_Shift_L               },
    { VC_SHIFT_R,               XK_Shift_R               },
    { VC_CONTROL_L,             XK_Control_L             },
    { VC_CONTROL_R,             XK_Control_R             },
    { VC_ALT_L,                 XK_Alt_L                 },
    { VC_ALT_R,                 XK_Alt_R                 },
    { VC_ALT_GRAPH,             XK_ISO_Level3_Shift      },
    { VC_META_L,                XK_Meta_L                },
    { VC_META_R,                XK_Meta_R                },
    { VC_CONTEXT_MENU,          XK_Menu                  },
    // End Modifier and Control Keys


    // Begin Shortcut Keys
    { VC_POWER,                 XF86XK_PowerOff          },
    { VC_SLEEP,                 XF86XK_Sleep             },
    { VC_WAKE,                  XF86XK_WakeUp            },

    { VC_MEDIA_PLAY,            XF86XK_AudioPlay         },
    { VC_MEDIA_STOP,            XF86XK_AudioStop         },
    { VC_MEDIA_PREVIOUS,        XF86XK_AudioPrev         },
    { VC_MEDIA_NEXT,            XF86XK_AudioNext         },
    { VC_MEDIA_SELECT,          XF86XK_Select            },
    { VC_MEDIA_EJECT,           XF86XK_Eject             },

    { VC_VOLUME_MUTE,           XF86XK_AudioMute         },
    { VC_VOLUME_MUTE,           SunXK_AudioMute          },
    { VC_VOLUME_DOWN,           XF86XK_AudioLowerVolume  },
    { VC_VOLUME_DOWN,           SunXK_AudioLowerVolume   },
    { VC_VOLUME_UP,             XF86XK_AudioRaiseVolume  },
    { VC_VOLUME_UP,             SunXK_AudioRaiseVolume   },

    { VC_APP_BROWSER,           XF86XK_WWW               },
    { VC_APP_CALCULATOR,        XF86XK_Calculator        },
    { VC_APP_MAIL,              XF86XK_Mail              },
    { VC_APP_MUSIC,             XF86XK_Music             },
    { VC_APP_PICTURES,          XF86XK_Pictures          },

    { VC_BROWSER_SEARCH,        XF86XK_Search            },
    { VC_BROWSER_HOME,          XF86XK_HomePage          },
    { VC_BROWSER_BACK,          XF86XK_Back              },
    { VC_BROWSER_FORWARD,       XF86XK_Forward           },
    { VC_BROWSER_STOP,          XF86XK_Stop              },
    { VC_BROWSER_REFRESH,       XF86XK_Refresh           },
    { VC_BROWSER_FAVORITES,     XF86XK_Favorites         },
    // End Shortcut Keys


    // Begin European Language Keys
    { VC_CIRCUMFLEX,            XK_asciicircum           },

    { VC_DEAD_GRAVE,            XK_dead_grave            },
    { VC_DEAD_GRAVE,            SunXK_FA_Grave           },
    { VC_DEAD_GRAVE,            DXK_grave_accent         }, // DEC private keysym in DECkeysym.h
    { VC_DEAD_GRAVE,            hpXK_mute_grave          }, // HP OSF KeySym in HPkeysym.h

    { VC_DEAD_ACUTE,            XK_dead_acute            },
    { VC_DEAD_ACUTE,            SunXK_FA_Acute           },
    { VC_DEAD_ACUTE,            DXK_acute_accent         }, // DEC private keysym in DECkeysym.h
    { VC_DEAD_ACUTE,            hpXK_mute_acute          }, // HP OSF KeySym in HPkeysym.h

    { VC_DEAD_CIRCUMFLEX,       XK_dead_circumflex       },
    { VC_DEAD_CIRCUMFLEX,       SunXK_FA_Circum          },
    { VC_DEAD_CIRCUMFLEX,       DXK_circumflex_accent    }, // DEC private keysym in DECkeysym.h
    { VC_DEAD_CIRCUMFLEX,       hpXK_mute_asciicircum    }, // HP OSF KeySym in HPkeysym.h

    { VC_DEAD_TILDE,            XK_dead_tilde            },
    { VC_DEAD_TILDE,            SunXK_FA_Tilde           },
    { VC_DEAD_TILDE,            DXK_tilde                }, // DEC private keysym in DECkeysym.h
    { VC_DEAD_TILDE,            hpXK_mute_asciitilde     }, // HP OSF KeySym in HPkeysym.h

    { VC_DEAD_MACRON,           XK_dead_macron           },
    { VC_DEAD_BREVE,            XK_dead_breve            },
    { VC_DEAD_ABOVEDOT,         XK_dead_abovedot         },

    { VC_DEAD_DIAERESIS,        XK_dead_diaeresis        },
    { VC_DEAD_DIAERESIS,        SunXK_FA_Diaeresis       },
    { VC_DEAD_DIAERESIS,        DXK_diaeresis            }, // DEC private keysym in DECkeysym.h
    { VC_DEAD_DIAERESIS,        hpXK_mute_diaeresis      }, // HP OSF KeySym in HPkeysym.h

    { VC_DEAD_ABOVERING,        XK_dead_abovering        },
    { VC_DEAD_ABOVERING,        DXK_ring_accent          }, // DEC private keysym in DECkeysym.h
    { VC_DEAD_DOUBLEACUTE,      XK_dead_doubleacute      },
    { VC_DEAD_CARON,            XK_dead_caron            },

    { VC_DEAD_CEDILLA,          XK_dead_cedilla          },
    { VC_DEAD_CEDILLA,          SunXK_FA_Cedilla         },
    { VC_DEAD_CEDILLA,          DXK_cedilla_accent       }, // DEC private keysym in DECkeysym.h

    { VC_DEAD_OGONEK,           XK_dead_ogonek           },
    { VC_DEAD_IOTA,             XK_dead_iota             },
    { VC_DEAD_VOICED_SOUND,     XK_dead_voiced_sound     },
    { VC_DEAD_SEMIVOICED_SOUND, XK_dead_semivoiced_sound },
    // End European Language Keys


    // Begin Asian Language Keys
    { VC_KATAKANA,              XK_Katakana              },
    { VC_KANA,                  XK_Kana_Shift            },
    { VC_KANA_LOCK,             XK_Kana_Lock             },

    { VC_KANJI,                 XK_Kanji                 },
    { VC_HIRAGANA,              XK_Hiragana              },

    { VC_ACCEPT,                XK_Execute               }, // Type 5c Japanese keyboard: kakutei
    { VC_CONVERT,               XK_Kanji                 }, // Type 5c Japanese keyboard: henkan
    { VC_COMPOSE,               XK_Multi_key             },
    { VC_INPUT_METHOD_ON_OFF,   XK_Henkan_Mode           }, // Type 5c Japanese keyboard: nihongo

    { VC_ALL_CANDIDATES,        XK_Zen_Koho              },
    { VC_ALPHANUMERIC,          XK_Eisu_Shift            },
    { VC_ALPHANUMERIC,          XK_Eisu_toggle           },
    { VC_CODE_INPUT,            XK_Kanji_Bangou          },
    { VC_FULL_WIDTH,            XK_Zenkaku               },
    { VC_HALF_WIDTH,            XK_Hankaku               },
    { VC_NONCONVERT,            XK_Muhenkan              },
    { VC_PREVIOUS_CANDIDATE,    XK_Mae_Koho              },
    { VC_ROMAN_CHARACTERS,      XK_Romaji                },

    { VC_UNDERSCORE,            XK_underscore            },
    // End Asian Language Keys


    // Begin Sun Keys
    { VC_SUN_HELP,              XK_Help                  },
    { VC_SUN_HELP,              osfXK_Help               },

    { VC_SUN_STOP,              XK_Cancel                }, // FIXME Already used...
    { VC_SUN_STOP,              SunXK_Stop               }, // Same as XK_Cancel in Sunkeysym.h
    { VC_SUN_STOP,              XK_L1                    },

    { VC_SUN_PROPS,             SunXK_Props              },
    { VC_SUN_PROPS,             XK_L3                    },

    { VC_SUN_FRONT,             SunXK_Front              },
    { VC_SUN_OPEN,              SunXK_Open               },

    { VC_SUN_FIND,              XK_Find                  },
    { VC_SUN_FIND,              XK_L9                    },
    { VC_SUN_FIND,              SunXK_Find               }, // Same as XK_Find in Sunkeysym.h

    { VC_SUN_AGAIN,             XK_Redo                  },
    { VC_SUN_AGAIN,             XK_L2                    },
    { VC_SUN_AGAIN,             SunXK_Again              }, // Same as XK_Redo in Sunkeysym.h

    { VC_SUN_UNDO,              XK_Undo                  },
    { VC_SUN_UNDO,              XK_L4                    },
    { VC_SUN_UNDO,              SunXK_Undo               }, // Same as XK_Undo in Sunkeysym.h
    { VC_SUN_UNDO,              osfXK_Undo               },

    { VC_SUN_COPY,              XK_L6                    },
    { VC_SUN_COPY,              apXK_Copy                },
    { VC_SUN_COPY,              SunXK_Copy               },
    { VC_SUN_COPY,              osfXK_Copy               },

    { VC_SUN_PASTE,             XK_L8                    },
    { VC_SUN_PASTE,             SunXK_Paste              },
    { VC_SUN_PASTE,             apXK_Paste               },
    { VC_SUN_PASTE,             osfXK_Paste              },

    { VC_SUN_CUT,               XK_L10                   },
    { VC_SUN_CUT,               SunXK_Cut                },
    { VC_SUN_CUT,               apXK_Cut                 },
    { VC_SUN_CUT,               osfXK_Cut                },
    // End Sun Keys

    { VC_UNDEFINED,             NoSymbol                 }
};


uint16_t keysym_to_vcode(KeySym keysym) {
    uint16_t uiocode = VC_UNDEFINED;

    for (unsigned int i = 0; i < sizeof(keysym_vcode_table) / sizeof(keysym_vcode_table[0]); i++) {
        if (keysym == keysym_vcode_table[i][1]) {
            uiocode = keysym_vcode_table[i][0];
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

KeyCode vcode_to_keycode(uint16_t vcode) {
    KeyCode keycode = 0x0000;
    KeyCode keysym = NoSymbol;

    for (unsigned int i = 0; i < sizeof(keysym_vcode_table) / sizeof(keysym_vcode_table[0]); i++) {
        if (vcode == keysym_vcode_table[i][0]) {
            keycode = keysym_vcode_table[i][1];
            if (keysym = XKeysymToKeycode(helper_disp, keycode)) {
                break;
            }
        }
    }

    return keysym;
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

#ifdef USE_EPOCH_TIME
/* Get the current timestamp in unix epoch time. */
static uint64_t get_unix_timestamp() {
    struct timeval system_time;

    // Get the local system time in UTC.
    gettimeofday(&system_time, NULL);

    // Convert the local system time to a Unix epoch in MS.
    uint64_t timestamp = (system_time.tv_sec * 1000) + (system_time.tv_usec / 1000);

    return timestamp;
}
#endif

/* Based on mappings from _XWireToEvent in Xlibinit.c */
void wire_data_to_event(XRecordInterceptData *recorded_data, XEvent *x_event) {
    #ifdef USE_EPOCH_TIME
    uint64_t timestamp = get_unix_timestamp();
    #else
    uint64_t timestamp = (uint64_t) recorded_data->server_time;
    #endif

	((XAnyEvent *) x_event)->serial = timestamp;

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

size_t x_key_event_lookup(XKeyEvent *x_event, wchar_t *surrogate, size_t length, KeySym *keysym) {
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
    mouse_button_table = malloc(sizeof(unsigned char) * BUTTON_TABLE_MAX);
    if (mouse_button_table == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to allocate memory for mouse button map!\n",
                __FUNCTION__, __LINE__);

        //return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }
}

void unload_input_helper() {
    if (mouse_button_table != NULL) {
        free(mouse_button_table);
        mouse_button_table = NULL;
    }
}
