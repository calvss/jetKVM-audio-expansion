# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH "/home/calvin/git/pico-sdk")

set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.4.0")
  message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.4.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

project(kvm-audio-sw C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(kvm-audio-sw kvm-audio-sw.cpp )

pico_set_program_name(kvm-audio-sw "kvm-audio-sw")
pico_set_program_version(kvm-audio-sw "0.1")

pico_enable_stdio_uart(kvm-audio-sw 0)
pico_enable_stdio_usb(kvm-audio-sw 1)

# Add the standard library to the build
target_link_libraries(kvm-audio-sw
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(kvm-audio-sw PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts or any other standard includes, if required
)

# Add any user requested libraries
target_link_libraries(kvm-audio-sw 
        hardware_adc
        hardware_uart
        hardware_dma
        )

pico_add_extra_outputs(kvm-audio-sw)

