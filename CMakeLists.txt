cmake_minimum_required(VERSION 3.10)

project(libuiohook)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

# Creates CMake options which will be set as macros if enabled
function (macro_option opt desc default)
    option(${opt} ${desc} ${default})
    if (${opt})
        add_definitions("-D${opt}")
    endif()
endfunction()


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
    set(UIOHOOK_SRC
        "./include/uiohook.h"
        "./src/logger.c"
        "./src/logger.h"
        "./src/windows/input_helper.h"
        "./src/windows/input_helper.c"
        "./src/windows/input_hook.c"
        "./src/windows/post_event.c"
        "./src/windows/system_properties.c")
elseif (APPLE)
    set(UIOHOOK_SRC
        "include/uiohook.h"
        "src/logger.c"
        "src/logger.h"
        "src/darwin/*.h"
        "src/darwin/*.c")
else()
    set(UIOHOOK_SRC
        "./include/uiohook.h"
        "./src/logger.c"
        "./src/logger.h"
        "./src/x11/input_helper.h"
        "./src/x11/input_helper.c"
        "./src/x11/input_hook.c"
        "./src/x11/post_event.c"
        "./src/x11/system_properties.c")
endif()

add_library("uiohook" SHARED ${UIOHOOK_SRC})

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

endif()

include_directories("./include")

target_include_directories("uiohook" PUBLIC
    "./include"
    "./src")

if(WIN32 OR WIN64)
    if("$MSVC_VERSION" VERSION_LESS "1900")
        SET_TARGET_PROPERTIES("uiohook" PROPERTIES COMPILE_FLAGS
            "-Dinline=__inline -D_CRT_SECURE_NO_WARNINGS -Dsnprintf=_snprintf")
    else()
        SET_TARGET_PROPERTIES("uiohook" PROPERTIES COMPILE_FLAGS
            "-Dinline=__inline -D_CRT_SECURE_NO_WARNINGS")
    endif()
endif()

# function to generate demo project
function (register_demo dname)
    add_executable("${dname}" "./demos/${dname}.c")
    add_dependencies("${dname}" "uiohook")
    target_link_libraries("${dname}" "uiohook")

    if(WIN32 OR WIN64)
        if("$MSVC_VERSION" VERSION_LESS "1900")
            SET_TARGET_PROPERTIES("${dname}" PROPERTIES COMPILE_FLAGS
                "-Dinline=__inline -D_CRT_SECURE_NO_WARNINGS -Dsnprintf=_snprintf")
        else()
            SET_TARGET_PROPERTIES("${dname}" PROPERTIES COMPILE_FLAGS
                "-Dinline=__inline -D_CRT_SECURE_NO_WARNINGS")
        endif()
    endif()
endfunction()

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

if (ENABLE_TEST)
    # TODO: tests
endif()
