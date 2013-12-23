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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <uiohook.h>

#include "library_load.h"
#include "logger.h"
#include "osx_input_helper.h"

// Create a shared object constructor and destructor.

// FIXME This still does not work on OS X presumably because it is not required
// by anything when statically linked.

__attribute__ ((constructor))
void on_library_load() {
	// Display the copyright on library load.
	COPYRIGHT();

	load_input_helper();
}

__attribute__ ((destructor))
void on_library_unload() {
	unload_input_helper();
}

// FIXME This is only here to preserve constructors during static linking.  
// This should go away after platform refactoring.
void test(){}
