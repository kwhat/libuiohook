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

cmake_minimum_required(VERSION 3.10)

project(libuiohook VERSION 1.2.0 LANGUAGES C)

# Generates a demo project
function (register_demo dname)
    add_executable("${dname}" "./demo/${dname}.c")
    if (ENABLE_SHARED)
        add_dependencies("${dname}" shared)
        target_link_libraries("${dname}" shared)
    elseif (ENABLE_STATIC)
        add_dependencies("${dname}" static)
        target_link_libraries("${dname}" static)
    else()
        add_dependencies("${dname}" uiohook)
        target_link_libraries("${dname}" uiohook)
    endif()

    target_link_libraries("${dname}" "${CMAKE_THREAD_LIBS_INIT}")

    if(MSVC AND MSVC_VERSION LESS "1900")
        set_target_properties("${dname}" PROPERTIES COMPILE_FLAGS
            "-Dinline=__inline -D_CRT_SECURE_NO_WARNINGS -Dsnprintf=_snprintf")
    else()
        set_target_properties("${dname}" PROPERTIES COMPILE_FLAGS
            "-Dinline=__inline -D_CRT_SECURE_NO_WARNINGS")
    endif()
endfunction()

# Creates a CMake option which will be set as a macro if enabled
function (macro_option opt desc default)
    option(${opt} ${desc} ${default})
    if (${opt})
        add_definitions("-D${opt}")
    endif()
endfunction()

# Registers a dependency for the static, shared and normal library
function (register_library lib include)
    target_link_libraries(uiohook ${lib})
    target_include_directories(uiohook PRIVATE ${include})

    if (ENABLE_STATIC)
        target_link_libraries(static ${lib})
        target_include_directories(static PRIVATE ${include})
    endif()

    if (ENABLE_SHARED)
        target_link_libraries(shared ${lib})
        target_include_directories(shared PRIVATE ${include})
    endif()

    message(STATUS "Registering ${lib} ${include}")
endfunction()

# Creates a CMake option for X11 libraries including
function (x11_option opt val desc default)
    macro_option(${opt} ${desc} ${default})

    if (${opt} AND NOT X11_${val}_FOUND)
        message(FATAL_ERROR "${opt} enabled but not found")
    elseif(${opt})
        register_library("${X11_${val}_LIB}" "${X11_${val}_INCLUDE_PATH}")
    endif()
endfunction()

# Common Options
option(ENABLE_SHARED "Shared library (default: ON)" ON)
option(ENABLE_STATIC "Static library (default: OFF)" OFF)
option(ENABLE_DEMO "Demo applications (default: OFF)" OFF)
option(ENABLE_TEST "Testing (default: OFF)" OFF)

if (WIN32 OR WIN64)
    set(UIOHOOK_SOURCE_DIR "windows")
elseif (APPLE)
    set(UIOHOOK_SOURCE_DIR "darwin")
else()
    set(UIOHOOK_SOURCE_DIR "x11")
endif()

add_library(uiohook OBJECT
    "./src/logger.c"
    "./src/${UIOHOOK_SOURCE_DIR}/input_helper.c"
    "./src/${UIOHOOK_SOURCE_DIR}/input_hook.c"
    "./src/${UIOHOOK_SOURCE_DIR}/post_event.c"
    "./src/${UIOHOOK_SOURCE_DIR}/system_properties.c"
)
set_property(TARGET uiohook PROPERTY POSITION_INDEPENDENT_CODE 1)
set_property(TARGET uiohook PROPERTY C_STANDARD 99)
set_property(TARGET uiohook PROPERTY C_STANDARD_REQUIRED ON)

target_include_directories(uiohook
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/${UIOHOOK_SOURCE_DIR}
)

include(GNUInstallDirs)

if (ENABLE_SHARED)
    add_library(shared SHARED $<TARGET_OBJECTS:uiohook>)
    target_link_libraries(shared "${LIBS}")
    set_target_properties(shared PROPERTIES OUTPUT_NAME "uiohook")

    install(TARGETS shared DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

if (ENABLE_STATIC)
    add_library(static STATIC $<TARGET_OBJECTS:uiohook>)
    target_link_libraries(static "${LIBS}")
    set_target_properties(static PROPERTIES OUTPUT_NAME "uiohook")
    install(TARGETS static DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

if (ENABLE_SHARED OR ENABLE_STATIC)
    install(FILES ./include/uiohook.h DESTINATION include)
endif()

if (UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(uiohook "${CMAKE_THREAD_LIBS_INIT}")
endif()

if (ENABLE_DEMO)
    if (ENABLE_SHARED)
        set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
        set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
    endif()

    register_demo("demo_hook")
    register_demo("demo_hook_async")
    register_demo("demo_post")
    register_demo("demo_properties")

    add_custom_target("all_demos" DEPENDS
        "demo_hook"
        "demo_hook_async"
        "demo_post"
        "demo_properties"
    )
    set_property(TARGET all_demos PROPERTY C_STANDARD 99)
    set_property(TARGET all_demos PROPERTY C_STANDARD_REQUIRED ON)

    install(TARGETS demo_hook demo_hook_async demo_post demo_properties DESTINATION bin)
endif()

if (ENABLE_TEST)
    add_executable(uiohook_tests
        "./test/input_helper_test.c"
        "./test/system_properties_test.c"
        "./test/minunit.h"
        "./test/uiohook_test.c"
    )
    add_dependencies(uiohook_tests "uiohook")
    target_include_directories(uiohook_tests PRIVATE
    "./src/${UIOHOOK_SOURCE_DIR}")
    target_link_libraries(uiohook_tests "uiohook" "${CMAKE_THREAD_LIBS_INIT}")
endif()

if (UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)

    pkg_check_modules(X11 REQUIRED x11)
    register_library("${X11_LIBRARIES}" "${X11_INCLUDE_DIRS}")
    list(APPEND PKG_CONFIG_LIBS "${X11_LDFLAGS}")

    pkg_check_modules(XTST REQUIRED xtst)
    register_library("${XTST_LIBRARIES}" "${XTST_INCLUDE_DIRS}")
    list(APPEND PKG_CONFIG_LIBS "${XTST_LDFLAGS}")

    include(CheckLibraryExists)
    check_library_exists(Xtst XRecordQueryVersion "" HAVE_XRECORD)

    include(CheckIncludeFile)
    check_include_file(X11/extensions/record.h HAVE_RECORD_H "-include X11/Xlib.h")

    option(USE_XKB_COMMON "X Keyboard Common Extension (default: ON)" ON)
    if (USE_XKBCOMMON)
        pkg_check_modules(XKB_COMMON REQUIRED xkbcommon-x11)
        register_library("${XKB_COMMON_LIBRARIES}" "${XKB_COMMON_INCLUDE_DIRS}")
        list(APPEND PKG_CONFIG_LIBS "${XKB_COMMON_LDFLAGS}")
        add_definitions("-DUSE_XKB_COMMON")
    endif()

    option(USE_XKB_FILE "X Keyboard File Extension (default: ON)" ON)
    if (USE_XKB_FILE)
        pkg_check_modules(XKB_FILE REQUIRED xkbfile)
        register_library("${XKB_FILE_LIBRARIES}" "${XKB_FILE_INCLUDE_DIRS}")
        list(APPEND PKG_CONFIG_LIBS "${XKB_FILE_LDFLAGS}")
        add_definitions("-DUSE_XKB_FILE")
    endif()

    option(USE_XT "X Toolkit Extension (default: ON)" ON)
    if (USE_XT)
        pkg_check_modules(XT REQUIRED xt)
        register_library("${XT_LIBRARIES}" "${XT_INCLUDE_DIRS}")
        list(APPEND PKG_CONFIG_LIBS "${XT_LDFLAGS}")
        add_definitions("-DUSE_XT")
    endif()


    option(USE_XF86MISC "XFree86-Misc X Extension (default: OFF)" OFF)
    if (USE_XF86MISC)
        pkg_check_modules(XF86MISC REQUIRED Xxf86misc)
        register_library("${XF86MISC_LIBRARIES}" "${XF86MISC_INCLUDE_DIRS}")
        list(APPEND PKG_CONFIG_LIBS "${XF86MISC_LDFLAGS}")
        add_definitions("-DUSE_XF86MISC")
    endif()

    option(USE_XRANDR "XRandR Extension (default: OFF)" OFF)
    if (USE_XRANDR)
        pkg_check_modules(XRANDR REQUIRED xrandr)
        register_library("${XRANDR_LIBRARIES}" "${XRANDR_INCLUDE_DIRS}")
        list(APPEND PKG_CONFIG_LIBS "${XRANDR_LDFLAGS}")
        add_definitions("-DUSE_XRANDR")
    endif()

    option(USE_XINERAMA "Xinerama Extension (default: ON)" ON)
    if (USE_XINERAMA)
        pkg_check_modules(XINERAMA REQUIRED xinerama)
        register_library("${XINERAMA_LIBRARIES}" "${XINERAMA_INCLUDE_DIRS}")
        list(APPEND PKG_CONFIG_LIBS "${XINERAMA_LDFLAGS}")
        add_definitions("-DUSE_XINERAMA")
    endif()

    option(USE_XRECORD_ASYNC "XRecord Asynchronous API (default: OFF)" OFF)
    if (USE_XRECORD_ASYNC)
        add_definitions("-DUSE_XRECORD_ASYNC")
    endif()

    option(USE_XTEST "XTest API (default: ON)" ON)
    if (USE_XTEST)
        # XTest API is provided by Xtst
        add_definitions("-DUSE_XTEST")
    endif()

    if (LINUX)
        option(USE_EVDEV "Generic Linux input driver (default: enabled)" ON)
        if (USE_EVDEV)
            add_definitions("-DUSE_EVDEV")
        endif()
    endif()

    if (ENABLE_TEST)
        target_link_libraries(uiohook_tests ${X11_LIBRARIES})
        target_include_directories(uiohook_tests PRIVATE ${X11_INCLUDE_DIRS})
    endif()
elseif (APPLE)
    set(CMAKE_MACOSX_RPATH 1)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.5")

    macro_option(USE_APPLICATION_SERVICES "ApplicationServices framework (default: ON)" ON)
    macro_option(USE_IOKIT "IOKit framework (default: ON)" ON)
    macro_option(USE_OBJC "Objective-C API (default: ON)" ON)

    macro_option(USE_CARBON_LEGACY "Legacy Carbon framework functionality (default: OFF)" OFF)
    #macro_option(USE_WEAK_IMPORT "Weakly-linked symbols (default: OFF)" OFF)

    find_library(CARBON Carbon)
    register_library(${CARBON} ${CARBON})
    set(COMPILE_LIBRARIES "-framework Carbon")


    if (USE_APPLICATION_SERVICES)
        find_library(APPLICATION_SERVICES ApplicationServices)
        if (NOT APPLICATION_SERVICES)
            message(FATAL_ERROR "ApplicationServices enabled but not found")
        endif()

        register_library(${APPLICATION_SERVICES} ${APPLICATION_SERVICES})
        set(COMPILE_LIBRARIES "-framework ApplicationServices")
    endif()

    if (USE_IOKIT)
        find_library(IOKIT IOKit)
        if (NOT IOKIT)
            message(FATAL_ERROR "IOKit enabled but not found")
        endif()

        register_library(${IOKIT} ${IOKIT})
        set(COMPILE_LIBRARIES "-framework IOKit")
    endif()

    if (USE_OBJC)
        find_library(OBJC objc)
        if (NOT OBJC)
            message(FATAL_ERROR "ObjC enabled but not found")
        endif()

        register_library(${OBJC} ${OBJC})
        set(COMPILE_LIBRARIES "-lobjc")
    endif()

    if (USE_CARBON_LEGACY)
        message(DEPRECATION "Legacy Carbon functionality has been deprecated.")

        if (USE_CARBON_LEGACY AND CMAKE_SIZEOF_VOID_P EQUAL 8)
            message(WARNING "Legacy Carbon functionality should not be used with 64-bit targets.")
        endif()
    endif()
elseif (WIN32)
    find_library(ADVAPI32 Advapi32)
    register_library(${ADVAPI32} ${ADVAPI32})
endif()

if (UNIX)
    if (ENABLE_SHARED OR ENABLE_STATIC)
        list(REMOVE_DUPLICATES PKG_CONFIG_LIBS)
        string(REPLACE ";" " " COMPILE_LIBRARIES "${PKG_CONFIG_LIBS}")

        configure_file("pc/uiohook.pc.in" "${PROJECT_BINARY_DIR}/uiohook.pc" @ONLY)
        install(FILES "${PROJECT_BINARY_DIR}/uiohook.pc" DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
    endif()
endif()

include_directories("./include")

target_include_directories("uiohook"
    PUBLIC "./src"
    INTERFACE "./include")

set_target_properties("uiohook" PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
if(MSVC AND MSVC_VERSION LESS "1900")
    set_target_properties("uiohook" PROPERTIES COMPILE_FLAGS
        "-Dinline=__inline -D_CRT_SECURE_NO_WARNINGS -Dsnprintf=_snprintf")
else()
    set_target_properties("uiohook" PROPERTIES COMPILE_FLAGS
        "-Dinline=__inline -D_CRT_SECURE_NO_WARNINGS")
endif()
