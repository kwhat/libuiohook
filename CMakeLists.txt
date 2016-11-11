cmake_minimum_required(VERSION 2.8)

project(libuiohook)

# Common Options
option(ENABLE_DEBUG "Enable debug output (default: disabled)" OFF)
option(ENABLE_DEMO "Enable demo applicaitons (default: disabled)" OFF)
option(ENABLE_TEST "Enable unit testing (default: disabled)" OFF)
option(ENABLE_QUIET "Enable copyright suppression (default: disabled)" OFF)

# libUIOHook Sources.
if(WIN32 OR WIN64)
	set(UIOHOOK_SRC
		  "${CMAKE_CURRENT_SOURCE_DIR}/include/uiohook.h"
		  "${CMAKE_CURRENT_SOURCE_DIR}/src/logger.c"
		  "${CMAKE_CURRENT_SOURCE_DIR}/src/logger.h"
		  "${CMAKE_CURRENT_SOURCE_DIR}/src/windows/input_helper.h"
		  "${CMAKE_CURRENT_SOURCE_DIR}/src/windows/input_helper.c"
		  "${CMAKE_CURRENT_SOURCE_DIR}/src/windows/input_hook.c"
		  "${CMAKE_CURRENT_SOURCE_DIR}/src/windows/post_event.c"
		  "${CMAKE_CURRENT_SOURCE_DIR}/src/windows/system_properties.c"
		)
elseif(APPLE)
	set(UIOHOOK_SRC
		  "include/uiohook.h"
		  "src/logger.c"
		  "src/logger.h"
		  "src/darwin/*.h"
		  "src/darwin/*.c"
		)
else()

	option(ENABLE_XKB "Disable X Keyboard Extension (default: enabled)" ON)
	option(ENABLE_XKBCOMMON "Disable X Keyboard Common Extension (default: enabled)" ON)
	option(ENABLE_XKBFILE "Disable X Keyboard File Extension (default: enabled)" ON)

	option(ENABLE_XT "Disable X Toolkit Intrinsics (default: enabled)" ON)
	option(ENABLE_XTEST "Disable XTest Extension (default: enabled)" ON)
	option(ENABLE_XINERAMA "Disable Xinerama Extension (default: enabled)" ON)

	option(ENABLE_XF86MISC "Enable XFree86-Misc X Extension (default: disabled)" OFF)
	option(ENABLE_XRANDER "Enable XRandR Extension (default: disabled)" OFF)
	option(ENABLE_XRECORD-ASYNC "Enable XRecord Asynchronous API (default: disabled)" OFF)

	find_package(PkgConfig)
	pkg_check_modules(PC_X11 REQUIRED x11)
	pkg_check_modules(PC_XTST REQUIRED xtst)

	# find_library (<VAR> name1 [path1 path2 ...])


	set(UIOHOOK_SRC
		  "${CMAKE_CURRENT_SOURCE_DIR}/include/uiohook.h"
		  "${CMAKE_CURRENT_SOURCE_DIR}/src/logger.c"
		  "${CMAKE_CURRENT_SOURCE_DIR}/src/logger.h"
		  "${CMAKE_CURRENT_SOURCE_DIR}/src/x11/input_helper.h"
		  "${CMAKE_CURRENT_SOURCE_DIR}/src/x11/input_helper.c"
		  "${CMAKE_CURRENT_SOURCE_DIR}/src/x11/input_hook.c"
		  "${CMAKE_CURRENT_SOURCE_DIR}/src/x11/post_event.c"
		  "${CMAKE_CURRENT_SOURCE_DIR}/src/x11/system_properties.c"
		)

endif()

##configure_file("${PROJECT_SOURCE_DIR}/include/config.h.in"
##		"${PROJECT_BINARY_DIR}/include/config.h")

include_directories("${PROJECT_BINARY_DIR}/include")


# Library
add_library("uiohook" SHARED ${UIOHOOK_SRC})
target_include_directories("uiohook" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/src")
if(WIN32 OR WIN64)
	if("$MSVC_VERSION" VERSION_LESS "1900")
		SET_TARGET_PROPERTIES("uiohook"  PROPERTIES COMPILE_FLAGS "-Dinline=__inline -D_CRT_SECURE_NO_WARNINGS -Dsnprintf=_snprintf")
	else()
			SET_TARGET_PROPERTIES("uiohook"  PROPERTIES COMPILE_FLAGS "-Dinline=__inline -D_CRT_SECURE_NO_WARNINGS")
	endif()
endif()

# function to generate demo project
function (register_demo dname)
	add_executable("${dname}" "${CMAKE_CURRENT_SOURCE_DIR}/src/${dname}.c")
	add_dependencies("${dname}" "uiohook")
	target_link_libraries("${dname}"  "uiohook")
	if(WIN32 OR WIN64)
		if("$MSVC_VERSION" VERSION_LESS "1900")
			SET_TARGET_PROPERTIES("${dname}"  PROPERTIES COMPILE_FLAGS "-Dinline=__inline -D_CRT_SECURE_NO_WARNINGS -Dsnprintf=_snprintf")
		else()
				SET_TARGET_PROPERTIES("${dname}"  PROPERTIES COMPILE_FLAGS "-Dinline=__inline -D_CRT_SECURE_NO_WARNINGS")
		endif()
	endif()
endfunction()

# demo_hook
register_demo("demo_hook")

# demo_hook_async
register_demo("demo_hook_async")

#demo_post
register_demo("demo_post")

# demo_properties
register_demo("demo_properties")

# all demo
add_custom_target("all_demo" 
					DEPENDS
						 "demo_hook"
						 "demo_hook_async"
						 "demo_post"
						 "demo_properties"
		)
