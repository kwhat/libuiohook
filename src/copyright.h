/* libUIOHook: Cross-platfrom userland keyboard and mouse hooking.
 * Copyright (C) 2006-2015 Alexander Barker.  All Rights Received.
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

#ifndef _included_copyright
#define _included_copyright

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef USE_QUIET
#define COPYRIGHT() (void) 0;
#else
#include <stdio.h>
#define COPYRIGHT()	fprintf(stdout, \
		"libUIOHook: Cross-platfrom userland keyboard and mouse hooking.\n" \
		"Copyright (C) 2006-2015 Alexander Barker.  All Rights Received.\n" \
		"https://github.com/kwhat/libuiohook/\n" \
		"\n" \
		"libUIOHook is free software: you can redistribute it and/or modify\n" \
		"it under the terms of the GNU Lesser General Public License as published\n" \
		"by the Free Software Foundation, either version 3 of the License, or\n" \
		"(at your option) any later version.\n" \
		"\n" \
		"libUIOHook is distributed in the hope that it will be useful,\n" \
		"but WITHOUT ANY WARRANTY; without even the implied warranty of\n" \
		"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n" \
		"GNU General Public License for more details.\n" \
		"\n" \
		"You should have received a copy of the GNU Lesser General Public License\n" \
		"along with this program.  If not, see <http://www.gnu.org/licenses/>.\n\n");
#endif

#endif
