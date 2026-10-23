add_executable(XenUITests
        Main.cpp
        ArenaTests.cpp
        SignalTests.cpp
        ElementTests.cpp
        ControlsTests.cpp
        LayoutTests.cpp
        InputTests.cpp
        LiveTests.cpp
        TextTests.cpp
        DragDropTests.cpp
        AnimationTests.cpp
        AccessibilityTests.cpp
        CatalogTests.cpp
        DeveloperToolsTests.cpp
        TestWidgets.hpp
)

XenUI_ConfigureTarget(XenUITests)

target_include_directories(XenUITests SYSTEM PRIVATE "${doctest_SOURCE_DIR}")
target_link_libraries(XenUITests PRIVATE XenUI::XenUI)

add_test(NAME XenUITests COMMAND XenUITests)
