cmake_minimum_required(VERSION 3.28)
project(XenUI)

set(CMAKE_CXX_STANDARD 20)

# Set the output directory for executables
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Set the output directory for libraries (DLLs)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories(${CMAKE_SOURCE_DIR}/Lib)

add_library(XenUI SHARED Lib/Platform.h Lib/Types.h Lib/AppWindow.cpp Lib/AppWindow.h
        Lib/XenAPI.h
        Lib/XenRenderer.cpp
        Lib/XenRenderer.h
        Lib/UI/Offset.h
        Lib/UI/Size.h
        Lib/UI/Rect.cpp
        Lib/UI/Rect.h
        Lib/XenUI.h
        Lib/UI/Color.cpp
        Lib/UI/Color.h
        Lib/UI/LinearGradient.cpp
        Lib/UI/LinearGradient.h
        Lib/UI/Box.cpp
        Lib/UI/Box.h
        Lib/Helpers.inl)

add_executable(HelloXen Demos/HelloXen/main.cpp
        Demos/Res/app.rc)
target_link_libraries(HelloXen XenUI)

add_executable(test_offset Tests/test_offset.cpp)
find_package(Catch2 CONFIG REQUIRED)
target_link_libraries(test_offset PRIVATE XenUI Catch2::Catch2 Catch2::Catch2WithMain)