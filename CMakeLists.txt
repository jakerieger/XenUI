cmake_minimum_required(VERSION 3.14)
project(XenUI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/Source)
set(INC ${CMAKE_CURRENT_SOURCE_DIR}/Include/XenUI)
set(SHARED ${CMAKE_CURRENT_SOURCE_DIR}/Shared)
set(VEND ${CMAKE_CURRENT_SOURCE_DIR}/Vendor)

include(CMake/DetectWindowingSystem.cmake)

include_directories(
        ${INC}
        ${SHARED}
        ${SRC}
        ${VEND}
)

add_library(XenUI SHARED
        # SHARED
        ${SHARED}/Types.hpp
        ${SHARED}/Panic.hpp
        ${SHARED}/Expect.hpp
        ${SHARED}/IO.hpp

        # INCLUDES
        ${INC}/Export.hpp
        ${INC}/EventSystem.hpp
        ${INC}/Events.hpp
        ${INC}/NativeWindow.hpp
        ${INC}/PlatformWindow.hpp
        ${INC}/App.hpp

        # SOURCES
        ${SRC}/App.cpp
        ${SRC}/NativeWindow.cpp
)

target_compile_definitions(XenUI PRIVATE BUILDING_XENUI)

# Handle platform-specific sources
if (WINDOWING_SYSTEM STREQUAL "Win32")
    target_compile_definitions(XenUI PUBLIC -D_WS_WINDOWS)

    #    target_sources(XenUI PRIVATE
    #            ${INC}/Platform/Win32Window.h
    #    )
    #
    #    target_link_libraries(XenUI PRIVATE blend2d::blend2d)
elseif (WINDOWING_SYSTEM STREQUAL "X11")
    add_definitions(-D_WS_X11)
    target_compile_definitions(XenUI PUBLIC -D_WS_X11)

    target_sources(XenUI PRIVATE
            ${INC}/Platform/X11Window.hpp
            ${SRC}/Platform/X11Window.cpp
    )

    target_link_libraries(XenUI PRIVATE X11)
elseif (WINDOWING_SYSTEM STREQUAL "Wayland")
    add_definitions(-D_WS_WAYLAND)
    target_compile_definitions(XenUI PUBLIC -D_WS_WAYLAND)

    #    target_sources(XenUI PRIVATE
    #            ${INC}/Platform/WaylandWindow.h
    #            ${INC}/Platform/XdgShellClient.h
    #            ${SRC}/XdgShell.c
    #    )
    #
    #    target_link_libraries(XenUI PRIVATE
    #            wayland-client
    #            wayland-egl
    #            blend2d::blend2d
    #    )
elseif (WINDOWING_SYSTEM STREQUAL "Apple")
    target_compile_definitions(XenUI PUBLIC -D_WS_APPLE)

    #    target_sources(XenUI PRIVATE
    #            ${INCDIR}/XenUI/AppleWindow.h
    #            ${SRCDIR}/AppleWindow.cpp
    #    )
endif ()

# Examples
message("-- Building examples")
add_subdirectory(Examples/SimpleWindow)