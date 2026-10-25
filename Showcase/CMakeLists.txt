add_executable(XenUIShowcase WIN32
        main.cpp
        Showcase.rc
)

XenUI_ConfigureTarget(XenUIShowcase)

target_link_options(XenUIShowcase PRIVATE /ENTRY:mainCRTStartup)
target_link_libraries(XenUIShowcase PRIVATE XenUI::XenUI)
