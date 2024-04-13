/* libUIOHook: Cross-platform keyboard and mouse hooking from userland.
 * Copyright (C) 2006-2024 Alexander Barker.  All Rights Reserved.
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

#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef USE_EPOCH_TIME
#include <sys/time.h>
#endif

#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <X11/Xlib-xcb.h>

#ifdef HAVE_XKB_H
#include <X11/extensions/XKB.h>
#else
#define XkbMinLegalKeyCode 8
/* TODO Should we use XDisplayKeycodes
int min_keycodes, max_keycodes;
XDisplayKeycodes(display, &min_keycodes, &max_keycodes);
*/
/* TODO What about using xkb_keycode_t xkb_keymap_min_keycode(struct xkb_keymap *keymap)?
*/
#endif


#include <X11/Xlib.h>
#include <X11/Xutil.h>

// TODO It's unclear if these XKB_KEY's should still be used.
#ifndef XKB_KEY_osfPrior
#define XKB_KEY_osfPrior 0x1004FF55
#endif

#ifndef XKB_KEY_osfNext
#define XKB_KEY_osfNext 0x1004FF56
#endif

#ifndef XKB_KEY_apCopy
#define XKB_KEY_apCopy 0x1000FF02
#endif

#ifndef XKB_KEY_apCut
#define XKB_KEY_apCut 0x1000FF03
#endif

#ifndef XKB_KEY_apPaste
#define XKB_KEY_apPaste 0x1000FF04
#endif


#include <X11/XKBlib.h>
static XkbDescPtr keyboard_map;

#include "input_helper.h"
#include "logger.h"

#define BUTTON_TABLE_MAX 16

static unsigned char *mouse_button_table;

// FIXME This should be static
Display *display = NULL;
static struct xkb_context *ctx = NULL;
static struct xkb_compose_state *compose_state;
static struct xkb_compose_table *compose_table = NULL;
static struct xkb_keymap *keymap = NULL;
static struct xkb_state *state = NULL;


static uint16_t modifier_mask;

static const uint32_t keysym_vcode_table[][2] = {
    { VC_ESCAPE,                XKB_KEY_Escape                },
    { VC_ESCAPE,                XKB_KEY_osfEscape             }, // HP OSF KeySym in HPkeysym.h

    // Begin Function Keys
    { VC_F1,                    XKB_KEY_F1                    },
    { VC_F2,                    XKB_KEY_F2                    },
    { VC_F3,                    XKB_KEY_F3                    },
    { VC_F4,                    XKB_KEY_F4                    },
    { VC_F5,                    XKB_KEY_F5                    },
    { VC_F6,                    XKB_KEY_F6                    },
    { VC_F7,                    XKB_KEY_F7                    },
    { VC_F8,                    XKB_KEY_F8                    },
    { VC_F9,                    XKB_KEY_F9                    },
    { VC_F10,                   XKB_KEY_F10                   },
    { VC_F11,                   XKB_KEY_F11                   },
    { VC_F11,                   XKB_KEY_SunF36                }, // Labeled F11 in Sunkeysym.h
    { VC_F12,                   XKB_KEY_F12                   },
    { VC_F12,                   XKB_KEY_SunF37                }, // Labeled F12 in Sunkeysym.h

    { VC_F13,                   XKB_KEY_F13                   },
    { VC_F14,                   XKB_KEY_F14                   },
    { VC_F15,                   XKB_KEY_F15                   },
    { VC_F16,                   XKB_KEY_F16                   },
    { VC_F17,                   XKB_KEY_F17                   },
    { VC_F18,                   XKB_KEY_F18                   },
    { VC_F19,                   XKB_KEY_F19                   },
    { VC_F20,                   XKB_KEY_F20                   },
    { VC_F21,                   XKB_KEY_F21                   },
    { VC_F22,                   XKB_KEY_F22                   },
    { VC_F23,                   XKB_KEY_F23                   },
    { VC_F24,                   XKB_KEY_F24                   },
    // End Function Keys


    // Begin Alphanumeric Zone
    { VC_BACK_QUOTE,            XKB_KEY_grave                 },

    { VC_0,                     XKB_KEY_0                     },
    { VC_1,                     XKB_KEY_1                     },
    { VC_2,                     XKB_KEY_2                     },
    { VC_3,                     XKB_KEY_3                     },
    { VC_4,                     XKB_KEY_4                     },
    { VC_5,                     XKB_KEY_5                     },
    { VC_6,                     XKB_KEY_6                     },
    { VC_7,                     XKB_KEY_7                     },
    { VC_8,                     XKB_KEY_8                     },
    { VC_9,                     XKB_KEY_9                     },

    { VC_MINUS,                 XKB_KEY_minus                 },
    { VC_PLUS,                  XKB_KEY_plus                  },
    { VC_EQUALS,                XKB_KEY_equal                 },
    { VC_ASTERISK,              XKB_KEY_asterisk              },

    { VC_AT,                    XKB_KEY_at                    },
    { VC_AMPERSAND,             XKB_KEY_ampersand             },
    { VC_DOLLAR,                XKB_KEY_dollar                },
    { VC_EXCLAMATION_MARK,      XKB_KEY_exclam                },
    { VC_EXCLAMATION_DOWN,      XKB_KEY_exclamdown            },

    { VC_BACKSPACE,             XKB_KEY_BackSpace             },
    { VC_BACKSPACE,             XKB_KEY_osfBackSpace          }, // HP OSF KeySym in HPkeysym.h

    { VC_TAB,                   XKB_KEY_Tab                   },
    { VC_TAB,                   XKB_KEY_ISO_Left_Tab          },
    { VC_CAPS_LOCK,             XKB_KEY_Caps_Lock             },
    { VC_CAPS_LOCK,             XKB_KEY_Shift_Lock            },

    { VC_A,                     XKB_KEY_A                     },
    { VC_B,                     XKB_KEY_B                     },
    { VC_C,                     XKB_KEY_C                     },
    { VC_D,                     XKB_KEY_D                     },
    { VC_E,                     XKB_KEY_E                     },
    { VC_F,                     XKB_KEY_F                     },
    { VC_G,                     XKB_KEY_G                     },
    { VC_H,                     XKB_KEY_H                     },
    { VC_I,                     XKB_KEY_I                     },
    { VC_J,                     XKB_KEY_J                     },
    { VC_K,                     XKB_KEY_K                     },
    { VC_L,                     XKB_KEY_L                     },
    { VC_M,                     XKB_KEY_M                     },
    { VC_N,                     XKB_KEY_N                     },
    { VC_O,                     XKB_KEY_O                     },
    { VC_P,                     XKB_KEY_P                     },
    { VC_Q,                     XKB_KEY_Q                     },
    { VC_R,                     XKB_KEY_R                     },
    { VC_S,                     XKB_KEY_S                     },
    { VC_T,                     XKB_KEY_T                     },
    { VC_U,                     XKB_KEY_U                     },
    { VC_V,                     XKB_KEY_V                     },
    { VC_W,                     XKB_KEY_W                     },
    { VC_X,                     XKB_KEY_X                     },
    { VC_Y,                     XKB_KEY_Y                     },
    { VC_Z,                     XKB_KEY_Z                     },

    { VC_OPEN_BRACKET,          XKB_KEY_bracketleft           },
    { VC_CLOSE_BRACKET,         XKB_KEY_bracketright          },
    { VC_BACK_SLASH,            XKB_KEY_backslash             },

    { VC_COLON,                 XKB_KEY_colon                 },
    { VC_SEMICOLON,             XKB_KEY_semicolon             },
    { VC_QUOTE,                 XKB_KEY_apostrophe            },
    { VC_QUOTEDBL,              XKB_KEY_quotedbl              },
    { VC_ENTER,                 XKB_KEY_Return,               },
    { VC_ENTER,                 XKB_KEY_Linefeed,             },

    { VC_LESS,                  XKB_KEY_less                  },
    { VC_GREATER,               XKB_KEY_greater               },
    { VC_COMMA,                 XKB_KEY_comma                 },
    { VC_PERIOD,                XKB_KEY_period                },
    { VC_SLASH,                 XKB_KEY_slash                 },
    { VC_NUMBER_SIGN,           XKB_KEY_numbersign            },

    { VC_OPEN_BRACE,            XKB_KEY_braceleft             },
    { VC_CLOSE_BRACE,           XKB_KEY_braceright            },

    { VC_OPEN_PARENTHESIS,      XKB_KEY_parenleft             },
    { VC_CLOSE_PARENTHESIS,     XKB_KEY_parenright            },

    { VC_SPACE,                 XKB_KEY_space                 },
    // End Alphanumeric Zone


    // Begin Edit Key Zone
    { VC_PRINT_SCREEN,          XKB_KEY_Print                 },
    { VC_PRINT_SCREEN,          XKB_KEY_SunPrint_Screen       }, // Same as XK_Print in Sunkeysym.h
    { VC_PRINT_SCREEN,          XKB_KEY_SunSys_Req            }, // SysReq should be the same as Print Screen
    { VC_SCROLL_LOCK,           XKB_KEY_Scroll_Lock,          },
    { VC_PAUSE,                 XKB_KEY_Pause                 },
    { VC_CANCEL,                XKB_KEY_Cancel                },
    { VC_CANCEL,                XKB_KEY_osfCancel             }, // HP OSF KeySym in HPkeysym.h
    { VC_INSERT,                XKB_KEY_Insert                },
    { VC_INSERT,                XKB_KEY_osfInsert             }, // HP OSF KeySym in HPkeysym.h
    { VC_DELETE,                XKB_KEY_Delete                },
    { VC_DELETE,                XKB_KEY_osfDelete             }, // HP OSF KeySym in HPkeysym.h
    { VC_HOME,                  XKB_KEY_Home                  },
    { VC_END,                   XKB_KEY_End                   },
    { VC_END,                   XKB_KEY_osfEndLine            }, // HP OSF KeySym in HPkeysym.h
    { VC_PAGE_UP,               XKB_KEY_Page_Up               },
    { VC_PAGE_UP,               XKB_KEY_Prior                 },
    { VC_PAGE_UP,               XKB_KEY_osfPageUp             }, // HP OSF KeySym in HPkeysym.h
    { VC_PAGE_UP,               XKB_KEY_osfPrior              }, // HP OSF KeySym in HPkeysym.h
    { VC_PAGE_DOWN,             XKB_KEY_Page_Down             },
    { VC_PAGE_DOWN,             XKB_KEY_Next                  },
    { VC_PAGE_DOWN,             XKB_KEY_osfPageDown           }, // HP OSF KeySym in HPkeysym.h
    { VC_PAGE_DOWN,             XKB_KEY_osfNext               }, // HP OSF KeySym in HPkeysym.h
    // End Edit Key Zone


    // Begin Cursor Key Zone
    { VC_UP,                    XKB_KEY_Up                    },
    { VC_UP,                    XKB_KEY_osfUp                 }, // HP OSF KeySym in HPkeysym.h
    { VC_LEFT,                  XKB_KEY_Left                  },
    { VC_LEFT,                  XKB_KEY_osfLeft               }, // HP OSF KeySym in HPkeysym.h
    { VC_BEGIN,                 XKB_KEY_Begin                 },
    { VC_RIGHT,                 XKB_KEY_Right                 },
    { VC_RIGHT,                 XKB_KEY_osfRight              }, // HP OSF KeySym in HPkeysym.h
    { VC_DOWN,                  XKB_KEY_Down                  },
    { VC_DOWN,                  XKB_KEY_osfDown               }, // HP OSF KeySym in HPkeysym.h
    // End Cursor Key Zone


    // Begin Numeric Zone
    { VC_NUM_LOCK,              XKB_KEY_Num_Lock              },
    { VC_KP_CLEAR,              XKB_KEY_Clear,                },
    { VC_KP_CLEAR,              XKB_KEY_osfClear              }, // HP OSF KeySym in HPkeysym.h

    { VC_KP_DIVIDE,             XKB_KEY_KP_Divide             },
    { VC_KP_MULTIPLY,           XKB_KEY_KP_Multiply           },
    { VC_KP_SUBTRACT,           XKB_KEY_KP_Subtract           },
    { VC_KP_EQUALS,             XKB_KEY_KP_Equal              },
    { VC_KP_ADD,                XKB_KEY_KP_Add                },
    { VC_KP_ENTER,              XKB_KEY_KP_Enter              },
    { VC_KP_DECIMAL,            XKB_KEY_KP_Decimal            },
    { VC_KP_SEPARATOR,          XKB_KEY_KP_Separator          },

    /* TODO Implement
    { VC_KP_SPACE,              XKB_KEY_KP_Space              },
    { VC_KP_TAB,                XKB_KEY_KP_Tab                },
    //*/

    { VC_KP_0,                  XKB_KEY_KP_0                  },
    { VC_KP_1,                  XKB_KEY_KP_1                  },
    { VC_KP_2,                  XKB_KEY_KP_2                  },
    { VC_KP_3,                  XKB_KEY_KP_3                  },
    { VC_KP_4,                  XKB_KEY_KP_4                  },
    { VC_KP_5,                  XKB_KEY_KP_5                  },
    { VC_KP_6,                  XKB_KEY_KP_6                  },
    { VC_KP_7,                  XKB_KEY_KP_7                  },
    { VC_KP_8,                  XKB_KEY_KP_8                  },
    { VC_KP_9,                  XKB_KEY_KP_9                  },

    { VC_KP_END,                XKB_KEY_KP_End                },
    { VC_KP_DOWN,               XKB_KEY_KP_Down               },
    { VC_KP_PAGE_DOWN,          XKB_KEY_KP_Page_Down          },
    { VC_KP_PAGE_DOWN,          XKB_KEY_KP_Next               },
    { VC_KP_LEFT,               XKB_KEY_KP_Left               },
    { VC_KP_BEGIN,              XKB_KEY_KP_Begin,             },
    { VC_KP_RIGHT,              XKB_KEY_KP_Right              },
    { VC_KP_HOME,               XKB_KEY_KP_Home               },
    { VC_KP_UP,                 XKB_KEY_KP_Up                 },
    { VC_KP_PAGE_UP,            XKB_KEY_KP_Page_Up            },
    { VC_KP_PAGE_UP,            XKB_KEY_KP_Prior              },
    { VC_KP_INSERT,             XKB_KEY_KP_Insert             },
    { VC_KP_DELETE,             XKB_KEY_KP_Delete             },
    // End Numeric Zone


    // Begin Modifier and Control Keys
    { VC_SHIFT_L,               XKB_KEY_Shift_L               },
    { VC_SHIFT_R,               XKB_KEY_Shift_R               },
    { VC_CONTROL_L,             XKB_KEY_Control_L             },
    { VC_CONTROL_R,             XKB_KEY_Control_R             },
    { VC_ALT_L,                 XKB_KEY_Alt_L                 },
    { VC_ALT_R,                 XKB_KEY_Alt_R                 },
    { VC_ALT_GRAPH,             XKB_KEY_ISO_Level3_Shift      },
    { VC_META_L,                XKB_KEY_Meta_L                },
    { VC_META_R,                XKB_KEY_Meta_R                },
    { VC_CONTEXT_MENU,          XKB_KEY_Menu                  },
    // End Modifier and Control Keys


    // Begin Shortcut Keys
    { VC_POWER,                 XKB_KEY_XF86PowerOff          },
    { VC_SLEEP,                 XKB_KEY_XF86Sleep             },
    { VC_WAKE,                  XKB_KEY_XF86WakeUp            },

    { VC_MEDIA_PLAY,            XKB_KEY_XF86AudioPlay         },
    { VC_MEDIA_STOP,            XKB_KEY_XF86AudioStop         },
    { VC_MEDIA_PREVIOUS,        XKB_KEY_XF86AudioPrev         },
    { VC_MEDIA_NEXT,            XKB_KEY_XF86AudioNext         },
    { VC_MEDIA_SELECT,          XKB_KEY_XF86Select            },
    { VC_MEDIA_EJECT,           XKB_KEY_XF86Eject             },

    { VC_VOLUME_MUTE,           XKB_KEY_XF86AudioMute         },
    { VC_VOLUME_MUTE,           XKB_KEY_SunAudioMute          },
    { VC_VOLUME_DOWN,           XKB_KEY_XF86AudioLowerVolume  },
    { VC_VOLUME_DOWN,           XKB_KEY_SunAudioLowerVolume   },
    { VC_VOLUME_UP,             XKB_KEY_XF86AudioRaiseVolume  },
    { VC_VOLUME_UP,             XKB_KEY_SunAudioRaiseVolume   },

    { VC_APP_BROWSER,           XKB_KEY_XF86WWW               },
    { VC_APP_CALCULATOR,        XKB_KEY_XF86Calculator        },
    { VC_APP_MAIL,              XKB_KEY_XF86Mail              },
    { VC_APP_MUSIC,             XKB_KEY_XF86Music             },
    { VC_APP_PICTURES,          XKB_KEY_XF86Pictures          },

    { VC_BROWSER_SEARCH,        XKB_KEY_XF86Search            },
    { VC_BROWSER_HOME,          XKB_KEY_XF86HomePage          },
    { VC_BROWSER_BACK,          XKB_KEY_XF86Back              },
    { VC_BROWSER_FORWARD,       XKB_KEY_XF86Forward           },
    { VC_BROWSER_STOP,          XKB_KEY_XF86Stop              },
    { VC_BROWSER_REFRESH,       XKB_KEY_XF86Refresh           },
    { VC_BROWSER_FAVORITES,     XKB_KEY_XF86Favorites         },
    // End Shortcut Keys


    // Begin European Language Keys
    { VC_CIRCUMFLEX,            XKB_KEY_asciicircum           },

    { VC_DEAD_GRAVE,            XKB_KEY_dead_grave            },
    { VC_DEAD_GRAVE,            XKB_KEY_SunFA_Grave           },
    { VC_DEAD_GRAVE,            XKB_KEY_Dgrave_accent          }, // DEC private keysym in DECkeysym.h
    { VC_DEAD_GRAVE,            XKB_KEY_hpmute_grave          }, // HP OSF KeySym in HPkeysym.h

    { VC_DEAD_ACUTE,            XKB_KEY_dead_acute            },
    { VC_DEAD_ACUTE,            XKB_KEY_SunFA_Acute           },
    { VC_DEAD_ACUTE,            XKB_KEY_Dacute_accent          }, // DEC private keysym in DECkeysym.h
    { VC_DEAD_ACUTE,            XKB_KEY_hpmute_acute          }, // HP OSF KeySym in HPkeysym.h

    { VC_DEAD_CIRCUMFLEX,       XKB_KEY_dead_circumflex       },
    { VC_DEAD_CIRCUMFLEX,       XKB_KEY_SunFA_Circum          },
    { VC_DEAD_CIRCUMFLEX,       XKB_KEY_Dcircumflex_accent     }, // DEC private keysym in DECkeysym.h
    { VC_DEAD_CIRCUMFLEX,       XKB_KEY_hpmute_asciicircum    }, // HP OSF KeySym in HPkeysym.h

    { VC_DEAD_TILDE,            XKB_KEY_dead_tilde            },
    { VC_DEAD_TILDE,            XKB_KEY_SunFA_Tilde           },
    { VC_DEAD_TILDE,            XKB_KEY_Dtilde                 }, // DEC private keysym in DECkeysym.h
    { VC_DEAD_TILDE,            XKB_KEY_hpmute_asciitilde     }, // HP OSF KeySym in HPkeysym.h

    { VC_DEAD_MACRON,           XKB_KEY_dead_macron           },
    { VC_DEAD_BREVE,            XKB_KEY_dead_breve            },
    { VC_DEAD_ABOVEDOT,         XKB_KEY_dead_abovedot         },

    { VC_DEAD_DIAERESIS,        XKB_KEY_dead_diaeresis        },
    { VC_DEAD_DIAERESIS,        XKB_KEY_SunFA_Diaeresis       },
    { VC_DEAD_DIAERESIS,        XKB_KEY_Ddiaeresis             }, // DEC private keysym in DECkeysym.h
    { VC_DEAD_DIAERESIS,        XKB_KEY_hpmute_diaeresis           }, // HP OSF KeySym in HPkeysym.h

    { VC_DEAD_ABOVERING,        XKB_KEY_dead_abovering        },
    { VC_DEAD_ABOVERING,        XKB_KEY_Dring_accent           }, // DEC private keysym in DECkeysym.h
    { VC_DEAD_DOUBLEACUTE,      XKB_KEY_dead_doubleacute      },
    { VC_DEAD_CARON,            XKB_KEY_dead_caron            },

    { VC_DEAD_CEDILLA,          XKB_KEY_dead_cedilla          },
    { VC_DEAD_CEDILLA,          XKB_KEY_SunFA_Cedilla         },
    { VC_DEAD_CEDILLA,          XKB_KEY_Dcedilla_accent        }, // DEC private keysym in DECkeysym.h

    { VC_DEAD_OGONEK,           XKB_KEY_dead_ogonek           },
    { VC_DEAD_IOTA,             XKB_KEY_dead_iota             },
    { VC_DEAD_VOICED_SOUND,     XKB_KEY_dead_voiced_sound     },
    { VC_DEAD_SEMIVOICED_SOUND, XKB_KEY_dead_semivoiced_sound },
    // End European Language Keys


    // Begin Asian Language Keys
    { VC_KATAKANA,              XKB_KEY_Katakana              },
    { VC_KANA,                  XKB_KEY_Kana_Shift            },
    { VC_KANA_LOCK,             XKB_KEY_Kana_Lock             },

    { VC_KANJI,                 XKB_KEY_Kanji                 },
    { VC_HIRAGANA,              XKB_KEY_Hiragana              },

    { VC_ACCEPT,                XKB_KEY_Execute               }, // Type 5c Japanese keyboard: kakutei
    { VC_CONVERT,               XKB_KEY_Kanji                 }, // Type 5c Japanese keyboard: henkan
    { VC_COMPOSE,               XKB_KEY_Multi_key             },
    { VC_INPUT_METHOD_ON_OFF,   XKB_KEY_Henkan_Mode           }, // Type 5c Japanese keyboard: nihongo

    { VC_ALL_CANDIDATES,        XKB_KEY_Zen_Koho              },
    { VC_ALPHANUMERIC,          XKB_KEY_Eisu_Shift            },
    { VC_ALPHANUMERIC,          XKB_KEY_Eisu_toggle           },
    { VC_CODE_INPUT,            XKB_KEY_Kanji_Bangou          },
    { VC_FULL_WIDTH,            XKB_KEY_Zenkaku               },
    { VC_HALF_WIDTH,            XKB_KEY_Hankaku               },
    { VC_NONCONVERT,            XKB_KEY_Muhenkan              },
    { VC_PREVIOUS_CANDIDATE,    XKB_KEY_Mae_Koho              },
    { VC_ROMAN_CHARACTERS,      XKB_KEY_Romaji                },

    { VC_UNDERSCORE,            XKB_KEY_underscore            },
    // End Asian Language Keys


    // Begin Sun Keys
    { VC_SUN_HELP,              XKB_KEY_Help                  },
    { VC_SUN_HELP,              XKB_KEY_osfHelp               },

    { VC_SUN_STOP,              XKB_KEY_Cancel                }, // FIXME Already used...
    { VC_SUN_STOP,              XKB_KEY_SunStop               }, // Same as XK_Cancel in Sunkeysym.h
    { VC_SUN_STOP,              XKB_KEY_L1                    },

    { VC_SUN_PROPS,             XKB_KEY_SunProps              },
    { VC_SUN_PROPS,             XKB_KEY_L3                    },

    { VC_SUN_FRONT,             XKB_KEY_SunFront              },
    { VC_SUN_OPEN,              XKB_KEY_SunOpen               },

    { VC_SUN_FIND,              XKB_KEY_Find                  },
    { VC_SUN_FIND,              XKB_KEY_L9                    },
    { VC_SUN_FIND,              XKB_KEY_SunFind               }, // Same as XK_Find in Sunkeysym.h

    { VC_SUN_AGAIN,             XKB_KEY_Redo                  },
    { VC_SUN_AGAIN,             XKB_KEY_L2                    },
    { VC_SUN_AGAIN,             XKB_KEY_SunAgain              }, // Same as XK_Redo in Sunkeysym.h

    { VC_SUN_UNDO,              XKB_KEY_Undo                  },
    { VC_SUN_UNDO,              XKB_KEY_L4                    },
    { VC_SUN_UNDO,              XKB_KEY_SunUndo               }, // Same as XK_Undo in Sunkeysym.h
    { VC_SUN_UNDO,              XKB_KEY_osfUndo               },

    { VC_SUN_COPY,              XKB_KEY_L6                    },
    { VC_SUN_COPY,              XKB_KEY_apCopy                },
    { VC_SUN_COPY,              XKB_KEY_SunCopy               },
    { VC_SUN_COPY,              XKB_KEY_osfCopy               },

    { VC_SUN_PASTE,             XKB_KEY_L8                    },
    { VC_SUN_PASTE,             XKB_KEY_SunPaste              },
    { VC_SUN_PASTE,             XKB_KEY_apPaste               },
    { VC_SUN_PASTE,             XKB_KEY_osfPaste              },

    { VC_SUN_CUT,               XKB_KEY_L10                   },
    { VC_SUN_CUT,               XKB_KEY_SunCut                },
    { VC_SUN_CUT,               XKB_KEY_apCut                 },
    { VC_SUN_CUT,               XKB_KEY_osfCut                },
    // End Sun Keys

    { VC_UNDEFINED,             XKB_KEY_NoSymbol              }
};


uint16_t keysym_to_uiocode(xkb_keysym_t keysym) {
    uint16_t uiocode = VC_UNDEFINED;

    keysym = xkb_keysym_to_upper(keysym);
    for (unsigned int i = 0; i < sizeof(keysym_vcode_table) / sizeof(keysym_vcode_table[0]); i++) {
        if (keysym == keysym_vcode_table[i][1]) {
            uiocode = keysym_vcode_table[i][0];
            break;
        }
    }

    // TODO VC_ALT_GRAPH MASK?
    if      (uiocode == VC_SHIFT_L)   { set_modifier_mask(MASK_SHIFT_L); }
    else if (uiocode == VC_SHIFT_R)   { set_modifier_mask(MASK_SHIFT_R); }
    else if (uiocode == VC_CONTROL_L) { set_modifier_mask(MASK_CTRL_L);  }
    else if (uiocode == VC_CONTROL_R) { set_modifier_mask(MASK_CTRL_R);  }
    else if (uiocode == VC_ALT_L)     { set_modifier_mask(MASK_ALT_L);   }
    else if (uiocode == VC_ALT_R)     { set_modifier_mask(MASK_ALT_R);   }
    else if (uiocode == VC_META_L)    { set_modifier_mask(MASK_META_L);  }
    else if (uiocode == VC_META_R)    { set_modifier_mask(MASK_META_R);  }

    // FIXME We shouldn't be doing this on each key press, do something similar to above.
    //initialize_locks();

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

xkb_keycode_t uiocode_to_keycode(uint16_t vcode) {
    KeyCode keycode = 0x0000;
    KeyCode keysym = NoSymbol;

    for (unsigned int i = 0; i < sizeof(keysym_vcode_table) / sizeof(keysym_vcode_table[0]); i++) {
        if (vcode == keysym_vcode_table[i][0]) {
            keycode = keysym_vcode_table[i][1];
            if (keysym = XKeysymToKeycode(display, keycode)) {
                break;
            }
        }
    }

    return keysym;
}

xkb_keycode_t event_to_keycode(uint16_t code) {
    return XkbMinLegalKeyCode + code;
}

xkb_keysym_t event_to_keysym(xkb_keycode_t keycode, enum xkb_key_state_t key_state) {
    xkb_keysym_t keysym = xkb_state_key_get_one_sym(state, keycode);

    if (key_state != KEY_STATE_REPEAT || xkb_keymap_key_repeats(keymap, keycode)) {
        if (key_state != KEY_STATE_RELEASE) {
            xkb_compose_state_feed(compose_state, keysym);
        }

        enum xkb_compose_status status = xkb_compose_state_get_status(compose_state);
        if (status == XKB_COMPOSE_CANCELLED || status == XKB_COMPOSE_COMPOSED) {
            xkb_compose_state_reset(compose_state);
        }

        if (key_state == KEY_STATE_RELEASE) {
            xkb_state_update_key(state, keycode, XKB_KEY_UP);
        } else {
            xkb_state_update_key(state, keycode, XKB_KEY_DOWN);
        }
    }

    return keysym;
}

uint8_t button_map_lookup(uint8_t button) {
    unsigned int map_button = button;

    if (display != NULL) {
        if (mouse_button_table != NULL) {
            int map_size = XGetPointerMapping(display, mouse_button_table, BUTTON_TABLE_MAX);
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
    if (xkb_state_led_name_is_active(state, XKB_LED_NAME_CAPS) > 0) {
        set_modifier_mask(MASK_CAPS_LOCK);
    } else {
        unset_modifier_mask(MASK_CAPS_LOCK);
    }

    if (xkb_state_led_name_is_active(state, XKB_LED_NAME_NUM) > 0) {
        set_modifier_mask(MASK_NUM_LOCK);
    } else {
        unset_modifier_mask(MASK_NUM_LOCK);
    }

    if (xkb_state_led_name_is_active(state, XKB_LED_NAME_SCROLL) > 0) {
        set_modifier_mask(MASK_SCROLL_LOCK);
    } else {
        unset_modifier_mask(MASK_SCROLL_LOCK);
    }
}

#ifdef USE_EPOCH_TIME
/* Get the current timestamp in unix epoch time. */
uint64_t get_unix_timestamp(struct timeval *event_time) {
    // Convert the event time to a Unix epoch in MS.
    uint64_t timestamp = (event_time->tv_sec * 1000) + (event_time->tv_usec / 1000);

    return timestamp;
}
#else
static uint64_t seq_timestamp = 0;
uint64_t get_seq_timestamp() {
    if (seq_timestamp == UINT64_MAX) {
        // TODO Warning
        seq_timestamp = 0;
    }
    return seq_timestamp++;
}
#endif

size_t keycode_to_utf8(xkb_keycode_t keycode, wchar_t *surrogate, size_t length) {
    size_t count = 0;
    if (surrogate == NULL || length == 0) {
        return count;
    }

    char buffer[5] = {};
    count = xkb_state_key_get_utf8(state, keycode, buffer, sizeof(buffer));


    // If we produced a string and we have a buffer, convert to 16-bit surrogate pairs.
    if (count > 0) {
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

    return count;
}

void load_input_helper() {
    display = XOpenDisplay(NULL);
    if (display == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to open X11 display!\n",
                __FUNCTION__, __LINE__);
    }
    xcb_connection_t *xcb_connection = XGetXCBConnection(display);

    ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!ctx) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to create xkb context!\n",
                __FUNCTION__, __LINE__);
    }

    const char *locale = setlocale(LC_CTYPE, NULL);
    compose_table = xkb_compose_table_new_from_locale(ctx, locale, XKB_COMPOSE_COMPILE_NO_FLAGS);
    if (!compose_table) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to create compose table from locale!\n",
                __FUNCTION__, __LINE__);
    }

    compose_state = xkb_compose_state_new(compose_table, XKB_COMPOSE_STATE_NO_FLAGS);
    if (!compose_state) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to create compose state from table!\n",
                __FUNCTION__, __LINE__);
    }

    int32_t device_id = xkb_x11_get_core_keyboard_device_id(xcb_connection);
    if (device_id == -1) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to get core keyboard device id!\n",
                __FUNCTION__, __LINE__);
    }

    keymap = xkb_x11_keymap_new_from_device(ctx, xcb_connection, device_id, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!keymap) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to create x11 keymap from device!\n",
                __FUNCTION__, __LINE__);
    }
    state = xkb_x11_state_new_from_device(keymap, xcb_connection, device_id);

    // Setup memory for mouse button mapping.
    mouse_button_table = malloc(sizeof(unsigned char) * BUTTON_TABLE_MAX);
    if (mouse_button_table == NULL) {
        logger(LOG_LEVEL_ERROR, "%s [%u]: Failed to allocate memory for mouse button map!\n",
                __FUNCTION__, __LINE__);

        //return UIOHOOK_ERROR_OUT_OF_MEMORY;
    }
}

void unload_input_helper() {
    if (state != NULL) {
        xkb_state_unref(state);
        state = NULL;
    }

    if (keymap != NULL) {
        xkb_keymap_unref(keymap);
        keymap = NULL;
    }

    if (compose_table != NULL) {
        xkb_compose_table_unref(compose_table);
        compose_table = NULL;
    }

    if (ctx != NULL) {
        xkb_context_unref(ctx);
        ctx = NULL;
    }

    if (display != NULL) {
        XCloseDisplay(display);
        display = NULL;
    }

    if (mouse_button_table != NULL) {
        free(mouse_button_table);
        mouse_button_table = NULL;
    }
}
