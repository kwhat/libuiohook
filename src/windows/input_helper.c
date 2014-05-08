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

/***********************************************************************
 * The following code is based on code provided by Marc-André Moreau
 * to work around a failure to support dead keys in the ToUnicode() API.
 * According to the author some parts were taken directly from
 * Microsoft's kbd.h header file that is shipped with the Windows Driver
 * Development Kit.
 *
 * The original code was substantially modified to provide the following:
 *   1) More dynamic code structure.
 *   2) Support for compilers that do not implement _ptr64 (GCC / LLVM).
 *   3) Support for Wow64 at runtime via 32-bit binary.
 *   4) Support for contextual language switching.
 *
 * I have contacted Marc-André Moreau who has granted permission for
 * his original source code to be used under the Public Domain.  Although
 * the libUIOHook library as a whole is currently covered under the LGPLv3,
 * please feel free to use and learn from the source code contained in this
 * file under the terms of the Public Domain.
 *
 * For further reading and the original code, please visit:
 *   http://legacy.docdroppers.org/wiki/index.php?title=Writing_Keyloggers
 *   http://www.techmantras.com/content/writing-keyloggers-full-length-tutorial
 *
 ***********************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <uiohook.h>
#include <windows.h>

#include "logger.h"
#include "input_helper.h"

static unsigned short vk_lookup_table[] = {
	VC_UNDEFINED,				// 0x00							Undefined
	MOUSE_BUTTON1,				// 0x01 VK_LBUTTON
	MOUSE_BUTTON2,				// 0x02 VK_RBUTTON
	VC_UNDEFINED,				// 0x03 VK_CANCEL
	MOUSE_BUTTON3,				// 0x04 VK_MBUTTON
	MOUSE_BUTTON4,				// 0x05 VK_XBUTTON1
	MOUSE_BUTTON5,				// 0x06 VK_XBUTTON2
	VC_UNDEFINED,				// 0x07							Undefined
	VC_UNDEFINED,				// 0x08 VK_BACK
	VC_TAB,						// 0x09 VK_TAB
	VC_UNDEFINED,				// 0x0A							Reserved
	VC_UNDEFINED,				// 0x0B							Reserved
	VC_UNDEFINED,				// 0x0C VK_CLEAR
	VC_ENTER,					// 0x0D VK_RETURN
	VC_UNDEFINED,				// 0x0E							Undefined
	VC_UNDEFINED,				// 0x0F							Undefined
	VC_SHIFT_L,					// 0x10 VK_SHIFT
	VC_CONTROL_L,				// 0x11 VK_CONTROL
	VC_ALT_L,					// 0x12 VK_MENU					ALT key

	VC_PAUSE,					// 0x13 VK_PAUSE
	VC_CAPS_LOCK,				// 0x14 VK_CAPITAL				CAPS LOCK key

	// FIXME
	VC_UNDEFINED,				// 0x15 VK_KANA					IME Kana mode

	VC_UNDEFINED,				// 0x16 Undefined

	// FIXME
	VC_UNDEFINED,				// 0x17 VK_JUNJA				IME Junja mode
	VC_UNDEFINED,				// 0x18 VK_FINAL
	VC_UNDEFINED,				// 0x19 VK_HANJA				IME Hanja mode
	VC_UNDEFINED,				// 0x19 VK_KANJI				IME Kanji mode

	VC_UNDEFINED,				// 0x1A							Undefined
	VC_ESCAPE,					// 0x1B VK_ESCAPE				ESC key
	VC_UNDEFINED,				// 0x1C VK_CONVERT				IME convert
	VC_UNDEFINED,				// 0x1D VK_NONCONVERT			IME nonconvert
	VC_UNDEFINED,				// 0x1E VK_ACCEPT				IME accept
	VC_UNDEFINED,				// 0x1F VK_MODECHANGE			IME mode change request
	VC_SPACE,					// 0x20 VK_SPACE				SPACEBAR
	VC_PAGE_UP,					// 0x21 VK_PRIOR				PAGE UP key
	VC_PAGE_DOWN,				// 0x22 VK_NEXT					PAGE DOWN key
	VC_END,						// 0x23 VK_END					END key
	VC_HOME,					// 0x24 VK_HOME					HOME key
	VC_LEFT,					// 0x25 VK_LEFT					LEFT ARROW key
	VC_UP,						// 0x26 VK_UP					UP ARROW key
	VC_RIGHT,					// 0x27 VK_RIGHT				RIGHT ARROW key
	VC_DOWN,					// 0x28 VK_DOWN					DOWN ARROW key
	VC_UNDEFINED,				// 0x29 VK_SELECT				SELECT key
	VC_UNDEFINED,				// 0x2A VK_PRINT				PRINT key
	VC_UNDEFINED,				// 0x2B VK_EXECUTE				EXECUTE key
	VC_PRINTSCREEN,				// 0x2C VK_SNAPSHOT				PRINT SCREEN key
	VC_INSERT,					// 0x2D VK_INSERT				INS key
	VC_DELETE,					// 0x2E VK_DELETE				DEL key
	VC_UNDEFINED,				// 0x2F VK_HELP					HELP key

	VC_0,						// 0x30							0 key
	VC_1,						// 0x31							1 key
	VC_2,						// 0x32							2 key
	VC_3,						// 0x33							3 key
	VC_4,						// 0x34							4 key
	VC_5,						// 0x35							5 key
	VC_6,						// 0x36							6 key
	VC_7,						// 0x37							7 key
	VC_8,						// 0x38							8 key
	VC_9,						// 0x39							9 key
	VC_UNDEFINED,				// 0x3A							Undefined
	VC_A,						// 0x41							A key
	VC_B,						// 0x42							B key
	VC_C,						// 0x43							C key
	VC_D,						// 0x44							D key
	VC_E,						// 0x45							E key
	VC_F,						// 0x46							F key
	VC_G,						// 0x47							G key
	VC_H,						// 0x48							H key
	VC_I,						// 0x49							I key
	VC_J,						// 0x4A							J key
	VC_K,						// 0x4B							K key
	VC_L,						// 0x4C							L key
	VC_M,						// 0x4D							M key
	VC_N,						// 0x4E							N key
	VC_O,						// 0x4F							O key
	VC_P,						// 0x50							P key
	VC_Q,						// 0x51							Q key
	VC_R,						// 0x52							R key
	VC_S,						// 0x53							S key
	VC_T,						// 0x54							T key
	VC_U,						// 0x55							U key
	VC_V,						// 0x56							V key
	VC_W,						// 0x57							W key
	VC_X,						// 0x58							X key
	VC_Y,						// 0x59							Y key
	VC_Z,						// 0x5A							Z key

	VC_META_L,					// 0x5B VK_LWIN 				Left Windows key (Natural keyboard)
	VC_META_R,					// 0x5C VK_RWIN					Right Windows key (Natural keyboard)

	// 0x5D VK_APPS					Applications key (Natural keyboard)
	VC_UNDEFINED,				// 0x5E Reserved
	VC_SLEEP,					// 0x5F VK_SLEEP				Computer Sleep key

	VC_KP_0,					// 0x60 VK_NUMPAD0				Numeric keypad 0 key
	VC_KP_1,					// 0x61 VK_NUMPAD1				Numeric keypad 1 key
	VC_KP_2,					// 0x62 VK_NUMPAD2				Numeric keypad 2 key
	VC_KP_3,					// 0x63 VK_NUMPAD3				Numeric keypad 3 key
	VC_KP_4,					// 0x64 VK_NUMPAD4				Numeric keypad 4 key
	VC_KP_5,					// 0x65 VK_NUMPAD5				Numeric keypad 5 key
	VC_KP_6,					// 0x66 VK_NUMPAD6				Numeric keypad 6 key
	VC_KP_7,					// 0x67 VK_NUMPAD7				Numeric keypad 7 key
	VC_KP_8,					// 0x68 VK_NUMPAD8				Numeric keypad 8 key
	VC_KP_9,					// 0x69 VK_NUMPAD9				Numeric keypad 9 key

	VC_KP_MULTIPLY,				// 0x6A VK_MULTIPLY				Multiply key
	VC_KP_ADD,					// 0x6B VK_ADD					Add key
	VC_KP_SEPARATOR,			// 0x6C VK_SEPARATOR			Separator key
	VC_KP_SUBTRACT,				// 0x6D VK_SUBTRACT				Subtract key
	VC_KP_SEPARATOR,			// 0x6E VK_DECIMAL				Decimal key
	VC_KP_DIVIDE,				// 0x6F VK_DIVIDE				Divide key

	VC_F1,						// 0x70 VK_F1					F1 key
	VC_F2,						// 0x71 VK_F2					F2 key
	VC_F3,						// 0x72 VK_F3					F3 key
	VC_F4,						// 0x73 VK_F4					F4 key
	VC_F5,						// 0x74 VK_F5					F5 key
	VC_F6,						// 0x75 VK_F6					F6 key
	VC_F7,						// 0x76 VK_F7					F7 key
	VC_F8,						// 0x77 VK_F8					F8 key
	VC_F9,						// 0x78 VK_F9					F9 key
	VC_F10,						// 0x79 VK_F10					F10 key
	VC_F11,						// 0x7A VK_F11					F11 key
	VC_F12,						// 0x7B VK_F12					F12 key

	VC_F13,						// 0x7C VK_F13					F13 key
	VC_F14,						// 0x7D VK_F14					F14 key
	VC_F15,						// 0x7E VK_F15					F15 key
	VC_F16,						// 0x7F VK_F16					F16 key
	VC_F17,						// 0x80 VK_F17					F17 key
	VC_F18,						// 0x81 VK_F18					F18 key
	VC_F19,						// 0x82 VK_F19					F19 key
	VC_F20,						// 0x83 VK_F20					F20 key
	VC_F21,						// 0x84 VK_F21					F21 key
	VC_F22,						// 0x85 VK_F22					F22 key
	VC_F23,						// 0x86 VK_F23					F23 key
	VC_F24,						// 0x87 VK_F24					F24 key

	VC_UNDEFINED,				// 0x88							Unassigned
	VC_UNDEFINED,				// 0x89							Unassigned
	VC_UNDEFINED,				// 0x8A							Unassigned
	VC_UNDEFINED,				// 0x8B							Unassigned
	VC_UNDEFINED,				// 0x8C							Unassigned
	VC_UNDEFINED,				// 0x8D							Unassigned
	VC_UNDEFINED,				// 0x8E							Unassigned
	VC_UNDEFINED,				// 0x8F							Unassigned

	VC_NUM_LOCK,				// 0x90 VK_NUMLOCK				NUM LOCK key
	VC_SCROLL_LOCK,				// 0x91 VK_SCROLL				SCROLL LOCK key

	VC_UNDEFINED,				// 0x92							OEM specific
	VC_UNDEFINED,				// 0x93							OEM specific
	VC_UNDEFINED,				// 0x94							OEM specific
	VC_UNDEFINED,				// 0x95							OEM specific
	VC_UNDEFINED,				// 0x96							OEM specific

	VC_UNDEFINED,				// 0x97							Unassigned
	VC_UNDEFINED,				// 0x98							Unassigned
	VC_UNDEFINED,				// 0x99							Unassigned
	VC_UNDEFINED,				// 0x9A							Unassigned
	VC_UNDEFINED,				// 0x9B							Unassigned
	VC_UNDEFINED,				// 0x9C							Unassigned
	VC_UNDEFINED,				// 0x9D							Unassigned
	VC_UNDEFINED,				// 0x9F							Unassigned

	VC_SHIFT_L,					// 0xA0 VK_LSHIFT				Left SHIFT key
	VC_SHIFT_R,					// 0xA1 VK_RSHIFT				Right SHIFT key
	VC_CONTROL_L,				// 0xA2 VK_LCONTROL				Left CONTROL key
	VC_CONTROL_R,				// 0xA3 VK_RCONTROL				Right CONTROL key
	VC_ALT_L,					// 0xA4 VK_LMENU				Left MENU key
	VC_ALT_R,					// 0xA5 VK_RMENU				Right MENU key

	VC_BROWSER_BACK,			// 0xA6 VK_BROWSER_BACK			Browser Back key
	VC_BROWSER_FORWARD,			// 0xA7 VK_BROWSER_FORWARD		Browser Forward key
	VC_BROWSER_REFRESH,			// 0xA8 VK_BROWSER_REFRESH		Browser Refresh key
	VC_BROWSER_STOP,			// 0xA9 VK_BROWSER_STOP			Browser Stop key
	VC_BROWSER_SEARCH,			// 0xAA VK_BROWSER_SEARCH		Browser Search key
	VC_BROWSER_FAVORITES,		// 0xAB VK_BROWSER_FAVORITES	Browser Favorites key
	VC_BROWSER_HOME,			// 0xAC VK_BROWSER_HOME			Browser Start and Home key

	VC_VOLUME_MUTE,				// 0xAD VK_VOLUME_MUTE			Volume Mute key
	VC_VOLUME_UP,				// 0xAE VK_VOLUME_DOWN			Volume Down key
	VC_VOLUME_DOWN,				// 0xAF VK_VOLUME_UP			Volume Up key

	VC_MEDIA_NEXT,				// 0xB0 VK_MEDIA_NEXT_TRACK		Next Track key
	VC_MEDIA_PREVIOUS,			// 0xB1 VK_MEDIA_PREV_TRACK		Previous Track key
	VC_MEDIA_STOP,				// 0xB2 VK_MEDIA_STOP			Stop Media key
	VC_MEDIA_PLAY,				// 0xB3 VK_MEDIA_PLAY_PAUSE		Play/Pause Media key

	VC_APP_MAIL,				// 0xB4 VK_LAUNCH_MAIL			Start Mail key

	VC_MEDIA_SELECT,			// 0xB5 VK_LAUNCH_MEDIA_SELECT	Select Media key
	VC_APP_MAIL,				// 0xB6 VK_LAUNCH_APP1			Start Application 1 key
	VC_APP_CALCULATOR,			// 0xB7 VK_LAUNCH_APP2			Start Application 2 key

	VC_UNDEFINED,				// 0xB8							Reserved
	VC_UNDEFINED,				// 0xB9							Reserved

	VC_SEMICOLON,				// 0xBA VK_OEM_1				Varies by keyboard. For the US standard keyboard, the ';:' key
	VC_EQUALS,					// 0xBB VK_OEM_PLUS				For any country/region, the '+' key
	VC_COMMA,					// 0xBC VK_OEM_COMMA			For any country/region, the ',' key
	VC_MINUS,					// 0xBD VK_OEM_MINUS			For any country/region, the '-' key
	VC_PERIOD,					// 0xBE VK_OEM_PERIOD			For any country/region, the '.' key
	VC_SLASH,					// 0xBF VK_OEM_2				Varies by keyboard. For the US standard keyboard, the '/?' key
	VC_BACKQUOTE,				// 0xC0 VK_OEM_3				Varies by keyboard. For the US standard keyboard, the '`~' key

	VC_UNDEFINED,				// 0xC1							Reserved
	VC_UNDEFINED,				// 0xC2							Reserved
	VC_UNDEFINED,				// 0xC3							Reserved
	VC_UNDEFINED,				// 0xC4							Reserved
	VC_UNDEFINED,				// 0xC5							Reserved
	VC_UNDEFINED,				// 0xC6							Reserved
	VC_UNDEFINED,				// 0xC7							Reserved
	VC_UNDEFINED,				// 0xC8							Reserved
	VC_UNDEFINED,				// 0xC9							Reserved
	VC_UNDEFINED,				// 0xCA							Reserved
	VC_UNDEFINED,				// 0xCB							Reserved
	VC_UNDEFINED,				// 0xCC							Reserved
	VC_UNDEFINED,				// 0xCD							Reserved
	VC_UNDEFINED,				// 0xCE							Reserved
	VC_UNDEFINED,				// 0xCF							Reserved
	VC_UNDEFINED,				// 0xD0							Reserved
	VC_UNDEFINED,				// 0xD1							Reserved
	VC_UNDEFINED,				// 0xD2							Reserved
	VC_UNDEFINED,				// 0xD3							Reserved
	VC_UNDEFINED,				// 0xD4							Reserved
	VC_UNDEFINED,				// 0xD5							Reserved
	VC_UNDEFINED,				// 0xD6							Reserved
	VC_UNDEFINED,				// 0xD7							Reserved

	VC_UNDEFINED,				// 0xD8							Unassigned
	VC_UNDEFINED,				// 0xD9							Unassigned
	VC_UNDEFINED,				// 0xDA							Unassigned

	VC_OPEN_BRACKET,			// 0xDB VK_OEM_4				Varies by keyboard. For the US standard keyboard, the '[{' key
	VC_BACK_SLASH,				// 0xDC VK_OEM_5				Varies by keyboard. For the US standard keyboard, the '\|' key
	VC_CLOSE_BRACKET,			// 0xDD VK_OEM_6				Varies by keyboard. For the US standard keyboard, the ']}' key
	VC_QUOTE,					// 0xDE VK_OEM_7				Varies by keyboard. For the US standard keyboard, the 'single-quote/double-quote' key

	VC_UNDEFINED,				// 0xDF VK_OEM_8	 			Varies by keyboard.
	VC_UNDEFINED,				// 0xE0							Reserved
	VC_UNDEFINED,				// 0xE1							OEM specific
	VC_UNDEFINED,				// 0xE2 VK_OEM_102				Either the angle bracket key or the backslash key on the RT 102-key keyboard
	VC_UNDEFINED,				// 0xE3							OEM specific
	VC_UNDEFINED,				// 0xE4							OEM specific
	VC_UNDEFINED,				// 0xE5 VK_PROCESSKEY			IME PROCESS key
	VC_UNDEFINED,				// 0xE6							OEM specific
	VC_UNDEFINED,				// 0xE7 VK_PACKET				Used to pass Unicode characters as if they were keystrokes. The VK_PACKET key is the low word of a 32-bit Virtual Key value used for non-keyboard input methods.

	VC_UNDEFINED,				// 0xE8							Unassigned
	VC_UNDEFINED,				// 0xE9							OEM specific
	VC_UNDEFINED,				// 0xEA							OEM specific
	VC_UNDEFINED,				// 0xEB							OEM specific
	VC_UNDEFINED,				// 0xEC							OEM specific
	VC_UNDEFINED,				// 0xED							OEM specific
	VC_UNDEFINED,				// 0xEE							OEM specific
	VC_UNDEFINED,				// 0xF0							OEM specific
	VC_UNDEFINED,				// 0xF1							OEM specific
	VC_UNDEFINED,				// 0xF2							OEM specific
	VC_UNDEFINED,				// 0xF3							OEM specific
	VC_UNDEFINED,				// 0xF4							OEM specific
	VC_UNDEFINED,				// 0xF5							OEM specific

	VC_UNDEFINED,				// 0xF6 VK_ATTN					Attn key
	VC_UNDEFINED,				// 0xF7 VK_CRSEL				CrSel key
	VC_UNDEFINED,				// 0xF8 VK_EXSEL				ExSel key
	VC_UNDEFINED,				// 0xF9 VK_EREOF				Erase EOF key
	VC_UNDEFINED,				// 0xFA VK_PLAY					Play key
	VC_UNDEFINED,				// 0xFB VK_ZOOM					Zoom key
	VC_UNDEFINED,				// 0xFC VK_NONAME				Reserved
	VC_UNDEFINED,				// 0xFD VK_PA1					PA1 key
	VC_UNDEFINED,				// 0xFE VK_OEM_CLEAR			Clear key
};

// Structure and pointers for the keyboard locale cache.
typedef struct _KeyboardLocale {
	HKL id;									// Locale ID
	HINSTANCE library;						// Keyboard DLL instance.
	PVK_TO_BIT pVkToBit;					// Pointers struct arrays.
	PVK_TO_WCHAR_TABLE pVkToWcharTable;
	PDEADKEY pDeadKey;
	struct _KeyboardLocale* next;
} KeyboardLocale;

static KeyboardLocale* locale_first = NULL;
static KeyboardLocale* locale_current = NULL;

// Amount of pointer padding to apply for Wow64 instances.
static short int ptr_padding = 0;

#if defined(_WIN32) && !defined(_WIN64)
// Small function to check and see if we are executing under Wow64.
static BOOL is_wow64() {
	BOOL status = FALSE;

	LPFN_ISWOW64PROCESS pIsWow64Process = (LPFN_ISWOW64PROCESS)
			GetProcAddress(GetModuleHandle("kernel32"), "IsWow64Process");

	if (pIsWow64Process != NULL) {
		HANDLE current_proc = GetCurrentProcess();

		if (!pIsWow64Process(current_proc, &status)) {
			status = FALSE;

			logger(LOG_LEVEL_DEBUG,	"%s [%u]: pIsWow64Process(%#p, (%#p) failed!\n",
				__FUNCTION__, __LINE__, current_proc, &status);
		}
	}

	return status;
}
#endif

// Locate the DLL that contains the current keyboard layout.
static int get_keyboard_layout_file(char *layoutFile, DWORD bufferSize) {
	int status = UIOHOOK_FAILURE;
	HKEY hKey;
	DWORD varType = REG_SZ;

	char kbdName[KL_NAMELENGTH];
	if (GetKeyboardLayoutName(kbdName)) {
		char kbdKeyPath[51 + KL_NAMELENGTH];
		snprintf(kbdKeyPath, 51 + KL_NAMELENGTH, "SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts\\%s", kbdName);

		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, (LPCTSTR) kbdKeyPath, 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
			if (RegQueryValueEx(hKey, "Layout File", NULL, &varType, (LPBYTE) layoutFile, &bufferSize) == ERROR_SUCCESS) {
				RegCloseKey(hKey);
				status = UIOHOOK_SUCCESS;
			}
		}
	}

	return status;
}

static int refresh_locale_list() {
	int count = 0;

	// Get the number of layouts the user has activated.
	int hkl_size = GetKeyboardLayoutList(0, NULL);
	if (hkl_size > 0) {
		logger(LOG_LEVEL_INFO,	"%s [%u]: GetKeyboardLayoutList(0, NULL) found %i layouts.\n",
				__FUNCTION__, __LINE__, hkl_size);

		// Get the thread id that currently has focus for our default.
		DWORD focus_pid = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
		HKL hlk_focus = GetKeyboardLayout(focus_pid);
		HKL hlk_default = GetKeyboardLayout(0);
		HKL *hkl_list = malloc(sizeof(HKL) * hkl_size);

		int new_size = GetKeyboardLayoutList(hkl_size, hkl_list);
		if (new_size > 0) {
			if (new_size != hkl_size) {
				logger(LOG_LEVEL_ERROR,	"%s [%u]: Locale size mismatch!  "
						"Expected %i, received %i!\n",
						__FUNCTION__, __LINE__, hkl_size, new_size);
			}
			else {
				logger(LOG_LEVEL_INFO,	"%s [%u]: Received %i locales.\n",
						__FUNCTION__, __LINE__, new_size);
			}

			KeyboardLocale* locale_previous = NULL;
			KeyboardLocale* locale_item = locale_first;

			// Go though the linked list and remove KeyboardLocale's that are
			// no longer loaded.
			while (locale_item != NULL) {
				// Check to see if the old HKL is in the new list.
				bool is_loaded = false;
				for (int i = 0; i < new_size && !is_loaded; i++) {
					if (locale_item->id == hkl_list[i]) {
						// Flag and jump out of the loop.
						hkl_list[i] = NULL;
						is_loaded = true;
					}
				}


				if (is_loaded) {
					logger(LOG_LEVEL_DEBUG,	"%s [%u]: Found loacle ID %#p in the cache.\n",
							__FUNCTION__, __LINE__, locale_item->id);

					// Set the previous local to the current locale.
					locale_previous = locale_item;

					// Check and see if the locale is our current active locale.
					if (locale_item->id == hlk_focus) {
						locale_current = locale_item;
					}

					count++;
				}
				else {
					logger(LOG_LEVEL_DEBUG,	"%s [%u]: Removing loacle ID %#p from the cache.\n",
							__FUNCTION__, __LINE__, locale_item->id);

					// If the old id is not in the new list, remove it.
					locale_previous->next = locale_item->next;

					// Make sure the locale_current points NULL or something valid.
					if (locale_item == locale_current) {
						locale_current = NULL;
					}

					// Free the memory used by locale_item;
					free(locale_item);

					// Set the item to the pervious item to guarantee a next.
					locale_item = locale_previous;
				}

				// Iterate to the next linked list item.
				locale_item = locale_item->next;
			}


			// Insert anything new into the linked list.
			for (int i = 0; i < new_size; i++) {
				// Check to see if the item was already in the list.
				if (hkl_list[i] != NULL) {
					// TODO Unload this to a function, See else clause.

					// Set the active keyboard layout for this thread to the HKL.
					ActivateKeyboardLayout(hkl_list[i], 0x00);

					// Try to pull the current keyboard layout DLL from the registry.
					char layoutFile[MAX_PATH];
					if (get_keyboard_layout_file(layoutFile, sizeof(layoutFile)) == UIOHOOK_SUCCESS) {
						//You can't trust the %SYSPATH%, look it up manually.
						char systemDirectory[MAX_PATH];
						if (GetSystemDirectory(systemDirectory, MAX_PATH) != 0) {
							char kbdLayoutFilePath[MAX_PATH];
							snprintf(kbdLayoutFilePath, MAX_PATH, "%s\\%s", systemDirectory, layoutFile);

							logger(LOG_LEVEL_DEBUG,	"%s [%u]: Loading layout for %#p: %s.\n",
									__FUNCTION__, __LINE__, hkl_list[i], layoutFile);

							// Create the new locale item.
							locale_item = malloc(sizeof(KeyboardLocale));
							locale_item->id = hkl_list[i];
							locale_item->library = LoadLibrary(kbdLayoutFilePath);

							// Get the function pointer from the library to get the keyboard layer descriptor.
							KbdLayerDescriptor pKbdLayerDescriptor = (KbdLayerDescriptor) GetProcAddress(locale_item->library, "KbdLayerDescriptor");
							if (pKbdLayerDescriptor != NULL) {
								PKBDTABLES pKbd = pKbdLayerDescriptor();

								// Store the memory address of the following 3 structures.
								BYTE *base = (BYTE *) pKbd;

								// First element of each structure, no offset adjustment needed.
								locale_item->pVkToBit = pKbd->pCharModifiers->pVkToBit;

								// Second element of pKbd, +4 byte offset on wow64.
								locale_item->pVkToWcharTable = *((PVK_TO_WCHAR_TABLE *) (base + offsetof(KBDTABLES, pVkToWcharTable) + ptr_padding));

								// Third element of pKbd, +8 byte offset on wow64.
								locale_item->pDeadKey = *((PDEADKEY *) (base + offsetof(KBDTABLES, pDeadKey) + (ptr_padding * 2)));


								// This will always be added to the end of the list.
								locale_item->next = NULL;

								// Insert the item into the linked list.
								if (locale_previous == NULL) {
									// If nothing came before, the list is empty.
									locale_first = locale_item;
								}
								else {
									// Append the new locale to the end of the list.
									locale_previous->next = locale_item;
								}

								// Check and see if the locale is our current active locale.
								if (locale_item->id == hlk_focus) {
									locale_current = locale_item;
								}

								// Set the pervious locale item to the new one.
								locale_previous = locale_item;

								count++;
							}
							else {
								logger(LOG_LEVEL_ERROR,
										"%s [%u]: GetProcAddress() failed for KbdLayerDescriptor!\n",
										__FUNCTION__, __LINE__);

								FreeLibrary(locale_item->library);
								free(locale_item);
								locale_item = NULL;
							}
						}
						else {
							logger(LOG_LEVEL_ERROR,
									"%s [%u]: GetSystemDirectory() failed!\n",
									__FUNCTION__, __LINE__);
						}
					}
					else {
						logger(LOG_LEVEL_ERROR,
								"%s [%u]: Could not find keyboard map for locale %#p!\n",
								__FUNCTION__, __LINE__, hkl_list[i]);
					}
				} // End NULL Check.
			} // for (...)
		}
		else {
			logger(LOG_LEVEL_ERROR,
					"%s [%u]: GetKeyboardLayoutList() failed!\n",
					__FUNCTION__, __LINE__);

			// TODO Try and recover by using the current layout.
			// Hint: Use locale_id instead of hkl_list[i] in the loop above.
		}

		free(hkl_list);
		ActivateKeyboardLayout(hlk_default, 0x00);
	}

	return count;
}

int load_input_helper() {
	int count = 0;

	#if defined(_WIN32) && !defined(_WIN64)
	if (is_wow64()) {
		ptr_padding = sizeof(void *);
	}
	#endif

	count = refresh_locale_list();

	logger(LOG_LEVEL_INFO,
			"%s [%u]: refresh_locale_list() found %i locale(s).\n",
			__FUNCTION__, __LINE__, count);

	return count;
}

// This returns the number of locales that were removed.
int unload_input_helper() {
	int count = 0;

	// Cleanup and free memory from the old list.
	KeyboardLocale* locale_item = locale_first;
	while (locale_item != NULL) {
		// Remove the first item from the linked list.
		FreeLibrary(locale_item->library);
		locale_first = locale_item->next;
		free(locale_item);
		locale_item = locale_first;

		count++;
	}

	// Reset the current local.
	locale_current = NULL;

	return count;
}

int convert_vk_to_wchar(int virtualKey, PWCHAR outputChar, PWCHAR deadChar) {
	// Get the thread id that currently has focus and
	DWORD focus_pid = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
	HKL locale_id = GetKeyboardLayout(focus_pid);

	// If the current Locale is not the new locale, search the linked list.
	if (locale_current == NULL || locale_current->id != locale_id) {
		locale_current = NULL;
		KeyboardLocale* locale_item = locale_first;
		while (locale_item != NULL) {
			// Search the linked list.
			if (locale_item->id == locale_id) {
				logger(LOG_LEVEL_INFO,
					"%s [%u]: Activating keyboard layout %#p.\n",
					__FUNCTION__, __LINE__, locale_item->id);

				// If they layout changes the dead key state needs to be reset.
				// This is consistent with the way Windows handles locale changes.
				*deadChar = 0;
				locale_current = locale_item;
				locale_item = NULL;
			}
			else {
				locale_item = locale_item->next;
			}
		}

		// If we were unable to find the locale in the list, refresh the list.
		if (locale_current == NULL) {
			logger(LOG_LEVEL_DEBUG,
					"%s [%u]: Refreshing locale cache.\n",
					__FUNCTION__, __LINE__);

			refresh_locale_list();
		}
	}


	int charCount = 0;
	*outputChar = 0;

	// Check and make sure the Unicode helper was loaded.
	if (locale_current != NULL) {
		logger(LOG_LEVEL_INFO,
				"%s [%u]: Using keyboard layout %#p.\n",
				__FUNCTION__, __LINE__, locale_current->id);

		int mod = 0;

		WCHAR baseChar;
		WCHAR diacritic;

		int capsLock = (GetKeyState(VK_CAPITAL) & 0x01);

		PVK_TO_BIT pVkToBit = locale_current->pVkToBit;
		PVK_TO_WCHAR_TABLE pVkToWcharTable = locale_current->pVkToWcharTable;
		PDEADKEY pDeadKey = locale_current->pDeadKey;

		/* Loop over the modifier keys for this locale and determine what is
		 * currently depressed.  Because this is only a structure of two
		 * bytes, we don't need to worry about the structure padding of __ptr64
		 * offsets on Wow64.
		 */
		bool is_shift = false, is_ctrl = false, is_alt = false;
		for (int i = 0; pVkToBit[i].Vk != 0; i++) {
			short state = GetAsyncKeyState(pVkToBit[i].Vk);

			// Check to see if the most significant bit is active.
			if (state & ~SHRT_MAX) {
				if (pVkToBit[i].Vk == VK_SHIFT) {
					is_shift = true;
				}
				else if (pVkToBit[i].Vk == VK_CONTROL) {
					is_ctrl = true;
				}
				else if (pVkToBit[i].Vk == VK_MENU) {
					is_alt = true;
				}
			}
		}

		// Check the Shift modifier.
		if (is_shift) {
			mod = 1;
		}

		// Check for the AltGr modifier.
		if (is_ctrl && is_alt) {
			mod += 3;
		}

		// Default 32 bit structure size should be 6 bytes (4 for the pointer and 2
		// additional byte fields) that are padded out to 8 bytes by the compiler.
		unsigned short sizeVkToWcharTable = sizeof(VK_TO_WCHAR_TABLE);
		#if defined(_WIN32) && !defined(_WIN64)
		if (is_wow64()) {
			// If we are running under Wow64 the size of the first pointer will be
			// 8 bringing the total size to 10 bytes padded out to 16.
			sizeVkToWcharTable = (sizeVkToWcharTable + ptr_padding + 7) & -8;
		}
		#endif

		BYTE *ptrCurrentVkToWcharTable = (BYTE *) pVkToWcharTable;

		int cbSize, n;
		do {
			// cbSize is used to calculate n, and n is used for the size of pVkToWchars[j].wch[n]
			cbSize = *(ptrCurrentVkToWcharTable + offsetof(VK_TO_WCHAR_TABLE, cbSize) + ptr_padding);
			n = (cbSize - 2) / 2;

			// Same as VK_TO_WCHARS pVkToWchars[] = pVkToWcharTable[i].pVkToWchars
			PVK_TO_WCHARS pVkToWchars = (PVK_TO_WCHARS) ((PVK_TO_WCHAR_TABLE) ptrCurrentVkToWcharTable)->pVkToWchars;

			if (pVkToWchars != NULL && mod < n) {
				// pVkToWchars[j].VirtualKey
				BYTE *pCurrentVkToWchars = (BYTE *) pVkToWchars;

				do {
					if (((PVK_TO_WCHARS) pCurrentVkToWchars)->VirtualKey == virtualKey) {
						if ((((PVK_TO_WCHARS) pCurrentVkToWchars)->Attributes == CAPLOK) && capsLock) {
							if (is_shift && mod > 0) {
								mod -= 1;
							}
							else {
								mod += 1;
							}
						}
						*outputChar = ((PVK_TO_WCHARS) pCurrentVkToWchars)->wch[mod];
						charCount = 1;

						// Increment the pCurrentVkToWchars by the size of wch[n].
						pCurrentVkToWchars += sizeof(VK_TO_WCHARS) + (sizeof(WCHAR) * n);

						if (*outputChar == WCH_NONE) {
							charCount = 0;
						}
						else if (*outputChar == WCH_DEAD) {
							*deadChar = ((PVK_TO_WCHARS) pCurrentVkToWchars)->wch[mod];
							charCount = 0;
						}
						break;
					}
					else {
						// Add sizeof WCHAR because we are really an array of WCHAR[n] not WCHAR[]
						pCurrentVkToWchars += sizeof(VK_TO_WCHARS) + (sizeof(WCHAR) * n);
					}
				} while ( ((PVK_TO_WCHARS) pCurrentVkToWchars)->VirtualKey != 0 );
			}

			// This is effectively the same as: ptrCurrentVkToWcharTable = pVkToWcharTable[++i];
			ptrCurrentVkToWcharTable += sizeVkToWcharTable;
		} while (cbSize != 0);


		// Code to check for dead characters...
		if (*deadChar != 0) {
			for (int i = 0; pDeadKey[i].dwBoth != 0; i++) {
				baseChar = (WCHAR) pDeadKey[i].dwBoth;
				diacritic = (WCHAR) (pDeadKey[i].dwBoth >> 16);

				if ((baseChar == *outputChar) && (diacritic == *deadChar)) {
					*deadChar = 0;
					*outputChar = (WCHAR) pDeadKey[i].wchComposed;
				}
			}
		}
	}

	return charCount;
}

unsigned short convert_vk_to_scancode(DWORD vk_code) {
	unsigned short scancode = VC_UNDEFINED;

	// Check the vk_code is in range.
	if (vk_code >= 0 && vk_code < sizeof(vk_lookup_table) / sizeof(vk_lookup_table[0])) {
		scancode = vk_lookup_table[vk_code];
	}

	return scancode;
}
