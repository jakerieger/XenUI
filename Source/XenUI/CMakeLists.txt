# Sources that still depend on Win32, Direct3D 11, Direct2D or DirectWrite. Everything in the main list below
# builds on every platform. The Linux (SDL3 + OpenGL) twins of these files are in the Linux list.
if (XENUI_PLATFORM STREQUAL "Win32")
    find_program(XENUI_FXC fxc HINTS "$ENV{WindowsSdkVerBinPath}" "$ENV{WindowsSdkBinPath}" PATH_SUFFIXES x64 REQUIRED)

    set(XENUI_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/Shaders")
    set(XENUI_SHADER_HEADERS)

    function(XenUI_CompileShader Source Entry Profile VariableName Output)
        set(OutputPath "${XENUI_SHADER_DIR}/${Output}")
        add_custom_command(
                OUTPUT "${OutputPath}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${XENUI_SHADER_DIR}"
                COMMAND "${XENUI_FXC}" /nologo /O3 /T ${Profile} /E ${Entry} /Vn ${VariableName} /Fh "${OutputPath}" "${CMAKE_CURRENT_SOURCE_DIR}/${Source}"
                DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${Source}"
                COMMENT "Compiling ${Source} (${Entry})"
                VERBATIM
        )
        set(XENUI_SHADER_HEADERS ${XENUI_SHADER_HEADERS} "${OutputPath}" PARENT_SCOPE)
    endfunction()

    XenUI_CompileShader(Graphics/D2D/Shaders/LivePrimitives.hlsl VsMain vs_4_0 kLivePrimitivesVertexShader LivePrimitivesVS.h)
    XenUI_CompileShader(Graphics/D2D/Shaders/LivePrimitives.hlsl PsMain ps_4_0 kLivePrimitivesPixelShader LivePrimitivesPS.h)
    XenUI_CompileShader(Graphics/D2D/Shaders/LiveTexture.hlsl VsMain vs_4_0 kLiveTextureVertexShader LiveTextureVS.h)
    XenUI_CompileShader(Graphics/D2D/Shaders/LiveTexture.hlsl PsMain ps_4_0 kLiveTexturePixelShader LiveTexturePS.h)

    set(XENUI_PLATFORM_SOURCES
            Platform/Win32/AppContext.cpp
            Graphics/D2D/CompositorBackend.cpp
            Graphics/D2D/CompositorBackend.hpp
            Graphics/D2D/D2DConvert.hpp
            Graphics/D2D/D2DRenderer.hpp
            Graphics/D2D/GraphicsDevice.hpp
            Graphics/D2D/ImageData.hpp
            Graphics/D2D/LayerSurfaces.hpp
            Graphics/D2D/LiveRenderer.hpp
            Graphics/D2D/PathData.hpp
            Graphics/D2D/SwapChain.hpp
            Graphics/D2D/TextLayoutData.hpp
            Platform/Win32/Accessibility.hpp
            Platform/Win32/HResult.hpp
            Platform/Win32/PopupManager.hpp
            Platform/Win32/SystemDragDrop.hpp
            Platform/Win32/Utf8.hpp
            Platform/Win32/WindowImpl.hpp
            Graphics/D2D/D2DRenderer.cpp
            Graphics/D2D/GraphicsDevice.cpp
            Graphics/D2D/Image.cpp
            Graphics/D2D/LiveRenderer.cpp
            Graphics/D2D/Path.cpp
            Graphics/D2D/SwapChain.cpp
            Graphics/D2D/Text.cpp
            Platform/Win32/Accessibility.cpp
            Platform/Win32/Clipboard.cpp
            Platform/Win32/FileDialog.cpp
            Platform/Win32/PopupManager.cpp
            Platform/Win32/Shell.cpp
            Platform/Win32/SystemDragDrop.cpp
            Platform/Win32/Utf8.cpp
            Platform/Win32/WindowImpl.cpp
            Platform/Win32/WindowInput.cpp
            Render/RenderNativeHost.cpp
            Graphics/D2D/Shaders/LivePrimitives.hlsl
            Graphics/D2D/Shaders/LiveTexture.hlsl
            ${XENUI_SHADER_HEADERS}
            "${CMAKE_CURRENT_BINARY_DIR}/XenUI-$<CONFIG>.rc"
    )
    set_source_files_properties(Graphics/D2D/Shaders/LivePrimitives.hlsl Graphics/D2D/Shaders/LiveTexture.hlsl PROPERTIES HEADER_FILE_ONLY TRUE)
elseif (XENUI_PLATFORM STREQUAL "Linux")
    include(XenUILinuxDeps)
    set(XENUI_PLATFORM_SOURCES
            Graphics/GL/AtlasPacker.hpp
            Graphics/GL/CompositorBackend.hpp
            Graphics/GL/CompositorBackend.cpp
            Graphics/GL/Coverage.hpp
            Graphics/GL/Coverage.cpp
            Graphics/GL/FontLibrary.hpp
            Graphics/GL/FontLibrary.cpp
            Graphics/GL/GLApi.hpp
            Graphics/GL/GLApi.cpp
            Graphics/GL/GLObjects.hpp
            Graphics/GL/GLObjects.cpp
            Graphics/GL/GLRenderer.hpp
            Graphics/GL/GLRenderer.cpp
            Graphics/GL/GLShader.hpp
            Graphics/GL/GlyphCache.hpp
            Graphics/GL/GlyphCache.cpp
            Graphics/GL/GraphicsDevice.hpp
            Graphics/GL/GraphicsDevice.cpp
            Graphics/GL/HeadlessContext.hpp
            Graphics/GL/HeadlessContext.cpp
            Graphics/GL/Image.cpp
            Graphics/GL/ImageData.hpp
            Graphics/GL/LayerSurfaces.hpp
            Graphics/GL/LiveRenderer.hpp
            Graphics/GL/LiveRenderer.cpp
            Graphics/GL/Path.cpp
            Graphics/GL/PathData.hpp
            Graphics/GL/SwapChain.hpp
            Graphics/GL/SwapChain.cpp
            Graphics/GL/Text.cpp
            Graphics/GL/TextLayoutData.hpp
            Platform/Linux/AppContext.cpp
            Platform/Linux/Clipboard.cpp
            Platform/Linux/FileDialog.cpp
            Platform/Linux/IconFile.hpp
            Platform/Linux/IconFile.cpp
            Platform/Linux/PopupManager.hpp
            Platform/Linux/PopupManager.cpp
            Platform/Linux/RenderNativeHost.cpp
            Platform/Linux/SdlPlatform.hpp
            Platform/Linux/SdlPlatform.cpp
            Platform/Linux/Shell.cpp
            Platform/Linux/WindowImpl.hpp
            Platform/Linux/WindowImpl.cpp
            Platform/Linux/WindowInput.cpp
    )
endif ()

set(XENUI_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/Generated")
set(XENUI_GENERATED_DIR "${XENUI_GENERATED_DIR}" PARENT_SCOPE)
configure_file("${PROJECT_SOURCE_DIR}/cmake/Version.hpp.in" "${XENUI_GENERATED_DIR}/XenUI/Version.hpp" @ONLY)
if (WIN32)
    configure_file("${PROJECT_SOURCE_DIR}/cmake/XenUI.rc.in" "${CMAKE_CURRENT_BINARY_DIR}/XenUI.rc.configured" @ONLY)
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/XenUI-$<CONFIG>.rc" INPUT "${CMAKE_CURRENT_BINARY_DIR}/XenUI.rc.configured")
endif ()

add_library(XenUI SHARED
        Api.hpp
        XenUI.hpp
        Pch.hpp
        Version.cpp
        "${XENUI_GENERATED_DIR}/XenUI/Version.hpp"
        App.hpp
        App.cpp
        AppContext.hpp
        AppContext.cpp
        Core/Animation.hpp
        Core/Animation.cpp
        Core/Color.hpp
        Core/Concurrent.hpp
        Core/Error.hpp
        Core/Math.hpp
        Core/Matrix.hpp
        Core/SlabPool.hpp
        Core/SlabPool.cpp
        Core/Timer.hpp
        Core/Timer.cpp
        Core/FrameStats.hpp
        Core/FrameStats.cpp
        Graphics/Brush.hpp
        Graphics/Compositor.hpp
        Graphics/Compositor.cpp
        Graphics/CompositorBackend.hpp
        Graphics/GraphicsDevice.hpp
        Graphics/DamageRegion.hpp
        Graphics/DamageRegion.cpp
        Graphics/DeviceLost.hpp
        Graphics/DisplayList.hpp
        Graphics/Image.hpp
        Graphics/Layer.hpp
        Graphics/LiveCanvas.hpp
        Graphics/LiveCanvas.cpp
        Graphics/LiveRecording.hpp
        Graphics/LayerImpl.hpp
        Graphics/Painter.hpp
        Graphics/Painter.cpp
        Graphics/Path.cpp
        Graphics/Layer.cpp
        Graphics/Path.hpp
        Graphics/SvgIcon.hpp
        Graphics/SvgIcon.cpp
        Graphics/StrokeStyle.hpp
        Graphics/Text.hpp
        Input/DragDrop.hpp
        Input/Events.hpp
        Input/Focus.hpp
        Input/Focus.cpp
        Input/InputDispatcher.hpp
        Input/InputDispatcher.cpp
        Input/Shortcuts.hpp
        Input/Shortcuts.cpp
        Input/TextEditing.hpp
        Input/TextEditing.cpp
        Input/TextEditingController.hpp
        Input/TextEditingController.cpp
        Input/TextInput.hpp
        Layout/BoxConstraints.hpp
        Reactive/Reactive.hpp
        Reactive/Reactive.cpp
        Reactive/Signal.hpp
        Render/RenderObject.hpp
        Render/RenderObject.cpp
        Render/RenderBox.hpp
        Render/RenderBox.cpp
        Render/RenderControls.hpp
        Render/RenderControls.cpp
        Render/RenderEditable.hpp
        Render/RenderEditable.cpp
        Render/RenderInput.hpp
        Render/RenderInput.cpp
        Render/RenderLayout.hpp
        Render/RenderNativeHost.hpp
        Render/RenderWindowChrome.hpp
        Render/RenderWindowChrome.cpp
        Render/RenderLive.hpp
        Render/RenderLive.cpp
        Render/RenderLayout.cpp
        Render/RenderOverlay.hpp
        Render/RenderOverlay.cpp
        Render/RenderScroll.hpp
        Render/RenderScroll.cpp
        Render/RenderVisual.hpp
        Render/RenderVisual.cpp
        Tree/Component.hpp
        Tree/Component.cpp
        Tree/Element.hpp
        Tree/Element.cpp
        Tree/ElementTree.hpp
        Tree/ElementTree.cpp
        Tree/Key.hpp
        Tree/Node.hpp
        Tree/Node.cpp
        Tree/Provide.hpp
        Tree/RenderElement.hpp
        Tree/Widget.hpp
        Tree/WidgetArena.hpp
        Tree/WidgetArena.cpp
        Platform/Clipboard.hpp
        Platform/FileDialog.hpp
        Platform/Shell.hpp
        Platform/Window.hpp
        Platform/Window.cpp
        Platform/WindowCore.hpp
        Platform/WindowCore.cpp
        Widgets/Basic.hpp
        Widgets/Controls.hpp
        Widgets/Controls.cpp
        Widgets/Radio.hpp
        Widgets/Radio.cpp
        Widgets/Progress.hpp
        Widgets/Progress.cpp
        Widgets/Tabs.hpp
        Widgets/Tabs.cpp
        Widgets/NumberField.hpp
        Widgets/NumberField.cpp
        Widgets/Dialog.hpp
        Widgets/Dialog.cpp
        Widgets/MenuBar.hpp
        Widgets/MenuBar.cpp
        Widgets/TreeView.hpp
        Widgets/TreeView.cpp
        Widgets/Table.hpp
        Widgets/Table.cpp
        Widgets/Hyperlink.hpp
        Widgets/Hyperlink.cpp
        Widgets/DeveloperTools.hpp
        Widgets/DeveloperTools.cpp
        Widgets/DragDrop.hpp
        Widgets/DragDrop.cpp
        Widgets/Input.hpp
        Widgets/Live.hpp
        Widgets/NativeHost.hpp
        Widgets/WindowChrome.hpp
        Widgets/TitleBar.hpp
        Widgets/TitleBar.cpp
        Widgets/Semantics.hpp
        Render/Semantics.hpp
        Render/Semantics.cpp
        Widgets/WindowChrome.cpp
        Widgets/Overlay.hpp
        Widgets/Overlay.cpp
        Widgets/Scroll.hpp
        Widgets/Scroll.cpp
        Widgets/TextField.hpp
        Widgets/TextField.cpp
        Widgets/Theme.hpp
        Widgets/Theme.cpp
        Widgets/WidgetHost.hpp
        Widgets/WidgetHost.cpp
        ${XENUI_PLATFORM_SOURCES}
)

add_library(XenUI::XenUI ALIAS XenUI)

XenUI_ConfigureTarget(XenUI)

set_target_properties(XenUI PROPERTIES
        DEBUG_POSTFIX d
        VERSION "${PROJECT_VERSION}"
)
target_compile_features(XenUI PUBLIC cxx_std_20)
target_include_directories(XenUI PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>"
        "$<BUILD_INTERFACE:${XENUI_GENERATED_DIR}>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_compile_definitions(XenUI PRIVATE XENUI_EXPORTS)
target_precompile_headers(XenUI PRIVATE Pch.hpp)

if (XENUI_PLATFORM STREQUAL "Win32")
    target_include_directories(XenUI PRIVATE "${XENUI_SHADER_DIR}")
    target_link_libraries(XenUI PRIVATE
            d3d11
            dxgi
            d2d1
            dwrite
            dwmapi
            dxguid
            ole32
            windowscodecs
            imm32
            shell32
            uiautomationcore
    )
elseif (XENUI_PLATFORM STREQUAL "Linux")
    target_include_directories(XenUI SYSTEM PRIVATE "${XENUI_STB_DIR}")
    target_link_libraries(XenUI PRIVATE
            Fontconfig::Fontconfig
            XenUI::HarfBuzz
            XenUI::Unibreak
            Freetype::Freetype
            SDL3::SDL3
            ${CMAKE_DL_LIBS}
    )
endif ()
