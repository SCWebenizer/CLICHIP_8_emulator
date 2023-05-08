cmake_minimum_required(VERSION 3.21)
project(CLICHIP_8_emulator VERSION 1.0)

add_executable(CLICHIP_8_emulator CLICHIP_8_emulator.c)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED True)
set(CMAKE_C_FLAGS "-Wall -Wextra -Werror")

configure_file(CLICHIP_8_emulatorConfig.h.in CLICHIP_8_emulatorConfig.h)
target_include_directories(CLICHIP_8_emulator PUBLIC
                           "${PROJECT_BINARY_DIR}"
                           )

