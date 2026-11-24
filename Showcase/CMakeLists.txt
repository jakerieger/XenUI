add_executable(XenUIShowcase WIN32
        main.cpp
        $<$<PLATFORM_ID:Windows>:Showcase.rc>
)

XenUI_ConfigureTarget(XenUIShowcase)

if (MSVC)
    target_link_options(XenUIShowcase PRIVATE /ENTRY:mainCRTStartup)
endif ()
target_link_libraries(XenUIShowcase PRIVATE XenUI::XenUI)
