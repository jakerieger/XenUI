// Copyright 2026 Jake Rieger
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "XenUI/Platform/Window.hpp"
#include "XenUI/Platform/WindowImpl.hpp"

namespace XenUI {
    Window::Window(const WindowDesc& Desc) : _Impl(std::make_unique<WindowImpl>(Desc)) {}

    Window::~Window() = default;

    Window::Window(Window&&) noexcept            = default;
    Window& Window::operator=(Window&&) noexcept = default;

    void Window::Show() {
        _Impl->Show();
    }

    void Window::Maximize() {
        _Impl->Maximize();
    }

    void Window::Close() {
        _Impl->Close();
    }

    void Window::SetTitle(std::string_view Title) {
        _Impl->SetTitle(Title);
    }

    void Window::SetClientSize(const Size& ClientSize) {
        _Impl->SetClientSize(ClientSize);
    }

    void Window::SetDarkTitleBar(bool Dark) {
        _Impl->SetDarkTitleBar(Dark);
    }

    void Window::Activate() {
        _Impl->Activate();
    }

    void Window::SetBackground(const Color& NewBackground) {
        _Impl->SetBackground(NewBackground);
    }

    void Window::SetContent(const Node& Root) {
        _Impl->SetContent(Root);
    }

    void Window::SetRootLayer(std::shared_ptr<Layer> Root) {
        _Impl->SetRootLayer(std::move(Root));
    }

    const std::shared_ptr<Layer>& Window::GetRootLayer() const {
        return _Impl->GetRootLayer();
    }

    void Window::RequestFrame() {
        _Impl->RequestFrame();
    }

    void Window::SetFrameCallback(FrameCallback Callback) {
        _Impl->SetFrameCallback(std::move(Callback));
    }

    void Window::SetOnClosed(std::function<void()> Callback) {
        _Impl->SetOnClosed(std::move(Callback));
    }

    void Window::SetShortcuts(std::vector<Shortcut> Shortcuts) {
        _Impl->SetShortcuts(std::move(Shortcuts));
    }

    void Window::SetShowDamageRegions(bool Enabled) {
        _Impl->SetShowDamageRegions(Enabled);
    }

    bool Window::IsOpen() const {
        return _Impl && _Impl->IsOpen();
    }

    Size Window::GetClientSize() const {
        return _Impl->GetClientSize();
    }

    float Window::GetDpiScale() const {
        return _Impl->GetDpiScale();
    }

    void* Window::GetNativeHandle() const {
        return _Impl->GetHwnd();
    }
}  // namespace XenUI
