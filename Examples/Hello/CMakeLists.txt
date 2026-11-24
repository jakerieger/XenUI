add_executable(XenUIHello WIN32
        main.cpp
)

XenUI_ConfigureTarget(XenUIHello)

if (MSVC)
    target_link_options(XenUIHello PRIVATE /ENTRY:mainCRTStartup)
endif ()
target_link_libraries(XenUIHello PRIVATE XenUI::XenUI)
