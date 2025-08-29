# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# 编译模式，ON为接收模式，OFF为发射模式
option(EN_RECEIVER_MODE "Enable receiver mode (minimal build)" OFF)

if(EN_RECEIVER_MODE)
    message(STATUS "Receiver mode is enabled. This will be a minimal build focusing on receiver functionality.")
    set(EXTRA_COMPONENT_DIRS "components/Receiver" "components/Peripherals")
else()
    message(STATUS "Minimal build - OFF")
    set(EXTRA_COMPONENT_DIRS "components/Peripherals" "components/lvgl_port")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)


project(demo-hello-world)
