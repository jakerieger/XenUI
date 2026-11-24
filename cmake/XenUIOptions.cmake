# Compiler settings shared by every XenUI target.
function(XenUI_ConfigureTarget Target)
    if (MSVC)
        target_compile_options(${Target} PRIVATE
                /W4
                /permissive-
                /utf-8
                /Zc:__cplusplus
        )
    else ()
        target_compile_options(${Target} PRIVATE
                -Wall
                -Wextra
                -Wno-missing-field-initializers
                -finput-charset=UTF-8
        )
        set_target_properties(${Target} PROPERTIES
                CXX_VISIBILITY_PRESET hidden
                VISIBILITY_INLINES_HIDDEN ON
        )
    endif ()

    if (WIN32)
        target_compile_definitions(${Target} PRIVATE
                WIN32_LEAN_AND_MEAN
                NOMINMAX
                UNICODE
                _UNICODE
                _CRT_SECURE_NO_WARNINGS
        )
    endif ()

    target_compile_definitions(${Target} PRIVATE
            XENUI_PLATFORM_${XENUI_PLATFORM}=1
            XENUI_RENDERER_${XENUI_RENDERER}=1
    )
endfunction()
