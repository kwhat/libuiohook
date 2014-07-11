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

#include <stdint.h>
#include <stdio.h>

#include "input_helper.h"
#include "minunit.h"
#include "uiohook.h"

static char * test_bidirectional_keycode() {
	for (unsigned short i1 = 0; i1 < 256; i1++) {
		printf("Testing keycode %u...\n", i1);

		#ifdef _WIN32
		if ((i1 > 6 && i1 < 16) || i1 > 18) {
		#endif
			uint16_t scancode = keycode_to_scancode(i1);
			if (scancode > 127) {
				printf("\tproduced scancode offset %u %#X\n", (scancode & 0xFF) + 128, scancode);
			}
			else {
				printf("\tproduced scancode %u %#X\n", scancode, scancode);
			}

			uint16_t i2 = (uint16_t) scancode_to_keycode(scancode);
			printf("\treproduced keycode %u\n", i2);

			if (scancode != VC_UNDEFINED) {
				mu_assert("error, scancode to keycode failed to convert back", i1 == i2);
			}
		#ifdef _WIN32
		}
		#endif
	}

	return NULL;
}

static char * test_bidirectional_scancode() {
	for (unsigned short i1 = 0; i1 < 256; i1++) {
		printf("Testing scancode %u...\n", i1);
		
		uint16_t keycode;
		if (i1 < 128) {
			// Lower 0-127
			keycode = (uint16_t) scancode_to_keycode(i1);
		}
		else {
			// Upper 128-255
			keycode = (uint16_t) scancode_to_keycode(0xFF00 | (i1 % 128));
		}
		printf("\tproduced keycode %u %#X\n", keycode, keycode);

		// Convert native keycode back to a virtual scancode.
		uint16_t i2 = keycode_to_scancode(keycode);
		// Pull the scancode back into range.
		if (i2 > 127) {
			i2 = (i2 & 0x007F) + 128;
		}

		printf("\treproduced scancode %u\n", i2);

		if (keycode != VC_UNDEFINED) {
			mu_assert("error, scancode to keycode failed to convert back", i1 == i2);
		}
	}

	return NULL;
}

char * input_helper_tests() {
	mu_run_test(test_bidirectional_keycode);
	mu_run_test(test_bidirectional_scancode);

	return NULL;
}
