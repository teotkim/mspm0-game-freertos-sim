cmake_minimum_required(VERSION 3.15)
project(mspm0_game_freertos_sim C)

add_compile_options(-Wall -Wextra -Wpedantic)

add_library(freertos_config INTERFACE)
target_include_directories(freertos_config INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include")

set(FREERTOS_HEAP "3" CACHE STRING "" FORCE)
set(FREERTOS_PORT "GCC_POSIX" CACHE STRING "" FORCE)
add_subdirectory(vendor/FreeRTOS-Kernel)

add_executable(game_sim src/game_sim.c)
target_link_libraries(game_sim PRIVATE freertos_kernel freertos_config)
