# Getting started

XenUI runs on Windows and Linux from the same source. The public API is identical; only the backend underneath
differs (see [Architecture](Architecture.md#platforms-and-backends)).

## Requirements

### Windows

- Windows 10 version 1903 or later, x64.
- Visual Studio 2022 or later with the **Desktop development with C++** workload. XenUI needs MSVC and the Windows
  SDK; Clang and MinGW are not supported on Windows.
- CMake 3.26 or later and Ninja. Both ship with Visual Studio.

XenUI is a DLL that exports C++ classes, so your application must use the same MSVC toolset generation and the
dynamic C runtime (`/MD` or `/MDd`), which is CMake's default.

### Linux

- An x86-64 distribution with SDL 3.2 or later. On Ubuntu that means 25.04 or later; CI builds on 26.04.
- GCC 13 or later, or Clang 17 or later.
- CMake 3.26 or later and Ninja.
- OpenGL 3.3 core from the graphics driver (Mesa, NVIDIA or AMD). No GL development package is needed: XenUI loads
  OpenGL at run time.
- Wayland or X11. Without a display server, SDL's `offscreen` driver still renders, which is how the tests run in CI.

Install the development packages (Debian and Ubuntu names):

```bash
sudo apt install build-essential cmake ninja-build libsdl3-dev libharfbuzz-dev libfontconfig-dev libfreetype-dev libunibreak-dev
```

Other distributions package the same libraries under similar names. stb_image is downloaded when configuring.

## Building XenUI

### Windows

Open an **x64 Native Tools Command Prompt for VS** (or run `vcvars64.bat` in your shell) and, from the repository
root:

```bash
cmake --preset debug
cmake --build --preset debug
ctest --test-dir build/debug --output-on-failure
```

### Linux

From the repository root:

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --test-dir build/linux-debug --output-on-failure
```

Pick the compiler with `CC` and `CXX` (for example `CC=clang CXX=clang++ cmake --preset linux-debug`).

### Output

The `release` and `linux-release` presets build `RelWithDebInfo`. Programs are written to `build/<preset>/bin`
and, on Linux, the library to `build/<preset>/lib`:

| File                                          | What it is                                    |
|-----------------------------------------------|-----------------------------------------------|
| `XenUId.dll`, `XenUI.dll` (Windows)           | The library (Debug and optimized)             |
| `libXenUId.so`, `libXenUI.so` (Linux)         | The library (Debug and optimized)             |
| `XenUIShowcase`                               | A tour of every widget, with a theme switcher |
| `XenUIHello`                                  | The smallest complete application             |
| `XenUITests`                                  | The unit tests                                |

Turn parts off with `-DXENUI_BUILD_TESTS=OFF`, `-DXENUI_BUILD_SHOWCASE=OFF` or `-DXENUI_BUILD_EXAMPLES=OFF`.

## Adding XenUI to your project

### Option 1: an installed package

Install XenUI once. Debug and release builds can share a prefix, since the Debug library has a `d` suffix:

```bash
cmake --install build/debug --prefix C:/Libraries/XenUI
cmake --install build/release --prefix C:/Libraries/XenUI
```

On Linux, install the `linux-debug` and `linux-release` builds the same way, for example to `~/.local` or
`/opt/xenui`.

Then use it from your project, configuring with `-DCMAKE_PREFIX_PATH=<the prefix>`:

```cmake
cmake_minimum_required(VERSION 3.26)
project(MyApp LANGUAGES CXX)

find_package(XenUI 0.1 CONFIG REQUIRED)

add_executable(MyApp WIN32 main.cpp)
target_link_libraries(MyApp PRIVATE XenUI::XenUI)

if (MSVC)
    target_link_options(MyApp PRIVATE /ENTRY:mainCRTStartup)
    # Copy XenUI.dll (or XenUId.dll) next to the executable after every build.
    add_custom_command(TARGET MyApp POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different $<TARGET_RUNTIME_DLLS:MyApp> $<TARGET_FILE_DIR:MyApp>
            COMMAND_EXPAND_LISTS
    )
endif ()
```

`XenUI::XenUI` brings in the include directory and C++20. A consumer built as `Release`, `RelWithDebInfo` or
`MinSizeRel` links whichever optimized XenUI build is installed; a `Debug` consumer links the Debug build. On Linux
the executable finds the shared library through the build tree's RPATH; for a distributed application, install
`libXenUI.so` next to it or into a library directory.

XenUI's own dependencies (SDL3, HarfBuzz and the rest on Linux; Direct2D and friends on Windows) are private to the
shared library, so your project doesn't link them.

### Option 2: building XenUI as part of your project

Add the repository as a subdirectory or fetch it, and link the same target:

```cmake
include(FetchContent)
FetchContent_Declare(XenUI
        GIT_REPOSITORY https://github.com/jakerieger/XenUI.git
        GIT_TAG master
)
set(XENUI_BUILD_TESTS OFF)
set(XENUI_BUILD_SHOWCASE OFF)
set(XENUI_BUILD_EXAMPLES OFF)
FetchContent_MakeAvailable(XenUI)

target_link_libraries(MyApp PRIVATE XenUI::XenUI)
```

When XenUI is not the top-level project its install rules are off by default (`XENUI_INSTALL`). On Windows the DLL
is built into your project's runtime output directory if you set `CMAKE_RUNTIME_OUTPUT_DIRECTORY`; otherwise use the
same `POST_BUILD` copy shown above.

## Why `WIN32` and `/ENTRY:mainCRTStartup`

On Windows, `WIN32` builds a GUI application, so no console window opens. Windows then expects a `WinMain` entry
point; `/ENTRY:mainCRTStartup` keeps the ordinary `int main()` instead. Leave both out if you want a console for
logging while developing. Other platforms ignore `WIN32`, and Linux applications start from `main` either way.

## A first window

```cpp
#include <XenUI/XenUI.hpp>

using namespace XenUI;

int main() {
    App MyApp;

    Window MainWindow({.Title = "My app", .ClientSize = {640.0f, 400.0f}});
    MainWindow.SetOnClosed([&MyApp] { MyApp.Quit(); });
    MainWindow.SetContent(Center {.Child = Text {.Content = "Hello from XenUI"}});
    MainWindow.Show();

    return MyApp.Run();
}
```

- **`App`** sets up the graphics device and the run loop. Create exactly one, before any window, and keep it alive
  until the end.
- **`Window`** is a top-level window. `WindowDesc` sets its title, initial size in DIPs, background color, whether it
  can be resized, and whether the content draws its own title bar.
- **`SetContent`** shows a widget tree that fills the client area. Any widget converts to the `Node` it takes.
- **`App::Run`** processes events until `Quit` is called. Quitting when the main window closes is the usual choice.

The run loop sleeps whenever nothing is changing, so an idle XenUI window uses close to no CPU.

From here, read [Widgets](Widgets.md) to write your own components and [Signals](Signals.md) to give them state.
`Examples/Hello` is a complete small application, and `Showcase/main.cpp` shows every widget in use.

## Checking the version

`XenUI/Version.hpp` (included by `XenUI.hpp`) defines `XENUI_VERSION_MAJOR`, `XENUI_VERSION_MINOR`,
`XENUI_VERSION_PATCH` and `XENUI_VERSION_STRING`, and `XenUI::kVersion` for the headers you compiled against.
`XenUI::GetRuntimeVersion()` returns the version of the library actually loaded. On Windows the DLL also carries a
version resource, visible in its file properties.
