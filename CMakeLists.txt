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
    if (UIOHOOK_ENABLE_SHARED)
        add_dependencies("${dname}" uiohook_shared)
        target_link_libraries("${dname}" uiohook_shared)
    elseif (UIOHOOK_ENABLE_STATIC)
        add_dependencies("${dname}" uiohook_static)
        target_link_libraries("${dname}" uiohook_static)
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

# Registers a dependency for the uiohook_static, uiohook_shared and normal library
function (register_library lib include)
    target_link_libraries(uiohook ${lib})
    target_include_directories(uiohook PRIVATE ${include})

    if (UIOHOOK_ENABLE_STATIC)
        target_link_libraries(uiohook_static ${lib})
        target_include_directories(uiohook_static PRIVATE ${include})
    endif()

    if (UIOHOOK_ENABLE_SHARED)
        target_link_libraries(uiohook_shared ${lib})
        target_include_directories(uiohook_shared PRIVATE ${include})
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
option(UIOHOOK_ENABLE_SHARED "Shared library (default: ON)" ON)
option(UIOHOOK_ENABLE_STATIC "Static library (default: OFF)" OFF)
option(UIOHOOK_ENABLE_DEMO "Demo applications (default: OFF)" OFF)
option(UIOHOOK_ENABLE_TEST "Testing (default: OFF)" OFF)
option(UIOHOOK_ENABLE_INSTALL "Register files for install target (default: ON)" ON)

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

if (UIOHOOK_ENABLE_INSTALL)
    include(GNUInstallDirs)
endif()

if (UIOHOOK_ENABLE_SHARED)
    add_library(uiohook_shared SHARED $<TARGET_OBJECTS:uiohook>)
    target_link_libraries(uiohook_shared "${LIBS}")
    set_target_properties(uiohook_shared PROPERTIES OUTPUT_NAME "uiohook")

    if (UIOHOOK_ENABLE_INSTALL)
        install(TARGETS uiohook_shared DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif()
endif()

if (UIOHOOK_ENABLE_STATIC)
    add_library(uiohook_static STATIC $<TARGET_OBJECTS:uiohook>)
    target_link_libraries(uiohook_static "${LIBS}")
    set_target_properties(uiohook_static PROPERTIES OUTPUT_NAME "uiohook")
    if (UIOHOOK_ENABLE_INSTALL)
        install(TARGETS uiohook_static DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif()
endif()

if (UIOHOOK_ENABLE_SHARED OR UIOHOOK_ENABLE_STATIC)
    if (UIOHOOK_ENABLE_INSTALL)
        install(FILES ./include/uiohook.h DESTINATION include)
    endif()
endif()

if (UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(uiohook "${CMAKE_THREAD_LIBS_INIT}")
endif()

if (UIOHOOK_ENABLE_DEMO)
    if (UIOHOOK_ENABLE_SHARED)
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
    
    if (UIOHOOK_ENABLE_INSTALL)
        install(TARGETS demo_hook demo_hook_async demo_post demo_properties DESTINATION bin)
    endif()
endif()

if (UIOHOOK_ENABLE_TEST)
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
    find_package(X11)
    register_library("${X11_LIBRARIES}" "${X11_INCLUDE_DIRS}")
    add_definitions(${X11_DEFINITIONS})
    set(COMPILE_LIBRARIES "-lX11")

    x11_option(USE_XKB Xkb "X Keyboard Extension (default: ON)" ON)
    x11_option(USE_XKB_FILE xkbfile "X Keyboard File Extension (default: ON)" ON)

    x11_option(USE_XT Xt "X Toolkit Intrinsics (default: ON)" ON)
    x11_option(USE_XTEST Xtst "XTest Extension (default: ON)" ON)
    x11_option(USE_XINERAMA Xinerama "Xinerama Extension (default: ON)" ON)

    x11_option(USE_XF86MISC Xxf86misc "XFree86-Misc X Extension (default: OFF)" OFF)
    x11_option(USE_XRANDR Xrandr "XRandR Extension (default: OFF)" OFF)

    macro_option(USE_XRECORD_ASYNC "XRecord Asynchronous API (default: OFF)" OFF)
    macro_option(USE_XCB "X Keyboard Extension (default: ON)" ON)
    macro_option(USE_XKBCOMMON "X Keyboard Common Extension (default: ON)" ON)

    if(LINUX)
        macro_option(USE_EVDEV "Generic Linux input driver (default: enabled)" ON)
    endif()


    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/module")

    if (USE_XCB)
        find_package(XCB REQUIRED)
        find_package(X11_XCB REQUIRED)

        register_library("${XCB_LIBRARIES}" "${XCB_INCLUDE_DIRS}")
        add_definitions(${XCB_DEFINITIONS})
        set(COMPILE_LIBRARIES "${COMPILE_LIBRARIES} -lxcb")

        register_library("${X11_XCB_LIBRARIES}" "${X11_XCB_INCLUDE_DIRS}")
        add_definitions(${X11_XCB_DEFINITIONS})
        set(COMPILE_LIBRARIES "${COMPILE_LIBRARIES} -lx11-xcb")
    endif()

    if (USE_XKBCOMMON)
        find_package(XKBCommon)
        find_package(XKBCommonX11)
        if (NOT XKBCOMMON_FOUND OR NOT XKBCOMMONX11_FOUND)
            message(FATAL_ERROR "XKBCommon enabled but not found")
        endif()

        register_library("${XKBCOMMON_LIBRARIES}" "${XKBCOMMON_INCLUDE_DIRS}")
        add_definitions(${XKBCOMMON_DEFINITIONS})
        set(COMPILE_LIBRARIES "${COMPILE_LIBRARIES} -lxkbcommon")

        register_library("${XKBCOMMON_X11_LIBRARIES}" "${XKBCOMMON_X11_INCLUDE_DIRS}")
        add_definitions(${XKBCOMMON_X11_DEFINITIONS})
        set(COMPILE_LIBRARIES "${COMPILE_LIBRARIES} -lxkbcommon-x11")
    endif()

    if (UIOHOOK_ENABLE_TEST)
        target_link_libraries(uiohook_tests ${X11_LIBRARIES})
        target_include_directories(uiohook_tests PRIVATE ${X11_INCLUDE_DIRS})
    endif()

    include(CheckLibraryExists)
    check_library_exists(Xtst XRecordQueryVersion "" HAVE_XRECORD)

    include(CheckIncludeFile)
    check_include_file(X11/extensions/record.h HAVE_RECORD_H "-include X11/Xlib.h")
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
endif()

if (UNIX)
    set(WTF_FU "testing")
    configure_file("pc/uiohook.pc.in" "${PROJECT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.pc" @ONLY)
    if (UIOHOOK_ENABLE_INSTALL)
        install(FILES "${PROJECT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.pc" DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
    endif()
endif()

include_directories("./include")

target_include_directories("uiohook" PUBLIC
    "./include"
    "./src")

set_target_properties("uiohook" PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
if(MSVC AND MSVC_VERSION LESS "1900")
    set_target_properties("uiohook" PROPERTIES COMPILE_FLAGS
        "-Dinline=__inline -D_CRT_SECURE_NO_WARNINGS -Dsnprintf=_snprintf")
else()
    set_target_properties("uiohook" PROPERTIES COMPILE_FLAGS
        "-Dinline=__inline -D_CRT_SECURE_NO_WARNINGS")
endif()

