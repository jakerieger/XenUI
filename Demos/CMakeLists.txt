cmake_minimum_required(VERSION 3.28)

project(Demos)

include_directories(../Lib Res)

find_package(NanoSVG CONFIG REQUIRED)

add_executable(HelloXen HelloXen/main.cpp
        Res/app.rc
        Res/resource.h
)
target_include_directories(HelloXen PRIVATE Res)
target_link_libraries(HelloXen XenUI NanoSVG::nanosvg)

add_executable(SVGRendering SVGRendering/main.cpp
        Res/app.rc
        Res/resource.h
)
target_include_directories(SVGRendering PRIVATE Demos/Res)
target_link_libraries(SVGRendering XenUI NanoSVG::nanosvg)
configure_file(SVGRendering/XenLogo.svg ${CMAKE_BINARY_DIR}/Demos/XenLogo.svg COPYONLY)