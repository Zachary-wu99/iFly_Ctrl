# 项目顶层构建脚本。
# 负责汇总自写源码、包含路径和 STM32CubeMX 生成工程。
cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME iFly_Ctrl)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM/C/C++ languages
enable_language(C CXX ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    iFly/app_main.cpp
    iFly/app/cli/flight_ctrl_cli.cpp
    iFly/app/ctrl/pid.cpp
    iFly/app/fly_sys/parameter/parameter_manager.cpp
    iFly/app/fly_sys/parameter/sys_parameters.cpp
    iFly/app/fly_sys/ground_station/mavlink_parameter_service.cpp
    iFly/app/task_mage/task.cpp
    iFly/app/tick/tick.cpp
    iFly/app/proto/crsf/crsf_protocol.cpp
    iFly/app/proto/sbus/sbus_protocol.cpp
    iFly/lib/freelock_queue/lock_free_queue.cpp
    iFly/lib/shell/shell.cpp
    iFly/lib/soft_timer/soft_timer.cpp
    iFly/lib/systick_timer/systick_time.cpp
    iFly/lib/uart/uart_dma.cpp
    iFly/lib/usb_cdc/usb_cdc.cpp
    iFly/lib/can/can.cpp
    iFly/lib/pwm/pwm.cpp
    iFly/lib/spi/spi.cpp
    iFly/task/task_all_init.cpp
    iFly/task/task_mavlink.cpp
    iFly/task/task_led_ctrl.cpp
    iFly/task/task_pid_ctrl.cpp
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    iFly
    iFly/app/cli
    iFly/app/ctrl
    iFly/app/observer
    iFly/app/task_mage
    iFly/app/tick
    iFly/app/proto/crsf
    iFly/app/proto/sbus
    iFly/app/ledctrl
    iFly/app/fly_sys
    iFly/app/fly_sys/parameter
    iFly/app/fly_sys/ground_station
    iFly/dev
    iFly/lib/double_buffer
    iFly/dev/can_dev
    iFly/dev/uart_dev
    iFly/dev/usb_dev
    iFly/lib/freelock_queue
    iFly/lib/shell
    iFly/lib/soft_timer
    iFly/lib/systick_timer
    iFly/lib/uart
    iFly/lib/usb_cdc
    iFly/lib/can
    iFly/lib/pwm
    iFly/lib/spi
    iFly/lib/gpio
    iFly/lib/double_buffer
    iFly/lib
    iFly/lib/platform
    iFly/task
)

target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM PRIVATE
    iFly/middlewares/mavlink
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)

