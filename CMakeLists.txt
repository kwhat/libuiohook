cmake_minimum_required(VERSION 3.16)

project(libuiohook VERSION 1.1.0 LANGUAGES C)

# Common Options
option(ENABLE_SHARED "Enable debug output (default: ON)" ON)
option(ENABLE_STATIC "Enable debug output (default: OFF)" OFF)

option(ENABLE_DEBUG "Enable debug output (default: OFF)" OFF)
option(ENABLE_DEMO "Enable demo applicaitons (default: OFF)" OFF)
option(ENABLE_TEST "Enable unit testing (default: OFF)" OFF)
option(ENABLE_QUIET "Enable copyright suppression (default: OFF)" OFF)

add_library(uiohook OBJECT
	"${CMAKE_CURRENT_SOURCE_DIR}/src/logger.c"
)
set_property(TARGET uiohook PROPERTY POSITION_INDEPENDENT_CODE 1)

if(WIN32 OR WIN64)
	set("windows")
elseif(APPLE)
	set(CMAKE_MACOSX_RPATH 1)
	#set(MAC_OS_X_VERSION_MIN_REQUIRED 1050)

	option(USE_COREFOUNDATION "Disable CoreFoundation framework (default: ON)" ON)
	option(USE_IOKIT "Disable IOKit framework (default: ON)" ON)
	option(USE_OBJC "Disable Objective-C API (default: ON)" ON)

	option(USE_CARBON_LEGACY "Enable legacy Carbon framework functionality (default: OFF)" OFF)
	option(USE_WEAK_IMPORT "Enable weakly-linked symbols (default: OFF)" OFF)

	set(LIBS "-framework Carbon")

	if (USE_COREFOUNDATION)
		set(LIBS "${LIBS} -framework ApplicationServices")
    endif()

    if (USE_IOKIT)
		set(LIBS "${LIBS} -framework IOKit")
	endif()

	if (USE_OBJC)
		set(LIBS "${LIBS} -lobjc")
	endif()

    if (USE_CARBON_LEGACY)
    	message(DEPRECATION "Legacy Carbon functionality has been deprecated.")

    	if (USE_CARBON_LEGACY AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    		message(WARNING "Legacy Carbon functionality should not be used with 64-bit targets.")
    	endif()
	endif()

	set(UIOHOOK_TARGET_DIR "darwin")
else()
	if(LINUX)
        option(USE_EVDEV "Disable generic Linux input driver (default: ON)" ON)
	endif()

	option(USE_XCB "Disable X protocol C-language Binding (default: ON)" ON)
	option(USE_XKB "Disable X Keyboard Extension (default: ON)" ON)
	option(USE_XKB_COMMON "Disable X Keyboard Common Extension (default: ON)" ON)
	option(USE_XKB_FILE "Disable X Keyboard File Extension (default: ON)" ON)

	option(USE_XT "Disable X Toolkit Intrinsics (default: ON)" ON)
	option(USE_XTEST "Disable XTest Extension (default: ON)" ON)
	option(USE_XINERAMA "Disable Xinerama Extension (default: ON)" ON)

	option(USE_XF86MISC "Enable XFree86-Misc X Extension (default: OFF)" OFF)
	option(USE_XRANDER "Enable XRandR Extension (default: OFF)" OFF)
	option(USE_XRECORD_ASYNC "Enable XRecord Asynchronous API (default: OFF)" OFF)

	find_package(PkgConfig REQUIRED)

	pkg_check_modules(PKG_X11 REQUIRED IMPORTED_TARGET x11)
	#target_link_libraries(uiohook ${PC_X11_LIBRARIES})
	#target_include_directories(uiohook PUBLIC ${PC_X11_INCLUDE_DIRS})
	#target_compile_options(uiohook PUBLIC ${PC_X11_CFLAGS_OTHER})
	set(REQUIRE "${PKG_X11_MODULE_NAME}")

	pkg_check_modules(PKG_XTST REQUIRED IMPORTED_TARGET xtst)
	#target_link_libraries(uiohook ${PC_XTST_LIBRARIES})
	#target_include_directories(uiohook PUBLIC ${PC_XTST_INCLUDE_DIRS})
	#target_compile_options(uiohook PUBLIC ${PC_XTST_CFLAGS_OTHER})
	set(REQUIRE "${REQUIRE} ${PKG_XTST_MODULE_NAME}")

	include(CheckLibraryExists)
	check_library_exists(Xtst XRecordQueryVersion "" HAVE_XRECORD)

	include(CheckIncludeFile)
	check_include_file(X11/extensions/record.h HAVE_RECORD_H "-include X11/Xlib.h")

	if (USE_XCB)
        #pkg_check_modules(PC_XCB REQUIRED IMPORTED_TARGET x11-xcb)
        #target_link_libraries(uiohook PkgConfig::PC_XCB_LIBRARIES)
		#target_include_directories(uiohook PUBLIC ${PC_XCB_INCLUDE_DIRS})
		#target_compile_options(uiohook PUBLIC ${PC_XCB_CFLAGS_OTHER})
        #set(REQUIRE "${REQUIRE} ${PC_XCB_MODULE_NAME}")
    endif()

	if (USE_XKB)
        #pkg_check_modules(PC_XKB REQUIRED IMPORTED_TARGET xcb-xkb)
        #target_link_libraries(uiohook PkgConfig::PC_XCB_LIBRARIES)
		#target_include_directories(uiohook PUBLIC ${PC_XCB_INCLUDE_DIRS})
		#target_compile_options(uiohook PUBLIC ${PC_XCB_CFLAGS_OTHER})
        #set(REQUIRE "${REQUIRE} ${PC_XKB_MODULE_NAME}")
    endif()

	if (USE_XKB_COMMON)
        #pkg_check_modules(PC_XKB_COMMON REQUIRED IMPORTED_TARGET xkbcommon)
		#target_link_libraries(uiohook PkgConfig::PC_XKB_COMMON_LIBRARIES)
		#target_include_directories(uiohook PUBLIC ${PC_XKB_COMMON_INCLUDE_DIRS})
		#target_compile_options(uiohook PUBLIC ${PC_XKB_COMMON_CFLAGS_OTHER})
        #set(REQUIRE "${REQUIRE} ${PC_XKB_COMMON_MODULE_NAME}")

        #pkg_check_modules(PC_XKB_COMMON_X11 REQUIRED IMPORTED_TARGET xkbcommon-x11)
		#target_link_libraries(uiohook PkgConfig::PC_XKB_COMMON_X11_LIBRARIES)
		#target_include_directories(uiohook PUBLIC ${PC_XKB_COMMON_X11_INCLUDE_DIRS})
		#target_compile_options(uiohook PUBLIC ${PC_XKB_COMMON_X11_CFLAGS_OTHER})
		#set(REQUIRE "${REQUIRE} ${PC_XKB_COMMON_X11_MODULE_NAME}")
    endif()

	if (USE_XKB_FILE)
		#pkg_check_modules(PC_XKB_FILE REQUIRED IMPORTED_TARGET xkbfile)
		#target_link_libraries(uiohook PkgConfig::PC_XKB_FILE_LIBRARIES)
		#target_include_directories(uiohook PUBLIC ${PC_XKB_FILE_X11_INCLUDE_DIRS})
		#target_compile_options(uiohook PUBLIC ${PC_XKB_FILE_CFLAGS_OTHER})
		#set(REQUIRE "${REQUIRE} ${PC_XKB_FILE_MODULE_NAME}")
	endif()

	if (USE_XT)
        #pkg_check_modules(PC_XT REQUIRED IMPORTED_TARGET xt)
		#target_link_libraries(uiohook PkgConfig::PC_XT_LIBRARIES)
		#target_include_directories(uiohook PUBLIC ${PC_XT_INCLUDE_DIRS})
		#target_compile_options(uiohook PUBLIC ${PC_XT_CFLAGS_OTHER})
        #set(REQUIRE "${REQUIRE} ${PC_XT_MODULE_NAME}")
    endif()

	if (USE_XTEST)
        #pkg_check_modules(PC_XTEST REQUIRED IMPORTED_TARGET xtst)
		#target_link_libraries(uiohook PkgConfig::PC_XTEST_LIBRARIES)
		#target_include_directories(uiohook PUBLIC ${PC_XTEST_INCLUDE_DIRS})
		#target_compile_options(uiohook PUBLIC ${PC_XTEST_CFLAGS_OTHER})
		#set(REQUIRE "${REQUIRE} ${PC_XTEST_MODULE_NAME}")
    endif()

	if (USE_XINERAMA)
		#pkg_check_modules(PC_XINERAMA REQUIRED IMPORTED_TARGET xinerama)
		#target_link_libraries(uiohook PkgConfig::PC_XINERAMA_LIBRARIES)
		#target_include_directories(uiohook PUBLIC ${PC_XINERAMA_INCLUDE_DIRS})
		#target_compile_options(uiohook PUBLIC ${PC_XINERAMA_CFLAGS_OTHER})
		#set(REQUIRE "${REQUIRE} ${PC_XINERAMA_MODULE_NAME}")
	endif()

	if (USE_XF86MISC)
        #pkg_check_modules(PC_XF86MISC REQUIRED IMPORTED_TARGET xf86miscproto)
		#target_link_libraries(uiohook PkgConfig::PC_XF86MISC_LIBRARIES)
		#target_include_directories(uiohook PUBLIC ${PC_XF86MISC_INCLUDE_DIRS})
		#target_compile_options(uiohook PUBLIC ${PC_XF86MISC_CFLAGS_OTHER})
        #set(REQUIRE "${REQUIRE} ${PC_XF86MISC_MODULE_NAME}")
    endif()

	#find_path(XRecord_H X11/extensions/record.h HINTS ${PC_X11_INCLUDEDIR} ${PC_XTST_INCLUDEDIR})

	set(UIOHOOK_TARGET_DIR "x11")
endif()


configure_file(
	"${PROJECT_SOURCE_DIR}/include/config.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/include/config.h"
)

configure_file(
	"${PROJECT_SOURCE_DIR}/pc/uiohook.pc.in"
	"${CMAKE_CURRENT_BINARY_DIR}/pc/uiohook.pc"
	@ONLY
)

#include_directories("${CMAKE_CURRENT_BINARY_DIR}/include")
add_definitions("-DHAVE_CONFIG_H")

# Library
target_sources(uiohook PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}/src/${UIOHOOK_TARGET_DIR}/input_helper.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/${UIOHOOK_TARGET_DIR}/input_helper.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/${UIOHOOK_TARGET_DIR}/input_hook.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/${UIOHOOK_TARGET_DIR}/post_event.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/${UIOHOOK_TARGET_DIR}/system_properties.c"
)

target_include_directories(uiohook
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(uiohook PRIVATE c_std_99)


if (ENABLE_SHARED)
	add_library(shared SHARED $<TARGET_OBJECTS:uiohook>)
	target_link_libraries(shared "${LIBS}")
	set_target_properties(shared PROPERTIES OUTPUT_NAME ${CMAKE_PROJECT_NAME})
endif()

if (ENABLE_STATIC)
	add_library(static STATIC $<TARGET_OBJECTS:uiohook>)
	target_link_libraries(static "${LIBS}")
	set_target_properties(static PROPERTIES OUTPUT_NAME ${CMAKE_PROJECT_NAME})
endif()

if(WIN32 OR WIN64)
	if("$MSVC_VERSION" VERSION_LESS "1900")
		SET_TARGET_PROPERTIES(uiohook  PROPERTIES COMPILE_FLAGS "-Dinline=__inline -D_CRT_SECURE_NO_WARNINGS -Dsnprintf=_snprintf")
	else()
		SET_TARGET_PROPERTIES(uiohook  PROPERTIES COMPILE_FLAGS "-Dinline=__inline -D_CRT_SECURE_NO_WARNINGS")
	endif()
endif()

# Generate demo program files
function (register_demo dname)
	add_executable("${dname}" "${CMAKE_CURRENT_SOURCE_DIR}/src/${dname}.c")
	add_dependencies("${dname}" uiohook)
	target_link_libraries("${dname}" uiohook)
	if(WIN32 OR WIN64)
		if("$MSVC_VERSION" VERSION_LESS "1900")
			SET_TARGET_PROPERTIES("${dname}"  PROPERTIES COMPILE_FLAGS "-Dinline=__inline -D_CRT_SECURE_NO_WARNINGS -Dsnprintf=_snprintf")
		else()
			SET_TARGET_PROPERTIES("${dname}"  PROPERTIES COMPILE_FLAGS "-Dinline=__inline -D_CRT_SECURE_NO_WARNINGS")
		endif()
	endif()
endfunction()

if (ENABLE_DEMO)
	# demo_hook
	register_demo("demo_hook")

	# demo_hook_async
	register_demo("demo_hook_async")

	#demo_post
	register_demo("demo_post")

	# demo_properties
	register_demo("demo_properties")

	# all demo
	add_custom_target(
		"all_demo" DEPENDS
			 "demo_hook"
			 "demo_hook_async"
			 "demo_post"
			 "demo_properties"
	)
endif()
