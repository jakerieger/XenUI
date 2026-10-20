add_executable(XenUIHello WIN32
        main.cpp
)

XenUI_ConfigureTarget(XenUIHello)

target_link_options(XenUIHello PRIVATE /ENTRY:mainCRTStartup)
target_link_libraries(XenUIHello PRIVATE XenUI::XenUI)
