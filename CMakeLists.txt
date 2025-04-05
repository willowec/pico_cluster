cmake_minimum_required(VERSION 3.13...3.27)

# initialize the SDK based on PICO_SDK_PATH
include(pico_sdk_import.cmake)

project(pico_cluster C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

include_directories("src/" "inc/")

add_executable(head_pico
    src/head_main.c
    inc/shared.h
    src/shared.c
)

add_executable(compute_pico
    src/compute_main.c
    inc/shared.h
    src/shared.c
)


target_link_libraries(head_pico pico_stdlib pico_multicore pico_i2c_slave hardware_i2c)
pico_enable_stdio_usb(head_pico 1)
pico_enable_stdio_uart(head_pico 0)
pico_add_extra_outputs(head_pico)

target_link_libraries(compute_pico pico_stdlib pico_multicore pico_i2c_slave hardware_i2c)
pico_enable_stdio_usb(compute_pico 1)
pico_enable_stdio_uart(compute_pico 0)
pico_add_extra_outputs(compute_pico)
