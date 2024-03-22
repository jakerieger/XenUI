cmake_minimum_required(VERSION 3.28)

project(XenTests)

find_package(Catch2 CONFIG REQUIRED)

include_directories(../Lib)

add_executable(test_offset test_offset.cpp)
target_link_libraries(test_offset PRIVATE XenUI Catch2::Catch2 Catch2::Catch2WithMain)