cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# 选择模式
if(EN_RECEIVER_MODE)
    # 指定接收端组件目录
    set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/receiver_main")
else()
    # 指定主端组件目录
    set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/main")
endif()

project(demo_hello_world)
