# the name of the target operating system
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_BUILD_TYPE RelWithDebInfo)

# which compilers to use for C
set(CMAKE_C_COMPILER cl.exe)
set(CMAKE_C_FLAGS "/arch:ARMv7VE")

set(CMAKE_EXE_LINKER_FLAGS "/DEFAULTLIB:advapi32.lib")
set(CMAKE_SHARED_LINKER_FLAGS "/DEFAULTLIB:advapi32.lib")
