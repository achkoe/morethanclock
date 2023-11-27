cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(dcf77_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(dcf77
  dcf77.c
)

pico_enable_stdio_usb(dcf77 1)
pico_enable_stdio_uart(dcf77 1)
pico_add_extra_outputs(dcf77)
target_link_libraries(dcf77 pico_stdlib)