# Third-party libraries for the Linux (SDL3 + OpenGL) backend.
#
# System packages (Debian/Ubuntu names):
#   libharfbuzz-dev libfontconfig-dev libunibreak-dev libfreetype-dev libsdl3-dev
#
# OpenGL needs no development package: entry points are loaded at run time (Graphics/GL/GLApi.hpp).
#
# Each creates an imported target, so headers and libraries can also come from a prefix given in
# CMAKE_PREFIX_PATH instead of /usr.

include(FetchContent)

find_package(Fontconfig REQUIRED)
find_package(Freetype REQUIRED)
find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3-shared)

# HarfBuzz and libunibreak ship pkg-config files but no CMake find modules on every distribution, so look them up
# directly.
function(XenUI_FindLibrary Target Header HeaderSuffix Library)
    find_path(${Target}_INCLUDE_DIR ${Header} PATH_SUFFIXES ${HeaderSuffix})
    find_library(${Target}_LIBRARY ${Library})
    if (NOT ${Target}_INCLUDE_DIR OR NOT ${Target}_LIBRARY)
        message(FATAL_ERROR "XenUI: ${Library} was not found. Install its development package "
                "(see cmake/XenUILinuxDeps.cmake) or add its prefix to CMAKE_PREFIX_PATH.")
    endif ()
    add_library(XenUI::${Target} UNKNOWN IMPORTED)
    set_target_properties(XenUI::${Target} PROPERTIES
            IMPORTED_LOCATION "${${Target}_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${${Target}_INCLUDE_DIR}"
    )
endfunction()

XenUI_FindLibrary(HarfBuzz hb.h harfbuzz harfbuzz)
XenUI_FindLibrary(Unibreak linebreak.h "" unibreak)

# stb_image is a single header, compiled into XenUI by Graphics/GL/Image.cpp.
FetchContent_Declare(stb
        URL https://github.com/nothings/stb/archive/6b5799bae8a1fe3792691ef4b72ae26434c3e799.tar.gz
)
# stb has no CMakeLists.txt, so this only downloads it.
FetchContent_MakeAvailable(stb)
set(XENUI_STB_DIR "${stb_SOURCE_DIR}")
