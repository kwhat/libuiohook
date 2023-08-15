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

#ifndef __UIOHOOK_H
#define __UIOHOOK_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>

/* Begin Error Codes */
#define UIOHOOK_SUCCESS                          0x00
#define UIOHOOK_FAILURE                          0x01

// System level errors.
#define UIOHOOK_ERROR_OUT_OF_MEMORY              0x02

// Unix specific errors.
#define UIOHOOK_ERROR_X_OPEN_DISPLAY             0x20
#define UIOHOOK_ERROR_X_RECORD_NOT_FOUND         0x21
#define UIOHOOK_ERROR_X_RECORD_ALLOC_RANGE       0x22
#define UIOHOOK_ERROR_X_RECORD_CREATE_CONTEXT    0x23
#define UIOHOOK_ERROR_X_RECORD_ENABLE_CONTEXT    0x24
#define UIOHOOK_ERROR_X_RECORD_GET_CONTEXT       0x25

#define UIOHOOK_ERROR_EPOLL_CREATE               0x26

// Windows specific errors.
#define UIOHOOK_ERROR_SET_WINDOWS_HOOK_EX        0x30
#define UIOHOOK_ERROR_GET_MODULE_HANDLE          0x31

// Darwin specific errors.
#define UIOHOOK_ERROR_AXAPI_DISABLED             0x40
#define UIOHOOK_ERROR_CREATE_EVENT_PORT          0x41
#define UIOHOOK_ERROR_CREATE_RUN_LOOP_SOURCE     0x42
#define UIOHOOK_ERROR_GET_RUNLOOP                0x43
#define UIOHOOK_ERROR_CREATE_OBSERVER            0x44
/* End Error Codes */

/* Begin Log Levels and Function Prototype */
typedef enum _log_level {
    LOG_LEVEL_DEBUG = 1,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} log_level;

// Logger callback function prototype.
typedef void (*logger_t)(unsigned int, void *, const char *, va_list);
/* End Log Levels and Function Prototype */

/* Begin Virtual Event Types and Data Structures */
typedef enum _event_type {
    EVENT_HOOK_ENABLED = 1,
    EVENT_HOOK_DISABLED,
    EVENT_KEY_TYPED,
    EVENT_KEY_PRESSED,
    EVENT_KEY_RELEASED,
    EVENT_MOUSE_CLICKED,
    EVENT_MOUSE_PRESSED,
    EVENT_MOUSE_RELEASED,
    EVENT_MOUSE_MOVED,
    EVENT_MOUSE_DRAGGED,
    EVENT_MOUSE_WHEEL
} event_type;

typedef struct _screen_data {
    uint8_t number;
    int16_t x;
    int16_t y;
    uint16_t width;
    uint16_t height;
} screen_data;

typedef struct _keyboard_event_data {
    uint16_t keycode;
    uint16_t rawcode;
    wchar_t keychar;
} keyboard_event_data,
  key_pressed_event_data,
  key_released_event_data,
  key_typed_event_data;

typedef struct _mouse_event_data {
    uint16_t button;
    uint16_t clicks;
    int16_t x;
    int16_t y;
} mouse_event_data,
  mouse_pressed_event_data,
  mouse_released_event_data,
  mouse_clicked_event_data;

typedef struct _mouse_wheel_event_data {
    int16_t x;
    int16_t y;
    uint8_t type;
    int16_t rotation;
    uint16_t delta;
    uint8_t direction;
} mouse_wheel_event_data;

typedef struct _uiohook_event {
    event_type type;
    uint64_t time;
    uint16_t mask;
    uint16_t reserved;
    union {
        keyboard_event_data keyboard;
        mouse_event_data mouse;
        mouse_wheel_event_data wheel;
    } data;
} uiohook_event;

typedef void (*dispatcher_t)(uiohook_event * const, void *);
/* End Virtual Event Types and Data Structures */


/* Begin Virtual Key Codes */
#define VC_ESCAPE                                0x001B

// Begin Function Keys
#define VC_F1                                    0x0070
#define VC_F2                                    0x0071
#define VC_F3                                    0x0072
#define VC_F4                                    0x0073
#define VC_F5                                    0x0074
#define VC_F6                                    0x0075
#define VC_F7                                    0x0076
#define VC_F8                                    0x0077
#define VC_F9                                    0x0078
#define VC_F10                                   0x0079
#define VC_F11                                   0x007A
#define VC_F12                                   0x007B

#define VC_F13                                   0xF000
#define VC_F14                                   0xF001
#define VC_F15                                   0xF002
#define VC_F16                                   0xF003
#define VC_F17                                   0xF004
#define VC_F18                                   0xF005
#define VC_F19                                   0xF006
#define VC_F20                                   0xF007
#define VC_F21                                   0xF008
#define VC_F22                                   0xF009
#define VC_F23                                   0xF00A
#define VC_F24                                   0xF00B
// End Function Keys


// Begin Alphanumeric Zone
#define VC_BACK_QUOTE                            0x00C0    // GRAVE
#define VC_BACKQUOTE                      VC_BACK_QUOTE    // Deprecated

#define VC_0                                     0x0030
#define VC_1                                     0x0031
#define VC_2                                     0x0032
#define VC_3                                     0x0033
#define VC_4                                     0x0034
#define VC_5                                     0x0035
#define VC_6                                     0x0036
#define VC_7                                     0x0037
#define VC_8                                     0x0038
#define VC_9                                     0x0039

#define VC_PLUS                                  0x0209
#define VC_MINUS                                 0x002D
#define VC_EQUALS                                0x003D
#define VC_ASTERISK                              0x0097

#define VC_AT                                    0x0200
#define VC_AMPERSAND                             0x0096
#define VC_DOLLAR                                0x0203
#define VC_EXCLAMATION_MARK                      0x0205
#define VC_EXCLAMATION_DOWN                      0x0206    // Colombia & Mexico replaces '+/='

#define VC_BACKSPACE                             0x0008

#define VC_TAB                                   0x0009
#define VC_CAPS_LOCK                             0x0014

#define VC_A                                     0x0041
#define VC_B                                     0x0042
#define VC_C                                     0x0043
#define VC_D                                     0x0044
#define VC_E                                     0x0045
#define VC_F                                     0x0046
#define VC_G                                     0x0047
#define VC_H                                     0x0048
#define VC_I                                     0x0049
#define VC_J                                     0x004A
#define VC_K                                     0x004B
#define VC_L                                     0x004C
#define VC_M                                     0x004D
#define VC_N                                     0x004E
#define VC_O                                     0x004F
#define VC_P                                     0x0050
#define VC_Q                                     0x0051
#define VC_R                                     0x0052
#define VC_S                                     0x0053
#define VC_T                                     0x0054
#define VC_U                                     0x0055
#define VC_V                                     0x0056
#define VC_W                                     0x0057
#define VC_X                                     0x0058
#define VC_Y                                     0x0059
#define VC_Z                                     0x005A

#define VC_OPEN_BRACKET                          0x005B
#define VC_CLOSE_BRACKET                         0x005C
#define VC_BACK_SLASH                            0x005D

#define VC_COLON                                 0x0201
#define VC_SEMICOLON                             0x003B
#define VC_QUOTE                                 0x00DE
#define VC_QUOTEDBL                              0x0098
#define VC_ENTER                                 0x000A

#define VC_LESS                                  0x0099
#define VC_GREATER                               0x00A0
#define VC_COMMA                                 0x002C
#define VC_PERIOD                                0x002E
#define VC_SLASH                                 0x002F
#define VC_NUMBER_SIGN                           0x0208     // Used by Germany where the '/" key would be

#define VC_OPEN_BRACE                            0x00A1
#define VC_CLOSE_BRACE                           0x00A2

#define VC_OPEN_PARENTHESIS                      0x0207
#define VC_CLOSE_PARENTHESIS                     0x020A

#define VC_SPACE                                 0x0020
// End Alphanumeric Zone


// Begin Edit Key Zone
#define VC_PRINT_SCREEN                          0x009A    // SYSRQ
#define VC_PRINTSCREEN                  VC_PRINT_SCREEN    // Deprecated
#define VC_SCROLL_LOCK                           0x0091
#define VC_PAUSE                                 0x0013
#define VC_CANCEL                                0x00D3    // BREAK
#define VC_LESSER_GREATER                  VC_UNDEFINED    // Deprecated

#define VC_INSERT                                0x009B
#define VC_DELETE                                0x007F
#define VC_HOME                                  0x0024
#define VC_END                                   0x0023
#define VC_PAGE_UP                               0x0021
#define VC_PAGE_DOWN                             0x0022
// End Edit Key Zone


// Begin Cursor Key Zone
#define VC_UP                                    0x0026
#define VC_LEFT                                  0x0025
#define VC_BEGIN                                 0xFF58
#define VC_RIGHT                                 0x0027
#define VC_DOWN                                  0x0028
// End Cursor Key Zone


// Begin Numeric Zone
#define VC_NUM_LOCK                              0x0090
#define VC_KP_CLEAR                              0x000C
#define VC_CLEAR                            VC_KP_CLEAR    // Deprecated

#define VC_KP_DIVIDE                             0x006F
#define VC_KP_MULTIPLY                           0x006A
#define VC_KP_SUBTRACT                           0x006D
#define VC_KP_EQUALS                             0x007C
#define VC_KP_ADD                                0x006B
#define VC_KP_ENTER                              0x007D
#define VC_KP_DECIMAL                            0x006E
#define VC_KP_SEPARATOR                          0x006C
#define VC_KP_COMMA                              0x007E    // This may only be available on OS X?

#define VC_KP_0                                  0x0060
#define VC_KP_1                                  0x0061
#define VC_KP_2                                  0x0062
#define VC_KP_3                                  0x0063
#define VC_KP_4                                  0x0064
#define VC_KP_5                                  0x0065
#define VC_KP_6                                  0x0066
#define VC_KP_7                                  0x0067
#define VC_KP_8                                  0x0068
#define VC_KP_9                                  0x0069

#define VC_KP_END                                0xEE00 | VC_KP_1
#define VC_KP_DOWN                               0xEE00 | VC_KP_2
#define VC_KP_PAGE_DOWN                          0xEE00 | VC_KP_3
#define VC_KP_LEFT                               0xEE00 | VC_KP_4
#define VC_KP_BEGIN                              0xEE00 | VC_KP_5
#define VC_KP_RIGHT                              0xEE00 | VC_KP_6
#define VC_KP_HOME                               0xEE00 | VC_KP_7
#define VC_KP_UP                                 0xEE00 | VC_KP_8
#define VC_KP_PAGE_UP                            0xEE00 | VC_KP_9
#define VC_KP_INSERT                             0xEE00 | VC_KP_0
#define VC_KP_DELETE                             0xEE00 | VC_KP_SEPARATOR
// End Numeric Zone


// Begin Modifier and Control Keys
#define VC_SHIFT_L                               0xA010
#define VC_SHIFT_R                               0xB010
#define VC_CONTROL_L                             0xA011
#define VC_CONTROL_R                             0xB011
#define VC_ALT_L                                 0xA012    // Option or Alt Key
#define VC_ALT_R                                 0xB012    // Option or Alt Key
#define VC_ALT_GRAPH                             0xFF7E    // Replaces Right Alt Key
#define VC_META_L                                0xA09D    // Windows or Command Key
#define VC_META_R                                0xB09D    // Windows or Command Key
#define VC_CONTEXT_MENU                          0x020D
// End Modifier and Control Keys


// Begin Shortcut Keys
#define VC_POWER                                 0xE05E
#define VC_SLEEP                                 0xE05F
#define VC_WAKE                                  0xE063

#define VC_MEDIA_PLAY                            0xE022
#define VC_MEDIA_STOP                            0xE024
#define VC_MEDIA_PREVIOUS                        0xE010
#define VC_MEDIA_NEXT                            0xE019
#define VC_MEDIA_SELECT                          0xE06D
#define VC_MEDIA_EJECT                           0xE02C

#define VC_VOLUME_MUTE                           0xE020
#define VC_VOLUME_DOWN                           0xE030
#define VC_VOLUME_UP                             0xE02E

#define VC_APP_BROWSER                           0xE020
#define VC_APP_CALCULATOR                        0xE021
#define VC_APP_MAIL                              0xE06C
#define VC_APP_MUSIC                             0xE03C
#define VC_APP_PICTURES                          0xE064

#define VC_BROWSER_SEARCH                        0xE065
#define VC_BROWSER_HOME                          0xE032
#define VC_BROWSER_BACK                          0xE06A
#define VC_BROWSER_FORWARD                       0xE069
#define VC_BROWSER_STOP                          0xE068
#define VC_BROWSER_REFRESH                       0xE067
#define VC_BROWSER_FAVORITES                     0xE066
// End Shortcut Keys


// Begin European Language Keys
#define VC_CIRCUMFLEX                            0x0202
#define VC_DEAD_GRAVE                            0x0080
#define VC_DEAD_ACUTE                            0x0081
#define VC_DEAD_CIRCUMFLEX                       0x0082
#define VC_DEAD_TILDE                            0x0083
#define VC_DEAD_MACRON                           0x0084
#define VC_DEAD_BREVE                            0x0085
#define VC_DEAD_ABOVEDOT                         0x0086
#define VC_DEAD_DIAERESIS                        0x0087
#define VC_DEAD_ABOVERING                        0x0088
#define VC_DEAD_DOUBLEACUTE                      0x0089
#define VC_DEAD_CARON                            0x008A
#define VC_DEAD_CEDILLA                          0x008B
#define VC_DEAD_OGONEK                           0x008C
#define VC_DEAD_IOTA                             0x008D
#define VC_DEAD_VOICED_SOUND                     0x008E
#define VC_DEAD_SEMIVOICED_SOUND                 0x008F
// End European Language Keys


// Begin Asian Language Keys
#define VC_KATAKANA                              0x00F1
#define VC_KANA                                  0x0015
#define VC_KANA_LOCK                             0x0106

#define VC_KANJI                                 0x0019
#define VC_HIRAGANA                              0x00F2

#define VC_ACCEPT                                0x001E
#define VC_CONVERT                               0x001C
#define VC_COMPOSE                               0xFF20
#define VC_INPUT_METHOD_ON_OFF                   0x0107

#define VC_ALL_CANDIDATES                        0x0100
#define VC_ALPHANUMERIC                          0x00F0
#define VC_CODE_INPUT                            0x0102
#define VC_FULL_WIDTH                            0x00F3
#define VC_HALF_WIDTH                            0x00F4
#define VC_NONCONVERT                            0x001D
#define VC_PREVIOUS_CANDIDATE                    0x0101
#define VC_ROMAN_CHARACTERS                      0x00F5

#define VC_UNDERSCORE                            0x020B
#define VC_YEN                             VC_UNDEFINED    // Deprecated
// End Asian Language Keys

// Begin Sun Keys
#define VC_SUN_HELP                              0xFF75

#define VC_SUN_STOP                              0xFF78
#define VC_SUN_PROPS                             0xFF76
#define VC_SUN_FRONT                             0xFF77
#define VC_SUN_OPEN                              0xFF74
#define VC_SUN_FIND                              0xFF7E
#define VC_SUN_AGAIN                             0xFF79
#define VC_SUN_UNDO                              0xFF7A
#define VC_SUN_COPY                              0xFF7C
#define VC_SUN_PASTE                             0xFF7D
#define VC_SUN_INSERT                      VC_SUN_PASTE    // Deprecated
#define VC_SUN_CUT                               0xFF7B
// End Sun Keys

#define VC_UNDEFINED                             0x0000    // KeyCode Unknown

#define CHAR_UNDEFINED                           0xFFFF    // CharCode Unknown
/* End Virtual Key Codes */


/* Begin Virtual Modifier Masks */
#define MASK_SHIFT_L                             1 << 0
#define MASK_CTRL_L                              1 << 1
#define MASK_META_L                              1 << 2
#define MASK_ALT_L                               1 << 3

#define MASK_SHIFT_R                             1 << 4
#define MASK_CTRL_R                              1 << 5
#define MASK_META_R                              1 << 6
#define MASK_ALT_R                               1 << 7

#define MASK_SHIFT                               MASK_SHIFT_L | MASK_SHIFT_R
#define MASK_CTRL                                MASK_CTRL_L  | MASK_CTRL_R
#define MASK_META                                MASK_META_L  | MASK_META_R
#define MASK_ALT                                 MASK_ALT_L   | MASK_ALT_R

#define MASK_BUTTON1                             1 << 8
#define MASK_BUTTON2                             1 << 9
#define MASK_BUTTON3                             1 << 10
#define MASK_BUTTON4                             1 << 11
#define MASK_BUTTON5                             1 << 12

#define MASK_NUM_LOCK                            1 << 13
#define MASK_CAPS_LOCK                           1 << 14
#define MASK_SCROLL_LOCK                         1 << 15
/* End Virtual Modifier Masks */


/* Begin Virtual Mouse Buttons */
#define MOUSE_NOBUTTON                           0    // Any Button
#define MOUSE_BUTTON1                            1    // Left Button
#define MOUSE_BUTTON2                            2    // Right Button
#define MOUSE_BUTTON3                            3    // Middle Button
#define MOUSE_BUTTON4                            4    // Extra Mouse Button
#define MOUSE_BUTTON5                            5    // Extra Mouse Button

#define WHEEL_UNIT_SCROLL                        1    // Scroll by line
#define WHEEL_BLOCK_SCROLL                       2    // Scroll by page

#define WHEEL_VERTICAL_DIRECTION                 3
#define WHEEL_HORIZONTAL_DIRECTION               4
/* End Virtual Mouse Buttons */


#ifdef _WIN32
#define UIOHOOK_API __declspec(dllexport)
#else
#define UIOHOOK_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

    // Set the logger callback function.
    UIOHOOK_API void hook_set_logger_proc(logger_t logger_proc, void *user_data);

    // Set the event callback function.
    UIOHOOK_API void hook_set_dispatch_proc(dispatcher_t dispatch_proc, void *user_data);

    // Send a virtual event back to the system.
    UIOHOOK_API int hook_post_event(uiohook_event * const event);

    // Insert the event hook.
    UIOHOOK_API int hook_run();

    // Withdraw the event hook.
    UIOHOOK_API int hook_stop();

    // Retrieves an array of screen data for each available monitor.
    UIOHOOK_API screen_data* hook_create_screen_info(unsigned char *count);

    // Retrieves the keyboard auto repeat rate.
    UIOHOOK_API long int hook_get_auto_repeat_rate();

    // Retrieves the keyboard auto repeat delay.
    UIOHOOK_API long int hook_get_auto_repeat_delay();

    // Retrieves the mouse acceleration multiplier.
    UIOHOOK_API long int hook_get_pointer_acceleration_multiplier();

    // Retrieves the mouse acceleration threshold.
    UIOHOOK_API long int hook_get_pointer_acceleration_threshold();

    // Retrieves the mouse sensitivity.
    UIOHOOK_API long int hook_get_pointer_sensitivity();

    // Retrieves the double/triple click interval.
    UIOHOOK_API long int hook_get_multi_click_time();

#ifdef __cplusplus
}
#endif

#endif
