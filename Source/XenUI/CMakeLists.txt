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

XenUI_CompileShader(Graphics/Shaders/LivePrimitives.hlsl VsMain vs_4_0 kLivePrimitivesVertexShader LivePrimitivesVS.h)
XenUI_CompileShader(Graphics/Shaders/LivePrimitives.hlsl PsMain ps_4_0 kLivePrimitivesPixelShader LivePrimitivesPS.h)
XenUI_CompileShader(Graphics/Shaders/LiveTexture.hlsl VsMain vs_4_0 kLiveTextureVertexShader LiveTextureVS.h)
XenUI_CompileShader(Graphics/Shaders/LiveTexture.hlsl PsMain ps_4_0 kLiveTexturePixelShader LiveTexturePS.h)

set(XENUI_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/Generated")
set(XENUI_GENERATED_DIR "${XENUI_GENERATED_DIR}" PARENT_SCOPE)
configure_file("${PROJECT_SOURCE_DIR}/cmake/Version.hpp.in" "${XENUI_GENERATED_DIR}/XenUI/Version.hpp" @ONLY)
configure_file("${PROJECT_SOURCE_DIR}/cmake/XenUI.rc.in" "${CMAKE_CURRENT_BINARY_DIR}/XenUI.rc.configured" @ONLY)
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/XenUI-$<CONFIG>.rc" INPUT "${CMAKE_CURRENT_BINARY_DIR}/XenUI.rc.configured")

add_library(XenUI SHARED
        Api.hpp
        XenUI.hpp
        Pch.hpp
        Version.cpp
        "${XENUI_GENERATED_DIR}/XenUI/Version.hpp"
        "${CMAKE_CURRENT_BINARY_DIR}/XenUI-$<CONFIG>.rc"
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
        Graphics/D2DConvert.hpp
        Graphics/D2DRenderer.hpp
        Graphics/D2DRenderer.cpp
        Graphics/DamageRegion.hpp
        Graphics/DamageRegion.cpp
        Graphics/DeviceLost.hpp
        Graphics/DisplayList.hpp
        Graphics/GraphicsDevice.hpp
        Graphics/GraphicsDevice.cpp
        Graphics/Image.hpp
        Graphics/Image.cpp
        Graphics/ImageData.hpp
        Graphics/Layer.hpp
        Graphics/LiveCanvas.hpp
        Graphics/LiveCanvas.cpp
        Graphics/LiveRecording.hpp
        Graphics/LiveRenderer.hpp
        Graphics/LiveRenderer.cpp
        Graphics/Layer.cpp
        Graphics/LayerImpl.hpp
        Graphics/Painter.hpp
        Graphics/Painter.cpp
        Graphics/Path.hpp
        Graphics/Path.cpp
        Graphics/PathData.hpp
        Graphics/StrokeStyle.hpp
        Graphics/SwapChain.hpp
        Graphics/SwapChain.cpp
        Graphics/Text.hpp
        Graphics/Text.cpp
        Graphics/TextLayoutData.hpp
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
        Render/RenderNativeHost.cpp
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
        Platform/Clipboard.cpp
        Platform/HResult.hpp
        Platform/PopupManager.hpp
        Platform/PopupManager.cpp
        Platform/SystemDragDrop.hpp
        Platform/SystemDragDrop.cpp
        Platform/Utf8.hpp
        Platform/Utf8.cpp
        Platform/Window.hpp
        Platform/Window.cpp
        Platform/WindowImpl.hpp
        Platform/WindowImpl.cpp
        Platform/WindowInput.cpp
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
        Widgets/DeveloperTools.hpp
        Widgets/DeveloperTools.cpp
        Widgets/DragDrop.hpp
        Widgets/DragDrop.cpp
        Widgets/Input.hpp
                Widgets/Live.hpp
                Widgets/NativeHost.hpp
        Widgets/WindowChrome.hpp
        Widgets/Semantics.hpp
        Render/Semantics.hpp
        Render/Semantics.cpp
        Render/RenderWindowChrome.cpp
        Platform/Accessibility.hpp
        Platform/Accessibility.cpp
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
        Graphics/Shaders/LivePrimitives.hlsl
        Graphics/Shaders/LiveTexture.hlsl
        ${XENUI_SHADER_HEADERS}
)

set_source_files_properties(Graphics/Shaders/LivePrimitives.hlsl Graphics/Shaders/LiveTexture.hlsl PROPERTIES HEADER_FILE_ONLY TRUE)

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
target_include_directories(XenUI PRIVATE "${XENUI_SHADER_DIR}")
target_compile_definitions(XenUI PRIVATE XENUI_EXPORTS)
target_precompile_headers(XenUI PRIVATE Pch.hpp)

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
