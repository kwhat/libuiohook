function(run_checked)
    execute_process(COMMAND ${ARGV} RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE errors)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "Package consumer check failed (${status}):\n${output}\n${errors}")
    endif()
endfunction()

set(check_dir "${UIOHOOK_BINARY_DIR}/test/package-check-${MODE}")
if(MODE STREQUAL "install")
    set(package_prefix "${check_dir}/prefix")
    run_checked("${CMAKE_COMMAND}"
        "-DCMAKE_INSTALL_PREFIX=${package_prefix}"
        "-DCMAKE_INSTALL_CONFIG_NAME=${CONFIG}"
        -P "${UIOHOOK_BINARY_DIR}/cmake_install.cmake"
    )
else()
    set(package_prefix "${UIOHOOK_BINARY_DIR}")
endif()

run_checked("${CMAKE_COMMAND}"
    -S "${UIOHOOK_SOURCE_DIR}/test/package_consumer"
    -B "${check_dir}/build"
    -G "${GENERATOR}"
    "-DCMAKE_C_COMPILER=${C_COMPILER}"
    "-DCMAKE_BUILD_TYPE=${CONFIG}"
    "-Duiohook_DIR=${package_prefix}/${PACKAGE_CONFIG_SUBDIR}"
)
run_checked("${CMAKE_COMMAND}" --build "${check_dir}/build" --config "${CONFIG}")
