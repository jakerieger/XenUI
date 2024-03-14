cmake_minimum_required(VERSION 3.28)
project(XenUI)

set(CMAKE_CXX_STANDARD 20)

include_directories(${CMAKE_SOURCE_DIR}/Lib)

add_library(XenUI SHARED Lib/Platform.h Lib/Types.h Lib/AppWindow.cpp Lib/AppWindow.h
        Lib/XenAPI.h
        Lib/XenRenderer.cpp
        Lib/XenRenderer.h
        Lib/UI/Offset.cpp
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
        Lib/UI/Box.h)

add_executable(HelloXen Demos/HelloXen/main.cpp
)
target_link_libraries(HelloXen XenUI)