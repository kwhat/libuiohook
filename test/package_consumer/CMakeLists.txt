cmake_minimum_required(VERSION 3.10)
project(uiohook_package_consumer C)

find_package(uiohook CONFIG REQUIRED)
add_executable(package_consumer main.c)
target_link_libraries(package_consumer PRIVATE uiohook)
