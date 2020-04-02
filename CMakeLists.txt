cmake_minimum_required(VERSION 3.10)

project(libuiohook)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

# Gnerates a demo project
function (register_demo dname)
    add_executable("${dname}" "./demos/${dname}.c")
    add_dependencies("${dname}" "uiohook")
    target_link_libraries("${dname}" "uiohook")

    if(MSVC AND MSVC_VERSION LESS "1900")
        SET_TARGET_PROPERTIES("${dname}" PROPERTIES COMPILE_FLAGS
            "-Dinline=__inline -D_CRT_SECURE_NO_WARNINGS -Dsnprintf=_snprintf")
    else()
        SET_TARGET_PROPERTIES("${dname}" PROPERTIES COMPILE_FLAGS
            "-Dinline=__inline -D_CRT_SECURE_NO_WARNINGS")
    endif()
endfunction()

# Creates a CMake option which will be set as macros if enabled
function (macro_option opt desc default)
    option(${opt} ${desc} ${default})
    if (${opt})
        add_definitions("-D${opt}")
    endif()
endfunction()

# Creates a CMake option for X11 libraries 
function (x11_option opt val desc default)
    macro_option(${opt} ${desc} ${default})

    if (${opt} AND NOT X11_${val}_FOUND)
        message(FATAL_ERROR "${opt} enabled but not found")
    elseif(${opt})
        target_link_libraries(uiohook ${X11_${val}_LIB})
        target_include_directories(uiohook PUBLIC ${X11_${val}_INCLUDE_PATH})
    endif()
endfunction()

# Common Options
option(ENABLE_DEBUG "Enable debug output (default: disabled)" OFF)
option(ENABLE_DEMOS "Enable demo applicaitons (default: disabled)" OFF)
option(ENABLE_TESTS "Enable unit testing (default: disabled)" OFF)
option(ENABLE_QUIET "Enable copyright suppression (default: disabled)" OFF)

if (ENABLE_QUIET)
    add_definitions("-DUSE_QUIET")
endif()

if (ENABLE_DEBUG)
    add_definitions("-DUSE_DEBUG")
endif()

if (WIN32 OR WIN64)
    set(UIOHOOK_SOURCE_DIR "windows")
elseif (APPLE)
    set(UIOHOOK_SOURCE_DIR "darwin")
else()
    set(UIOHOOK_SOURCE_DIR "x11")
endif()

set(UIOHOOK_SRC
    "./include/uiohook.h"
    "./src/logger.c"
    "./src/logger.h"
    "./src/${UIOHOOK_SOURCE_DIR}/input_helper.h"
    "./src/${UIOHOOK_SOURCE_DIR}/input_helper.c"
    "./src/${UIOHOOK_SOURCE_DIR}/input_hook.c"
    "./src/${UIOHOOK_SOURCE_DIR}/post_event.c"
    "./src/${UIOHOOK_SOURCE_DIR}/system_properties.c")

add_library("uiohook" SHARED ${UIOHOOK_SRC})

if (ENABLE_DEMOS)
    register_demo("demo_hook")
    register_demo("demo_hook_async")
    register_demo("demo_post")
    register_demo("demo_properties")

    add_custom_target("all_demos" DEPENDS
        "demo_hook"
        "demo_hook_async"
        "demo_post"
        "demo_properties")
endif()

if (ENABLE_TESTS)
    add_executable(uiohook_tests
        "./test/input_helper_test.c"
	"./test/system_properties_test.c"
	"./test/minunit.h"
	"./test/uiohook_test.c")
    add_dependencies(uiohook_tests "uiohook")
    target_include_directories(uiohook_tests PRIVATE
	"./src/${UIOHOOK_SOURCE_DIR}")
    target_link_libraries(uiohook_tests "uiohook")
endif()

if (UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(uiohook ${THREADS_LIBRARIES})
endif()

if (UNIX AND NOT APPLE)
    if(LINUX)
        macro_option(USE_EVDEV "Disable generic Linux input driver (default: enabled)" ON)
    endif()

    find_package(X11)

    x11_option(USE_XKB Xkb "Disable X Keyboard Extension (default: ON)" ON)
    x11_option(USE_XKB_FILE xkbfile "Disable X Keyboard File Extension (default: enabled)" ON)

    x11_option(USE_XT Xt "Disable X Toolkit Intrinsics (default: enabled)" ON)
    x11_option(USE_XTEST Xtst "Disable XTest Extension (default: enabled)" ON)
    x11_option(USE_XINERAMA Xinerama "Disable Xinerama Extension (default: enabled)" ON)

    x11_option(USE_XF86MISC Xxf86misc "Enable XFree86-Misc X Extension (default: disabled)" OFF)
    x11_option(USE_XRANDR Xrandr "Enable XRandR Extension (default: disabled)" OFF)

    macro_option(USE_XRECORD_ASYNC "Enable XRecord Asynchronous API (default: disabled)" OFF)
    macro_option(USE_XCB "Disable X Keyboard Extension (default: enabled)" ON)
    macro_option(USE_XKBCOMMON "Disable X Keyboard Common Extension (default: enabled)" ON)

    include(CheckLibraryExists)
    check_library_exists(Xtst XRecordQueryVersion "" HAVE_XRECORD)

    include(CheckIncludeFile)
    check_include_file(X11/extensions/record.h HAVE_RECORD_H "-include X11/Xlib.h")
    
    if (ENABLE_TESTS)
        target_link_libraries(uiohook_tests ${X11_LIBRARIES})
        target_include_directories(uiohook_tests PRIVATE ${X11_INCLUDE_DIRS})
    endif()

    if (USE_XCB)
        find_package(XCB REQUIRED)
        find_package(X11_XCB REQUIRED)
        if (NOT XCB_FOUND)
            message(FATAL_ERROR "XCB enabled but not found")
        endif()

        target_link_libraries(uiohook ${XCB_LIBRARIES})
        target_include_directories(uiohook PRIVATE ${XCB_INCLUDE_DIRS})
        target_link_libraries(uiohook ${X11_XCB_LIBRARIES})
        target_include_directories(uiohook PRIVATE ${X11_XCB_INCLUDE_DIRS})
        add_definitions(${X11_XCB_DEFINITIONS})
    endif()

    if (USE_XKBCOMMON)
        find_package(XKBCommon)
        find_package(XKBCommonX11)
        if (NOT XKBCOMMON_FOUND OR NOT XKBCOMMONX11_FOUND)
            message(FATAL_ERROR "XKBCommon enabled but not found")
        endif()

        target_link_libraries(uiohook ${XKBCOMMON_X11_LIBRARIES})
        target_include_directories(uiohook PRIVATE ${XKBCOMMON_X11_INCLUDE_DIRS})
        target_compile_options(uiohook PUBLIC ${XKBCOMMON_X11_DEFINITIONS})

        target_link_libraries(uiohook ${XKBCOMMON_LIBRARIES})
        target_include_directories(uiohook PRIVATE ${XKBCOMMON_INCLUDE_DIRS})
        target_compile_options(uiohook PUBLIC ${XKBCOMMON_DEFINITIONS})
    endif()
elseif (APPLE)
    set(CMAKE_MACOSX_RPATH 1)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.8")

    macro_option(USE_COREFOUNDATION "Disable CoreFoundation framework (default: ON)" ON)
    macro_option(USE_IOKIT "Disable IOKit framework (default: ON)" ON)
    macro_option(USE_OBJC "Disable Objective-C API (default: ON)" ON)

    macro_option(USE_CARBON_LEGACY "Enable legacy Carbon framework functionality (default: OFF)" OFF)
    macro_option(USE_WEAK_IMPORT "Enable weakly-linked symbols (default: OFF)" OFF)

    find_library(CARBON Carbon)
    target_link_libraries(uiohook ${CARBON})
    target_include_directories(uiohook PRIVATE ${CARBON})

    if (USE_COREFOUNDATION)
        find_library(APPLICATIONSERVICES ApplicationServices)
        target_link_libraries(uiohook ${APPLICATIONSERVICES})
        target_include_directories(uiohook PRIVATE ${APPLICATIONSERVICES})
    endif()

    if (USE_IOKIT)
        find_library(IOKIT IOKit)
        target_link_libraries(uiohook ${IOKIT})
        target_include_directories(uiohook PRIVATE ${IOKIT})
    endif()

    if (USE_OBJC)
        find_library(OBJC objc)
        target_link_libraries(uiohook ${OBJC})
        target_include_directories(uiohook PRIVATE ${OBJC})
    endif()

    if (USE_CARBON_LEGACY)
    	message(DEPRECATION "Legacy Carbon functionality has been deprecated.")

    	if (USE_CARBON_LEGACY AND CMAKE_SIZEOF_VOID_P EQUAL 8)
            message(WARNING "Legacy Carbon functionality should not be used with 64-bit targets.")
    	endif()
    endif()
endif()

include_directories("./include")

target_include_directories("uiohook" PUBLIC
    "./include"
    "./src")

if(MSVC AND MSVC_VERSION LESS "1900")
    SET_TARGET_PROPERTIES("uiohook" PROPERTIES COMPILE_FLAGS
        "-Dinline=__inline -D_CRT_SECURE_NO_WARNINGS -Dsnprintf=_snprintf")
else()
    SET_TARGET_PROPERTIES("uiohook" PROPERTIES COMPILE_FLAGS
        "-Dinline=__inline -D_CRT_SECURE_NO_WARNINGS")
endif()

