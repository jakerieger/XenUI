cmake_minimum_required(VERSION 3.28)
project(XenUI)

set(CMAKE_CXX_STANDARD 20)

# Set the output directory for executables
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Set the output directory for libraries (DLLs)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_definitions(-D_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING)

include_directories(${CMAKE_SOURCE_DIR}/Lib)

add_library(XenUI SHARED
        Lib/Platform.h
        Lib/Types.h
        Lib/XenAPI.h
        Lib/UI/Offset.h
        Lib/UI/Offset.cpp
        Lib/UI/Size.h
        Lib/UI/Rect.cpp
        Lib/UI/Rect.h
        Lib/UI/Color.cpp
        Lib/UI/Color.h
        Lib/UI/LinearGradient.cpp
        Lib/UI/LinearGradient.h
        Lib/UI/Box.cpp
        Lib/UI/Box.h
        Lib/Helpers.inl
        Lib/UI/Text.cpp
        Lib/UI/Text.h
        Lib/AppWindow.cpp
        Lib/AppWindow.h
        Lib/Renderer.cpp
        Lib/Renderer.h
        Lib/XenUI.h
        Lib/Widgets/Widget.cpp
        Lib/Widgets/Widget.h
        Lib/RenderTree.cpp
        Lib/RenderTree.h
        Lib/UI/Element.cpp
        Lib/UI/Element.h
        Lib/XenApp.cpp
        Lib/XenApp.h
        Lib/UI/Theme.h
        Lib/AppContext.cpp
        Lib/AppContext.h
        Lib/UI/Interactive.h
        Lib/API/Dialog.cpp
        Lib/API/Dialog.h
        Lib/ElementAllocator.cpp
        Lib/ElementAllocator.h
        Lib/UI/SVG.cpp
        Lib/UI/SVG.h
        Lib/RuntimeError.h
)

find_package(NanoSVG CONFIG REQUIRED)
target_link_libraries(XenUI PRIVATE NanoSVG::nanosvg)

add_executable(HelloXen Demos/HelloXen/main.cpp
        Demos/Res/app.rc
        Demos/Res/resource.h
)
target_include_directories(HelloXen PRIVATE Demos/Res)
target_link_libraries(HelloXen XenUI NanoSVG::nanosvg)

add_executable(SVGRendering Demos/SVGRendering/main.cpp
        Demos/Res/app.rc
        Demos/Res/resource.h
)
target_include_directories(SVGRendering PRIVATE Demos/Res)
target_link_libraries(SVGRendering XenUI NanoSVG::nanosvg)
configure_file(Demos/SVGRendering/XenLogo.svg ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/XenLogo.svg COPYONLY)

add_executable(test_offset Tests/test_offset.cpp)
find_package(Catch2 CONFIG REQUIRED)
target_link_libraries(test_offset PRIVATE XenUI NanoSVG::nanosvg Catch2::Catch2 Catch2::Catch2WithMain)