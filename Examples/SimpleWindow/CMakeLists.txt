cmake_minimum_required(VERSION 3.14)
project(XenUI)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Examples/SimpleWindow)

add_executable(SimpleWindow
        main.cpp
)

target_link_libraries(SimpleWindow PUBLIC XenUI)