# Unit targets compile production sources directly, without loading libuiohook's
# native startup code. Platform SDKs are needed only by tests using native types.
function(configure_unit_test target)
    set_target_properties(${target} PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
    target_include_directories(${target} PRIVATE
        "${PROJECT_SOURCE_DIR}/include"
        "${PROJECT_SOURCE_DIR}/src"
        "${PROJECT_SOURCE_DIR}/src/${UIOHOOK_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endfunction()

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
        "${PROJECT_SOURCE_DIR}/src/input_state.c"
        "${PROJECT_SOURCE_DIR}/src/evdev/dispatch_event.c"
        "${PROJECT_SOURCE_DIR}/src/evdev/system_properties.c"
        evdev/mock_desktop.c
    )
    configure_unit_test(evdev_test_backend)
    target_include_directories(evdev_test_backend PUBLIC
        ${XKB_INCLUDE_DIRS} ${XKB_X11_INCLUDE_DIRS} ${XCB_X11_INCLUDE_DIRS}
        ${X11_INCLUDE_DIRS} ${XINERAMA_INCLUDE_DIRS}
    )
    target_link_libraries(evdev_test_backend PUBLIC ${XKB_LDFLAGS})
    target_link_libraries(uiohook_tests evdev_test_backend)

    add_executable(system_properties_test system_properties_test.c)
    configure_unit_test(system_properties_test)
    target_link_libraries(system_properties_test evdev_test_backend)
    foreach(property repeat pointer click_time screens)
        add_test(NAME evdev.properties.${property} COMMAND system_properties_test ${property})
        set_tests_properties(evdev.properties.${property} PROPERTIES LABELS "unit;evdev")
    endforeach()

    add_executable(touchpad_test
        evdev/touchpad_helper_test.c
        "${PROJECT_SOURCE_DIR}/src/evdev/touchpad_helper.c"
    )
    configure_unit_test(touchpad_test)
    target_include_directories(touchpad_test PRIVATE ${LIBINPUT_INCLUDE_DIRS} ${EVDEV_INCLUDE_DIRS})
    # libinput and uinput calls are provided by the test, not a live backend.
    target_link_libraries(touchpad_test evdev_test_backend ${EVDEV_LDFLAGS})
    # New scroll events are only used to check that legacy and new events are
    # not both dispatched. Keep the test buildable with older libinput headers.
    if(LIBINPUT_VERSION VERSION_GREATER_EQUAL "1.19")
        target_compile_definitions(touchpad_test PRIVATE HAVE_LIBINPUT_POINTER_SCROLL_FINGER)
    endif()
    add_test(NAME evdev.touchpad_translation COMMAND touchpad_test)
    set_tests_properties(evdev.touchpad_translation PROPERTIES LABELS "unit;evdev")
    add_test(NAME evdev.touchpad_initialization COMMAND touchpad_test initialization)
    set_tests_properties(evdev.touchpad_initialization PROPERTIES LABELS "unit;evdev")

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
