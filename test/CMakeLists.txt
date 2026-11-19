# Unit targets compile production sources directly, without loading libuiohook's
# native startup code. Platform SDKs are needed only by tests using native types.
function(configure_unit_test target)
    set_target_properties(${target} PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
    target_include_directories(${target} PRIVATE
        "${PROJECT_SOURCE_DIR}/include"
        "${PROJECT_SOURCE_DIR}/src"
        "$<$<PLATFORM_ID:Darwin>:${PROJECT_SOURCE_DIR}/src/darwin>"
        "$<$<PLATFORM_ID:Windows>:${PROJECT_SOURCE_DIR}/src/windows>"
        "$<$<PLATFORM_ID:Linux>:${PROJECT_SOURCE_DIR}/src/evdev>"
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endfunction()

add_executable(header_test header_test.c)
configure_unit_test(header_test)
add_test(NAME common.header COMMAND header_test)
set_tests_properties(common.header PROPERTIES LABELS "unit;common;portable")

if(WIN32)
    add_executable(unicode_table_test
        windows/unicode_table_test.c windows/unicode_fixture.c
        "${PROJECT_SOURCE_DIR}/src/windows/unicode_table.c"
    )
    configure_unit_test(unicode_table_test)
    add_test(NAME windows.unicode_table COMMAND unicode_table_test)
    set_tests_properties(windows.unicode_table PROPERTIES LABELS "unit;windows;portable")
endif()

if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_executable(unicode_helper_test
        windows/unicode_helper_test.c windows/unicode_fixture.c
        "${PROJECT_SOURCE_DIR}/src/windows/unicode_helper.c"
        "${PROJECT_SOURCE_DIR}/src/windows/unicode_table.c"
        "${PROJECT_SOURCE_DIR}/src/logger.c"
    )
    configure_unit_test(unicode_helper_test)
    target_link_libraries(unicode_helper_test user32 advapi32)
    add_test(NAME windows.unicode_helper COMMAND unicode_helper_test)
    set_tests_properties(windows.unicode_helper PROPERTIES LABELS "unit;windows")
endif()

if(APPLE)
    find_package(Threads REQUIRED)
    add_executable(runloop_queue_test
        darwin/runloop_queue_test.c
        "${PROJECT_SOURCE_DIR}/src/darwin/runloop_queue.c"
    )
    configure_unit_test(runloop_queue_test)
    target_link_libraries(runloop_queue_test PRIVATE "${CMAKE_THREAD_LIBS_INIT}")
    add_test(NAME darwin.runloop_queue COMMAND runloop_queue_test)
    set_tests_properties(darwin.runloop_queue PROPERTIES LABELS "unit;darwin" TIMEOUT 20)

    add_executable(runloop_helper_test
        darwin/runloop_helper_test.c
        "${PROJECT_SOURCE_DIR}/src/darwin/runloop_helper.c"
        "${PROJECT_SOURCE_DIR}/src/darwin/runloop_queue.c"
    )
    configure_unit_test(runloop_helper_test)
    target_link_libraries(runloop_helper_test PRIVATE "${CMAKE_THREAD_LIBS_INIT}" "-framework CoreFoundation")
    add_test(NAME darwin.runloop_helper COMMAND runloop_helper_test)
    set_tests_properties(darwin.runloop_helper PROPERTIES LABELS "unit;darwin" TIMEOUT 20)
endif()

add_executable(logger_test logger_test.c "${PROJECT_SOURCE_DIR}/src/logger.c")
configure_unit_test(logger_test)
add_test(NAME common.logger COMMAND logger_test)
set_tests_properties(common.logger PROPERTIES LABELS "unit;common")

if(APPLE)
    add_executable(darwin_keycode_test
        darwin/keycode_helper_test.c
        "${PROJECT_SOURCE_DIR}/src/darwin/keycode_helper.c"
        "${PROJECT_SOURCE_DIR}/src/darwin/mouse_helper.c"
        "${PROJECT_SOURCE_DIR}/src/input_state.c"
    )
    configure_unit_test(darwin_keycode_test)
    add_test(NAME darwin.keycode_helper COMMAND darwin_keycode_test)
    set_tests_properties(darwin.keycode_helper PROPERTIES LABELS "unit;darwin")
endif()

if(WIN32)
    add_executable(windows_keycode_test
        windows/keycode_helper_test.c
        "${PROJECT_SOURCE_DIR}/src/windows/keycode_helper.c"
        "${PROJECT_SOURCE_DIR}/src/input_state.c"
        "${PROJECT_SOURCE_DIR}/src/logger.c"
    )
    configure_unit_test(windows_keycode_test)
    add_test(NAME windows.keycode_helper COMMAND windows_keycode_test)
    set_tests_properties(windows.keycode_helper PROPERTIES LABELS "unit;windows")
endif()

if(UNIX AND NOT APPLE)
    add_executable(uiohook_tests uiohook_test.c input_helper_test.c)
    configure_unit_test(uiohook_tests)

    # Existing function-pointer APIs provide the X11 mock boundary. Wayland
    # functions are supplied by the fixture object instead of a live compositor.
    add_library(evdev_test_backend STATIC
        "${PROJECT_SOURCE_DIR}/src/logger.c"
        "${PROJECT_SOURCE_DIR}/src/evdev/input_helper.c"
        "${PROJECT_SOURCE_DIR}/src/evdev/keycode_helper.c"
        "${PROJECT_SOURCE_DIR}/src/evdev/mouse_helper.c"
        "${PROJECT_SOURCE_DIR}/src/numeric_helper.c"
        "${PROJECT_SOURCE_DIR}/src/input_state.c"
        "${PROJECT_SOURCE_DIR}/src/evdev/dispatch_event.c"
        "${PROJECT_SOURCE_DIR}/src/evdev/system_properties.c"
        evdev/mock_desktop.c
    )
    configure_unit_test(evdev_test_backend)
    target_include_directories(evdev_test_backend PUBLIC
        ${LIBINPUT_INCLUDE_DIRS} ${EVDEV_INCLUDE_DIRS}
        ${XKB_INCLUDE_DIRS} ${XKB_X11_INCLUDE_DIRS} ${XCB_X11_INCLUDE_DIRS}
        ${X11_INCLUDE_DIRS} ${XINERAMA_INCLUDE_DIRS}
    )
    target_link_libraries(evdev_test_backend PUBLIC ${XKB_LDFLAGS} ${LIBINPUT_LDFLAGS} ${EVDEV_LDFLAGS})
    target_link_libraries(uiohook_tests evdev_test_backend)

    add_executable(post_event_test
        evdev/post_event_test.c
        "${PROJECT_SOURCE_DIR}/src/evdev/post_event.c"
    )
    configure_unit_test(post_event_test)
    target_include_directories(post_event_test PRIVATE ${EVDEV_INCLUDE_DIRS})
    target_link_libraries(post_event_test evdev_test_backend ${EVDEV_LDFLAGS})
    add_test(NAME evdev.post_event COMMAND post_event_test)
    set_tests_properties(evdev.post_event PROPERTIES LABELS "unit;evdev")

    add_executable(relative_mouse_test evdev/relative_mouse_test.c)
    configure_unit_test(relative_mouse_test)
    target_link_libraries(relative_mouse_test evdev_test_backend)
    add_test(NAME evdev.relative_mouse COMMAND relative_mouse_test)
    set_tests_properties(evdev.relative_mouse PROPERTIES LABELS "unit;evdev")

    add_executable(system_properties_test system_properties_test.c)
    configure_unit_test(system_properties_test)
    target_link_libraries(system_properties_test evdev_test_backend)
    foreach(property repeat pointer click_time screens)
        add_test(NAME evdev.properties.${property} COMMAND system_properties_test ${property})
        set_tests_properties(evdev.properties.${property} PROPERTIES LABELS "unit;evdev")
    endforeach()

    add_executable(input_activity_test
        evdev/input_activity_test.c
    )
    configure_unit_test(input_activity_test)
    target_link_libraries(input_activity_test evdev_test_backend)
    add_test(NAME evdev.input_activity COMMAND input_activity_test)
    set_tests_properties(evdev.input_activity PROPERTIES LABELS "unit;evdev")

    add_executable(libinput_backend_test
        evdev/libinput_backend_test.c
    )
    configure_unit_test(libinput_backend_test)
    target_include_directories(libinput_backend_test PRIVATE ${LIBINPUT_INCLUDE_DIRS} ${EVDEV_INCLUDE_DIRS})
    # libinput and uinput calls are provided by the test, not a live backend.
    target_link_libraries(libinput_backend_test evdev_test_backend ${EVDEV_LDFLAGS})
    # New scroll events are only used to check that legacy and new events are
    # not both dispatched. Keep the test buildable with older libinput headers.
    if(LIBINPUT_VERSION VERSION_GREATER_EQUAL "1.19")
        target_compile_definitions(libinput_backend_test PRIVATE HAVE_LIBINPUT_POINTER_SCROLL_FINGER)
    endif()
    add_test(NAME evdev.libinput_translation COMMAND libinput_backend_test)
    set_tests_properties(evdev.libinput_translation PROPERTIES LABELS "unit;evdev")
    add_test(NAME evdev.libinput_idle COMMAND libinput_backend_test idle)
    set_tests_properties(evdev.libinput_idle PROPERTIES LABELS "unit;evdev")
    add_test(NAME evdev.libinput_passthrough COMMAND libinput_backend_test passthrough)
    set_tests_properties(evdev.libinput_passthrough PROPERTIES LABELS "unit;evdev")
    add_test(NAME evdev.libinput_initialization COMMAND libinput_backend_test initialization)
    set_tests_properties(evdev.libinput_initialization PROPERTIES LABELS "unit;evdev")

    add_test(NAME evdev.input_helper COMMAND uiohook_tests)
    set_tests_properties(evdev.input_helper PROPERTIES LABELS "unit;evdev")
endif()

# Exercise find_package() from independent projects so missing exported
# dependencies are caught for both build-tree and installed packages.
foreach(mode build install)
    if(mode STREQUAL "install")
        set(package_config_subdir "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")
    else()
        set(package_config_subdir ".")
    endif()
    add_test(NAME package.${mode}_consumer
        COMMAND "${CMAKE_COMMAND}"
            "-DUIOHOOK_BINARY_DIR=${PROJECT_BINARY_DIR}"
            "-DUIOHOOK_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            "-DPACKAGE_CONFIG_SUBDIR=${package_config_subdir}"
            "-DGENERATOR=${CMAKE_GENERATOR}"
            "-DC_COMPILER=${CMAKE_C_COMPILER}"
            "-DCONFIG=$<CONFIG>"
            "-DMODE=${mode}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_package.cmake"
    )
    set_tests_properties(package.${mode}_consumer PROPERTIES LABELS "package")
endforeach()

add_executable(numeric_helper_test
    numeric_helper_test.c
    "${PROJECT_SOURCE_DIR}/src/numeric_helper.c"
    "${PROJECT_SOURCE_DIR}/src/logger.c"
)
configure_unit_test(numeric_helper_test)
add_test(NAME common.numeric_helper COMMAND numeric_helper_test)
set_tests_properties(common.numeric_helper PROPERTIES LABELS "unit;common")

# Floating-point clamps are compiled only on Apple platforms.
if(APPLE)
    add_executable(darwin_numeric_conversion_test
        darwin/numeric_conversion_test.c
        "${PROJECT_SOURCE_DIR}/src/numeric_helper.c"
        "${PROJECT_SOURCE_DIR}/src/logger.c"
    )
    configure_unit_test(darwin_numeric_conversion_test)
    add_test(NAME darwin.numeric_conversion COMMAND darwin_numeric_conversion_test)
    set_tests_properties(darwin.numeric_conversion PROPERTIES LABELS "unit;darwin")
endif()
