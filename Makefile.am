# libUIOHook: Cross-platfrom userland keyboard and mouse hooking.
# Copyright (C) 2006-2014 Alexander Barker.  All Rights Received.
# https://github.com/kwhat/libuiohook/
#
# libUIOHook is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# libUIOHook is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

ACLOCAL_AMFLAGS = -I m4

HOOK_SRC = src/logger.c
TEST_SRC = 
if BUILD_DARWIN
HOOK_SRC += src/darwin/hook_callback.c \
	 src/darwin/hook_thread.c \
	 src/darwin/input_helper.c \
	 src/darwin/post_event.c \
	 src/darwin/system_properties.c
TEST_SRC += test/darwin/input_helper_test.c
endif

if BUILD_X11
HOOK_SRC += src/x11/hook_callback.c \
	 src/x11/hook_thread.c \
	 src/x11/input_helper.c \
	 src/x11/post_event.c \
	 src/x11/system_properties.c
TEST_SRC += test/x11/input_helper_test.c
endif

if BUILD_WINDOWS
HOOK_SRC += src/windows/hook_callback.c \
	 src/windows/hook_thread.c \
	 src/windows/input_helper.c \
	 src/windows/post_event.c \
	 src/windows/system_properties.c
TEST_SRC += test/windows/input_helper_test.c
endif

pkgconfigdir = $(libdir)/pc
pkgconfig_DATA = $(top_builddir)/pc/uiohook.pc

include_HEADERS = include/uiohook.h

bin_PROGRAMS =
lib_LTLIBRARIES = libuiohook.la


libuiohook_la_CFLAGS = $(AM_CFLAGS) -Wall -Wextra -pedantic -Wno-unused-parameter -I$(top_srcdir)/include -I$(top_srcdir)/src/$(backend) -I$(top_srcdir)/src
libuiohook_la_LDFLAGS = $(LTLDFLAGS) $(LDFLAGS)
libuiohook_la_SOURCES = $(HOOK_SRC)


if BUILD_DEMO
bin_PROGRAMS += demohook demopost

demohook_SOURCES = src/demo_hook.c
demohook_LDADD = $(top_builddir)/libuiohook.la
demohook_CFLAGS = $(AM_CFLAGS) -Wall -Wextra -pedantic $(DEMO_CFLAGS) -I$(top_srcdir)/include
demohook_LDFLAGS = $(LTLDFLAGS) $(DEMO_LIBS)

demopost_SOURCES = src/demo_post.c
demopost_LDADD = $(top_builddir)/libuiohook.la
demopost_CFLAGS = $(AM_CFLAGS) -Wall -Wextra -pedantic $(DEMO_CFLAGS) -I$(top_srcdir)/include
demopost_LDFLAGS = $(LTLDFLAGS) $(DEMO_LIBS)
endif


if BUILD_TEST
bin_PROGRAMS += testhook

testhook_SOURCES = $(TEST_SRC)
testhook_LDADD = $(top_builddir)/libuiohook.la
testhook_CFLAGS = $(AM_CFLAGS) -Wall -Wextra -pedantic $(TEST_CFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/test -I$(top_srcdir)/src/$(backend) -I$(top_srcdir)/src
testhook_LDFLAGS = $(LTLDFLAGS) $(TEST_LIBS)
endif


EXTRA_DIST = COPYING.md COPYING.LESSER.md README.md
