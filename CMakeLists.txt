###############################################################################
###############################################################################

set(FAMILY rp2040)
set(BOARD pico_sdk)

# Set minimum required version of CMake
cmake_minimum_required(VERSION 3.12)
#include build functions from Pico SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
#include($ENV{PICO_SDK_PATH}/lib/tinyusb/hw/bsp/rp2040/family.cmake)

# Set compiler:
set(CMAKE_C_COMPILER /usr/bin/arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER /usr/bin/arm-none-eabi-g++)

# Set name of project (as PROJECT_NAME) and C/C++ Standards
project(pad C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Creates a pico-sdk subdirectory in our project for the libraries
pico_sdk_init()

###############################################################################
###############################################################################

# point out the CMake, where to find the executable source file
add_executable(${PROJECT_NAME}
        main.c
        usb_descriptors.c
)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/generated)
pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)

# create map/bin/hex/uf2 files.
pico_add_extra_outputs(${PROJECT_NAME})
# Pull in our pico_stdlib which pulls in commonly used features (gpio, timer-delay etc)
target_link_libraries(${PROJECT_NAME}
            pico_stdlib
            pico_unique_id
            pico_time
            hardware_adc
            hardware_pio
            tinyusb_device 
            tinyusb_board
)

pico_add_extra_outputs(${PROJECT_NAME})
#add_dependencies(${PROJECT_NAME} pio_ws2812_datasheet)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
