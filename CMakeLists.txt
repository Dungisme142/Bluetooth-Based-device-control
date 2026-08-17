cmake_minimum_required(VERSION 3.22)

set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gcc-arm-none-eabi.cmake)

project(firmware C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# ============================================================
# Source files
# ============================================================

# Gom toàn bộ file .c trong src/, lib/, Drivers/
file(GLOB_RECURSE PROJECT_SOURCES
    "${CMAKE_SOURCE_DIR}/src/*.c"
    "${CMAKE_SOURCE_DIR}/lib/Src/*.c"
    "${CMAKE_SOURCE_DIR}/Drivers/Src/*.c"
    "${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Src/*.c"
)

# Các file *_template.c chỉ để tham khảo/copy,
# không được compile trực tiếp
list(FILTER PROJECT_SOURCES EXCLUDE REGEX "_template\\.c$")
# Không compile STM32 LL drivers nếu project chỉ sử dụng HAL
list(FILTER PROJECT_SOURCES EXCLUDE REGEX "stm32f1xx_ll_.*\\.c$")
add_executable(${CMAKE_PROJECT_NAME}
    ${PROJECT_SOURCES}
    ${CMAKE_SOURCE_DIR}/startup_stm32f103xb.s
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F1xx/Source/Templates/system_stm32f1xx.c
)

# ============================================================
# Include directories
# ============================================================

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/lib/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F1xx_HAL_Driver/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F1xx/Include
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
)

# ============================================================
# Preprocessor definitions
# ============================================================

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    STM32F103xB
    USE_HAL_DRIVER
)

# ============================================================
# Generate .hex and .bin
# ============================================================

add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY}
            -O ihex
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            ${CMAKE_PROJECT_NAME}.hex

    COMMAND ${CMAKE_OBJCOPY}
            -O binary
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            ${CMAKE_PROJECT_NAME}.bin
)