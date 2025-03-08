SET(CMAKE_SYSTEM_PROCESSOR arm)
SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_VERSION 1)

SET(cpu_flags "-march=armv7-a -mfloat-abi=hard -mfpu=neon")

# specify the cross compiler to use the one from the uClibc toolchain
SET(CMAKE_C_COMPILER   ${PROJECT_SOURCE_DIR}/sdk/buildroot-2024.11.2/output/host/bin/arm-linux-gcc ${cpu_flags})
SET(CMAKE_CXX_COMPILER ${PROJECT_SOURCE_DIR}/sdk/buildroot-2024.11.2/output/host/bin/arm-linux-g++ ${cpu_flags})
SET(CMAKE_STRIP ${PROJECT_SOURCE_DIR}/sdk/buildroot-2024.11.2/output/host/bin/arm-linux-strip)

# target environment from uClibc toolchain
SET(CMAKE_FIND_ROOT_PATH  ${PROJECT_SOURCE_DIR}/sdk/buildroot-2024.11.2/output/host/arm-buildroot-linux-uclibcgnueabihf/sysroot/usr)

# search for programs in the build host directories
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)