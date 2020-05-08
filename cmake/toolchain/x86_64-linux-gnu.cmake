# the name of the target operating system
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_BUILD_TYPE RelWithDebInfo)

# which compilers to use for C
set(CMAKE_C_COMPILER x86_64-linux-gnu-gcc)
set(CMAKE_C_FLAGS "-march=x86-64 -mtune=generic -fomit-frame-pointer -flto -ffat-lto-objects -fno-stack-protector -pipe")

set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=gold -Wl,-O1 -Wl,--as-needed")
set(CMAKE_SHARED_LINKER_FLAGS "-fuse-ld=gold -Wl,-O1 -Wl,--as-needed")

set(THREADS_PTHREAD_ARG "2" CACHE STRING "Forcibly set by CMakeLists.txt." FORCE)

set(CMAKE_FIND_ROOT_PATH /usr/x86_64-linux-gnu)

# the target environment lib locations
set(CMAKE_LIBRARY_PATH /usr/lib/x86_64-linux-gnu/;/usr/lib64/)

# adjust the default behaviour of the FIND_XXX() commands:
# search headers and libraries in the target environment, search
# programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
