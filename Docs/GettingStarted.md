# Getting started

## Requirements

- Windows 10 version 1903 or later, x64.
- Visual Studio 2022 or later with the **Desktop development with C++** workload. XenUI needs MSVC and the Windows
  SDK; Clang and MinGW are not supported.
- CMake 3.26 or later and Ninja. Both ship with Visual Studio.

XenUI is a DLL that exports C++ classes, so your application must use the same MSVC toolset generation and the
dynamic C runtime (`/MD` or `/MDd`), which is CMake's default.

## Building XenUI

Open an **x64 Native Tools Command Prompt for VS** (or run `vcvars64.bat` in your shell) and, from the repository
root:

```bash
cmake --preset debug
cmake --build --preset debug
ctest --test-dir build/debug --output-on-failure
```

The `release` preset builds `RelWithDebInfo`. Everything is written to `build/<preset>/bin`:

| File | What it is |
|---|---|
| `XenUId.dll`, `XenUI.dll` | The library (Debug and optimized) |
| `XenUIShowcase.exe` | A tour of every widget, with a theme switcher |
| `XenUIHello.exe` | The smallest complete application |
| `XenUITests.exe` | The unit tests |

Turn parts off with `-DXENUI_BUILD_TESTS=OFF`, `-DXENUI_BUILD_SHOWCASE=OFF` or `-DXENUI_BUILD_EXAMPLES=OFF`.

## Adding XenUI to your project

### Option 1: an installed package

Install XenUI once. Debug and release builds can share a prefix, since the Debug DLL has a `d` suffix:

```bash
cmake --install build/debug --prefix C:/Libraries/XenUI
cmake --install build/release --prefix C:/Libraries/XenUI
```

Then use it from your project, configuring with `-DCMAKE_PREFIX_PATH=C:/Libraries/XenUI`:

```cmake
cmake_minimum_required(VERSION 3.26)
project(MyApp LANGUAGES CXX)

find_package(XenUI 0.1 CONFIG REQUIRED)

add_executable(MyApp WIN32 main.cpp)
target_link_options(MyApp PRIVATE /ENTRY:mainCRTStartup)
target_link_libraries(MyApp PRIVATE XenUI::XenUI)

# Copy XenUI.dll (or XenUId.dll) next to the executable after every build.
add_custom_command(TARGET MyApp POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different $<TARGET_RUNTIME_DLLS:MyApp> $<TARGET_FILE_DIR:MyApp>
        COMMAND_EXPAND_LISTS
)
```

`XenUI::XenUI` brings in the include directory and C++20. A consumer built as `Release`, `RelWithDebInfo` or
`MinSizeRel` links whichever optimized XenUI build is installed; a `Debug` consumer links the Debug build.

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

When XenUI is not the top-level project its install rules are off by default (`XENUI_INSTALL`). The DLL is built
into your project's runtime output directory if you set `CMAKE_RUNTIME_OUTPUT_DIRECTORY`; otherwise use the same
`POST_BUILD` copy shown above.

## Why `WIN32` and `/ENTRY:mainCRTStartup`

`WIN32` builds a GUI application, so no console window opens. Windows then expects a `WinMain` entry point;
`/ENTRY:mainCRTStartup` keeps the ordinary `int main()` instead. Leave both out if you want a console for logging
while developing.

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
- **`App::Run`** processes messages until `Quit` is called. Quitting when the main window closes is the usual choice.

The run loop sleeps whenever nothing is changing, so an idle XenUI window uses close to no CPU.

From here, read [Widgets](Widgets.md) to write your own components and [Signals](Signals.md) to give them state.
`Examples/Hello` is a complete small application, and `Showcase/main.cpp` shows every widget in use.

## Checking the version

`XenUI/Version.hpp` (included by `XenUI.hpp`) defines `XENUI_VERSION_MAJOR`, `XENUI_VERSION_MINOR`,
`XENUI_VERSION_PATCH` and `XENUI_VERSION_STRING`, and `XenUI::kVersion` for the headers you compiled against.
`XenUI::GetRuntimeVersion()` returns the version of the DLL actually loaded. The DLL also carries a Windows version
resource, visible in its file properties.
