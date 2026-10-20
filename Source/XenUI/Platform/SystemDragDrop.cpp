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

#include "XenUI/Platform/SystemDragDrop.hpp"
#include "XenUI/AppContext.hpp"
#include "XenUI/Platform/Utf8.hpp"
#include "XenUI/Platform/WindowImpl.hpp"

#include <ole2.h>
#include <ShlObj.h>
#include <shellapi.h>
#include <wrl/client.h>

#include <atomic>
#include <cstring>

namespace XenUI {
    namespace {
        using Microsoft::WRL::ComPtr;

        std::shared_ptr<const DragData> ActiveDrag;

        CLIPFORMAT GetInternalFormat() {
            static const auto Format = static_cast<CLIPFORMAT>(RegisterClipboardFormatW(L"XenUI.InternalDrag"));
            return Format;
        }

        DWORD ToOle(DropEffect Effect) {
            DWORD Result = DROPEFFECT_NONE;
            if (Allows(Effect, DropEffect::Copy)) { Result |= DROPEFFECT_COPY; }
            if (Allows(Effect, DropEffect::Move)) { Result |= DROPEFFECT_MOVE; }
            if (Allows(Effect, DropEffect::Link)) { Result |= DROPEFFECT_LINK; }
            return Result;
        }

        DropEffect FromOle(DWORD Effect) {
            DropEffect Result = DropEffect::None;
            if ((Effect & DROPEFFECT_COPY) != 0) { Result = Result | DropEffect::Copy; }
            if ((Effect & DROPEFFECT_MOVE) != 0) { Result = Result | DropEffect::Move; }
            if ((Effect & DROPEFFECT_LINK) != 0) { Result = Result | DropEffect::Link; }
            return Result;
        }

        KeyModifiers FromKeyState(DWORD KeyState) {
            KeyModifiers Result = KeyModifiers::None;
            if ((KeyState & MK_SHIFT) != 0) { Result = Result | KeyModifiers::Shift; }
            if ((KeyState & MK_CONTROL) != 0) { Result = Result | KeyModifiers::Control; }
            if ((KeyState & MK_ALT) != 0) { Result = Result | KeyModifiers::Alt; }
            return Result;
        }

        bool HasFormat(IDataObject* Object, CLIPFORMAT Format) {
            FORMATETC Request {Format, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
            return Object->QueryGetData(&Request) == S_OK;
        }

        DragData ReadDataObject(IDataObject* Object) {
            if (ActiveDrag && HasFormat(Object, GetInternalFormat())) { return *ActiveDrag; }

            DragData Data {.External = true};
            FORMATETC Files {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
            STGMEDIUM Medium {};
            if (SUCCEEDED(Object->GetData(&Files, &Medium))) {
                if (auto* Drop = static_cast<HDROP>(GlobalLock(Medium.hGlobal))) {
                    const UINT Count = DragQueryFileW(Drop, 0xFFFFFFFF, nullptr, 0);
                    for (UINT i = 0; i < Count; ++i) {
                        std::wstring Path(DragQueryFileW(Drop, i, nullptr, 0), L'\0');
                        DragQueryFileW(Drop, i, Path.data(), static_cast<UINT>(Path.size() + 1));
                        Data.Files.emplace_back(Path);
                    }
                    GlobalUnlock(Medium.hGlobal);
                }
                ReleaseStgMedium(&Medium);
            }
            FORMATETC Text {CF_UNICODETEXT, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
            if (SUCCEEDED(Object->GetData(&Text, &Medium))) {
                if (const auto* Wide = static_cast<const wchar_t*>(GlobalLock(Medium.hGlobal))) {
                    Data.Text =
                      Narrow(std::wstring_view(Wide, wcsnlen(Wide, GlobalSize(Medium.hGlobal) / sizeof(wchar_t))));
                    GlobalUnlock(Medium.hGlobal);
                }
                ReleaseStgMedium(&Medium);
            }
            return Data;
        }

        HGLOBAL CopyToGlobal(const void* Bytes, size_t Size) {
            HGLOBAL Memory = GlobalAlloc(GMEM_MOVEABLE, Size);
            if (Memory == nullptr) { return nullptr; }
            if (void* Target = GlobalLock(Memory)) {
                std::memcpy(Target, Bytes, Size);
                GlobalUnlock(Memory);
            }
            return Memory;
        }

        void Store(IDataObject* Object, CLIPFORMAT Format, HGLOBAL Memory) {
            if (Memory == nullptr) { return; }
            FORMATETC Description {Format, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
            STGMEDIUM Medium {.tymed = TYMED_HGLOBAL, .hGlobal = Memory};
            if (FAILED(Object->SetData(&Description, &Medium, TRUE))) { GlobalFree(Memory); }
        }

        HGLOBAL MakeFileList(const std::vector<std::filesystem::path>& Files) {
            std::wstring Paths;
            for (const std::filesystem::path& File : Files) {
                Paths += File.wstring();
                Paths.push_back(L'\0');
            }
            Paths.push_back(L'\0');
            std::vector<uint8_t> Bytes(sizeof(DROPFILES) + Paths.size() * sizeof(wchar_t));
            DROPFILES Header {.pFiles = sizeof(DROPFILES), .fWide = TRUE};
            std::memcpy(Bytes.data(), &Header, sizeof(Header));
            std::memcpy(Bytes.data() + sizeof(Header), Paths.data(), Paths.size() * sizeof(wchar_t));
            return CopyToGlobal(Bytes.data(), Bytes.size());
        }

        void PumpFrames() {
            if (AppContext* Context = AppContext::TryGet()) { Context->RenderReadyWindows(); }
        }

        class DropSource final : public IDropSource {
        public:
            HRESULT STDMETHODCALLTYPE QueryInterface(REFIID Interface, void** Object) override {
                if (Interface == __uuidof(IUnknown) || Interface == __uuidof(IDropSource)) {
                    *Object = static_cast<IDropSource*>(this);
                    AddRef();
                    return S_OK;
                }
                *Object = nullptr;
                return E_NOINTERFACE;
            }

            ULONG STDMETHODCALLTYPE AddRef() override { return ++_References; }

            ULONG STDMETHODCALLTYPE Release() override {
                const ULONG Remaining = --_References;
                if (Remaining == 0) { delete this; }
                return Remaining;
            }

            HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL EscapePressed, DWORD KeyState) override {
                if (EscapePressed != FALSE) { return DRAGDROP_S_CANCEL; }
                if ((KeyState & (MK_LBUTTON | MK_RBUTTON)) == 0) { return DRAGDROP_S_DROP; }
                PumpFrames();
                return S_OK;
            }

            HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD Effect) override {
                (void)Effect;
                return DRAGDROP_S_USEDEFAULTCURSORS;
            }

        private:
            std::atomic<ULONG> _References = 1;
        };

        class DropTarget final : public IDropTarget {
        public:
            explicit DropTarget(WindowImpl& Window) : _Window(Window) {}

            HRESULT STDMETHODCALLTYPE QueryInterface(REFIID Interface, void** Object) override {
                if (Interface == __uuidof(IUnknown) || Interface == __uuidof(IDropTarget)) {
                    *Object = static_cast<IDropTarget*>(this);
                    AddRef();
                    return S_OK;
                }
                *Object = nullptr;
                return E_NOINTERFACE;
            }

            ULONG STDMETHODCALLTYPE AddRef() override { return ++_References; }

            ULONG STDMETHODCALLTYPE Release() override {
                const ULONG Remaining = --_References;
                if (Remaining == 0) { delete this; }
                return Remaining;
            }

            HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* Object,
                                                DWORD KeyState,
                                                POINTL Point,
                                                DWORD* Effect) override {
                _Data    = ReadDataObject(Object);
                _Allowed = FromOle(*Effect);
                return DragOver(KeyState, Point, Effect);
            }

            HRESULT STDMETHODCALLTYPE DragOver(DWORD KeyState, POINTL Point, DWORD* Effect) override {
                InputDispatcher* Input  = _Window.GetInput();
                const DropEffect Result = Input != nullptr
                                            ? Input->DragOverExternal(_Window.ScreenToDips({Point.x, Point.y}),
                                                                      _Data,
                                                                      FromKeyState(KeyState),
                                                                      _Allowed)
                                            : DropEffect::None;
                *Effect                 = ToOle(Result);
                PumpFrames();
                return S_OK;
            }

            HRESULT STDMETHODCALLTYPE DragLeave() override {
                if (InputDispatcher* Input = _Window.GetInput()) { Input->DragLeaveExternal(); }
                _Data = {};
                return S_OK;
            }

            HRESULT STDMETHODCALLTYPE Drop(IDataObject* Object, DWORD KeyState, POINTL Point, DWORD* Effect) override {
                (void)Object;
                InputDispatcher* Input  = _Window.GetInput();
                const DropEffect Result = Input != nullptr
                                            ? Input->DropExternal(_Window.ScreenToDips({Point.x, Point.y}),
                                                                  _Data,
                                                                  FromKeyState(KeyState),
                                                                  _Allowed)
                                            : DropEffect::None;
                *Effect                 = ToOle(Result);
                _Data                   = {};
                return S_OK;
            }

        private:
            WindowImpl& _Window;
            DragData _Data;
            DropEffect _Allowed            = DropEffect::Copy;
            std::atomic<ULONG> _References = 1;
        };
    }  // namespace

    struct SystemDropRegistration::Impl {
        HWND Hwnd = nullptr;
        ComPtr<IDropTarget> Target;
    };

    SystemDropRegistration::SystemDropRegistration(WindowImpl& Window) : _Impl(std::make_unique<Impl>()) {
        _Impl->Hwnd = Window.GetHwnd();
        _Impl->Target.Attach(new DropTarget(Window));
        if (FAILED(RegisterDragDrop(_Impl->Hwnd, _Impl->Target.Get()))) { _Impl->Hwnd = nullptr; }
    }

    SystemDropRegistration::~SystemDropRegistration() {
        if (_Impl->Hwnd != nullptr) { RevokeDragDrop(_Impl->Hwnd); }
    }

    DropEffect RunSystemDrag(const DragData& Data, DropEffect Allowed) {
        ComPtr<IDataObject> Object;
        if (FAILED(SHCreateDataObject(nullptr, 0, nullptr, nullptr, IID_PPV_ARGS(&Object)))) {
            return DropEffect::None;
        }

        if (!Data.Files.empty()) { Store(Object.Get(), CF_HDROP, MakeFileList(Data.Files)); }
        if (Data.Text) {
            const std::wstring Wide = Widen(*Data.Text);
            Store(Object.Get(), CF_UNICODETEXT, CopyToGlobal(Wide.c_str(), (Wide.size() + 1) * sizeof(wchar_t)));
        }
        const uint8_t Marker = 1;
        Store(Object.Get(), GetInternalFormat(), CopyToGlobal(&Marker, sizeof(Marker)));

        ActiveDrag = std::make_shared<const DragData>(Data);
        ComPtr<IDropSource> Source;
        Source.Attach(new DropSource());
        DWORD Effect         = DROPEFFECT_NONE;
        const HRESULT Result = DoDragDrop(Object.Get(), Source.Get(), ToOle(Allowed), &Effect);
        ActiveDrag.reset();

        if (Result != DRAGDROP_S_DROP) { return DropEffect::None; }
        const DropEffect Dropped = FromOle(Effect);
        for (const DropEffect Candidate : {DropEffect::Move, DropEffect::Copy, DropEffect::Link}) {
            if (Allows(Dropped, Candidate)) { return Candidate; }
        }
        return DropEffect::None;
    }

    bool IsSystemDragActive() {
        return ActiveDrag != nullptr;
    }
}  // namespace XenUI
