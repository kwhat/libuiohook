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

int tests_run = 0;

static char * test_bidirectional_scancodes() {

	for (unsigned short i1 = 0; i1 < 256; i1++) {
		printf("Testing keycode %u...\n", i1);

		uint16_t scancode = vk_to_scancode(i1);
		if (scancode > 127) {
			printf("\tproduced scancode offset %u %#X\n", (scancode & 0xFF) + 128, scancode);
		}
		else {
			printf("\tproduced scancode %u %#X\n", scancode, scancode);
		}

		KeyCode i2 = scancode_to_vk(scancode);
		printf("\treproduced keycode %u\n", i2);

		if (scancode != VC_UNDEFINED) {
			mu_assert("error, scancode to keycode failed to convert back", i1 == i2);
		}
	}

	return 0;
}

 static char * all_tests() {
     mu_run_test(test_bidirectional_scancodes);

     return NULL;
 }

int main() {
	load_input_helper();

	char *result = all_tests();
	if (result != NULL) {
		status = 0;
		printf("%s\n", result);
	}
	else {
		printf("ALL TESTS PASSED\n");
	}
	printf("Tests run: %d\n", tests_run);

	unload_input_helper();

	return status;
}
