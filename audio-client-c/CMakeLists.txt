cmake_minimum_required(VERSION 3.10.0)
set(CMAKE_TOOLCHAIN_FILE toolchain.cmake)
set(CMAKE_C_STANDARD 99)

project(audio-client-c VERSION 0.1.0 LANGUAGES C)

add_executable(audio-client-c main.c)

