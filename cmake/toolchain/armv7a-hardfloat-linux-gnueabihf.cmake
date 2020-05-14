# libUIOHook: Cross-platform userland keyboard and mouse hooking.
# Copyright (C) 2006-2020 Alexander Barker.  All Rights Received.
# https://github.com/kwhat/libuiohook/
#
# libUIOHook is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# libUIOHook is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_BUILD_TYPE MinSizeRel)

set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_C_FLAGS "-march=armv7-a -mfpu=vfp -mfloat-abi=hard -flto -fomit-frame-pointer -fno-stack-protector -g -pipe")

set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=gold -Wl,-O1 -Wl,--as-needed")
set(CMAKE_SHARED_LINKER_FLAGS "-fuse-ld=gold -Wl,-O1 -Wl,--as-needed")

# ubuntu stuff...
set(CMAKE_FIND_ROOT_PATH /usr/arm-linux-gnueabihf/)
set(CMAKE_LIBRARY_PATH /usr/lib/arm-linux-gnueabihf/)

# adjust the default behaviour of the FIND_XXX() commands:
# search headers and libraries in the target environment, search
# programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
