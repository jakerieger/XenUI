cmake_minimum_required(VERSION 3.26)
project(XenUI
        VERSION 0.1.0
        DESCRIPTION "GPU-accelerated Windows GUI framework for C++20"
        HOMEPAGE_URL "https://github.com/jakerieger/XenUI"
        LANGUAGES CXX
)

if (NOT MSVC)
    message(FATAL_ERROR "XenUI requires the MSVC toolchain")
endif ()

option(XENUI_BUILD_TESTS "Build the XenUI unit tests" ON)
option(XENUI_BUILD_SHOWCASE "Build the XenUI showcase application" ON)
option(XENUI_BUILD_EXAMPLES "Build the small XenUI example applications" ON)
option(XENUI_INSTALL "Generate install rules for XenUI" ${PROJECT_IS_TOP_LEVEL})

if (PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
endif ()

include(GNUInstallDirs)
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(XenUIOptions)

add_subdirectory(Source/XenUI)

if (XENUI_BUILD_TESTS)
    enable_testing()
    include(FetchContent)
    FetchContent_Declare(doctest
            GIT_REPOSITORY https://github.com/doctest/doctest.git
            GIT_TAG v2.4.11
            GIT_SHALLOW TRUE
            SOURCE_SUBDIR header-only-no-cmake
    )
    FetchContent_MakeAvailable(doctest)
    add_subdirectory(Tests)
endif ()

if (XENUI_BUILD_SHOWCASE)
    add_subdirectory(Showcase)
endif ()

if (XENUI_BUILD_EXAMPLES)
    add_subdirectory(Examples/Hello)
endif ()

if (XENUI_INSTALL)
    include(cmake/XenUIInstall.cmake)
endif ()
