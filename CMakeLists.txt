add_subdirectory(Source/XenUI)

if (XEN_BUILD_TESTS)
    add_subdirectory(Tests)
endif ()

if (XEN_BUILD_SHOWCASE)
    add_subdirectory(Showcase)
endif ()

if (XEN_BUILD_EXAMPLES)
    add_subdirectory(Examples/Hello)
endif ()
