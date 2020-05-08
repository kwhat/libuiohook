# the name of the target operating system
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_BUILD_TYPE MinSizeRel)

# which compilers to use for C
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_C_FLAGS "-march=armv7-a -mfpu=vfp -mfloat-abi=hard -fomit-frame-pointer -flto -ffat-lto-objects -fno-stack-protector -g -pipe")

set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=gold -Wl,-O1 -Wl,--as-needed")
set(CMAKE_SHARED_LINKER_FLAGS "-fuse-ld=gold -Wl,-O1 -Wl,--as-needed")

set(THREADS_PTHREAD_ARG "2" CACHE STRING "Forcibly set by CMakeLists.txt." FORCE)

set(CMAKE_FIND_ROOT_PATH /usr/arm-linux-gnueabihf/)

# the target environment lib locations
set(CMAKE_LIBRARY_PATH /usr/lib/arm-linux-gnueabihf/)

# adjust the default behaviour of the FIND_XXX() commands:
# search headers and libraries in the target environment, search
# programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
