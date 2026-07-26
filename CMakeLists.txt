cmake_minimum_required(VERSION 3.20.0)

# Makes this app's own boards/ directory discoverable to `west build -b ...`
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(stm32_sensor_node)

target_sources(app PRIVATE src/main.c)
