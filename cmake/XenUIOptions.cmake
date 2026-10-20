# Compiler settings shared by every XenUI target.
function(XenUI_ConfigureTarget Target)
    target_compile_options(${Target} PRIVATE
            /W4
            /permissive-
            /utf-8
            /Zc:__cplusplus
    )
    target_compile_definitions(${Target} PRIVATE
            WIN32_LEAN_AND_MEAN
            NOMINMAX
            UNICODE
            _UNICODE
            _CRT_SECURE_NO_WARNINGS
    )
endfunction()
