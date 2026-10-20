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

#include "XenUI/Platform/Accessibility.hpp"
#include "XenUI/Platform/Utf8.hpp"
#include "XenUI/Platform/WindowImpl.hpp"
#include "XenUI/Render/Semantics.hpp"
#include "XenUI/Widgets/WidgetHost.hpp"

#include <UIAutomation.h>
#include <wrl/client.h>

#include <atomic>
#include <cmath>
#include <unordered_map>

using Microsoft::WRL::ComPtr;

namespace XenUI {
    class ElementProvider;

    struct AccessibilityCore {
        WindowImpl* Window = nullptr;
        HWND Hwnd          = nullptr;
        SemanticsTree Tree;
        std::unordered_map<uint64_t, ComPtr<ElementProvider>> Providers;
        std::weak_ptr<AccessibilityCore> Self;
        bool Connected = false;
        bool Dirty     = true;

        [[nodiscard]] ComPtr<ElementProvider> GetProvider(uint64_t Id);
        SemanticsUpdate Rebuild();
        void EnsureFresh() {
            if (Dirty) { Rebuild(); }
        }
        void RaiseEvents(const SemanticsUpdate& Update);
        [[nodiscard]] bool OwnsForeground() const;
        [[nodiscard]] UiaRect ToScreen(const Rect& Bounds) const;
    };

    namespace {
        long ControlTypeOf(SemanticsRole Role) {
            switch (Role) {
                case SemanticsRole::Pane:
                    return UIA_PaneControlTypeId;
                case SemanticsRole::Text:
                    return UIA_TextControlTypeId;
                case SemanticsRole::Image:
                    return UIA_ImageControlTypeId;
                case SemanticsRole::Button:
                case SemanticsRole::Switch:
                    return UIA_ButtonControlTypeId;
                case SemanticsRole::CheckBox:
                    return UIA_CheckBoxControlTypeId;
                case SemanticsRole::Slider:
                    return UIA_SliderControlTypeId;
                case SemanticsRole::Edit:
                    return UIA_EditControlTypeId;
                case SemanticsRole::ComboBox:
                    return UIA_ComboBoxControlTypeId;
                case SemanticsRole::List:
                    return UIA_ListControlTypeId;
                case SemanticsRole::ListItem:
                    return UIA_ListItemControlTypeId;
                case SemanticsRole::Menu:
                    return UIA_MenuControlTypeId;
                case SemanticsRole::MenuItem:
                    return UIA_MenuItemControlTypeId;
                case SemanticsRole::ToolBar:
                    return UIA_ToolBarControlTypeId;
                default:
                    return UIA_GroupControlTypeId;
            }
        }

        void SetString(VARIANT& Result, const std::string& Text) {
            Result.vt      = VT_BSTR;
            Result.bstrVal = SysAllocString(Widen(Text).c_str());
        }

        void SetBool(VARIANT& Result, bool Value) {
            Result.vt      = VT_BOOL;
            Result.boolVal = Value ? VARIANT_TRUE : VARIANT_FALSE;
        }

        void SetInt(VARIANT& Result, long Value) {
            Result.vt   = VT_I4;
            Result.lVal = Value;
        }

        ToggleState ToToggleState(const std::optional<bool>& Toggled) {
            return Toggled.value_or(false) ? ToggleState_On : ToggleState_Off;
        }

        ExpandCollapseState ToExpandState(const std::optional<bool>& Expanded) {
            return Expanded.value_or(false) ? ExpandCollapseState_Expanded : ExpandCollapseState_Collapsed;
        }
    }  // namespace

    /// @brief One node of the tree as UI Automation sees it. Identified by node id, so it outlives rebuilds and
    /// reports itself unavailable once its node is gone.
    class ElementProvider final : public IRawElementProviderSimple,
                                  public IRawElementProviderFragment,
                                  public IRawElementProviderFragmentRoot,
                                  public IInvokeProvider,
                                  public IToggleProvider,
                                  public IValueProvider,
                                  public IExpandCollapseProvider,
                                  public ISelectionItemProvider {
    public:
        ElementProvider(std::weak_ptr<AccessibilityCore> Core, uint64_t Id) : _Core(std::move(Core)), _Id(Id) {}

        [[nodiscard]] uint64_t GetId() const { return _Id; }
        [[nodiscard]] bool IsRoot() const { return _Id == SemanticsTree::kRootId; }

        // IUnknown
        IFACEMETHODIMP_(ULONG) AddRef() override { return ++_References; }

        IFACEMETHODIMP_(ULONG) Release() override {
            const ULONG Remaining = --_References;
            if (Remaining == 0) { delete this; }
            return Remaining;
        }

        IFACEMETHODIMP QueryInterface(REFIID Riid, void** Result) override {
            if (Result == nullptr) { return E_POINTER; }
            *Result = nullptr;
            if (Riid == __uuidof(IUnknown) || Riid == __uuidof(IRawElementProviderSimple)) {
                *Result = static_cast<IRawElementProviderSimple*>(this);
            } else if (Riid == __uuidof(IRawElementProviderFragment)) {
                *Result = static_cast<IRawElementProviderFragment*>(this);
            } else if (Riid == __uuidof(IRawElementProviderFragmentRoot) && IsRoot()) {
                *Result = static_cast<IRawElementProviderFragmentRoot*>(this);
            } else if (Riid == __uuidof(IInvokeProvider)) {
                *Result = static_cast<IInvokeProvider*>(this);
            } else if (Riid == __uuidof(IToggleProvider)) {
                *Result = static_cast<IToggleProvider*>(this);
            } else if (Riid == __uuidof(IValueProvider)) {
                *Result = static_cast<IValueProvider*>(this);
            } else if (Riid == __uuidof(IExpandCollapseProvider)) {
                *Result = static_cast<IExpandCollapseProvider*>(this);
            } else if (Riid == __uuidof(ISelectionItemProvider)) {
                *Result = static_cast<ISelectionItemProvider*>(this);
            } else {
                return E_NOINTERFACE;
            }
            AddRef();
            return S_OK;
        }

        // IRawElementProviderSimple
        IFACEMETHODIMP get_ProviderOptions(ProviderOptions* Result) override {
            if (Result == nullptr) { return E_POINTER; }
            *Result = ProviderOptions_ServerSideProvider | ProviderOptions_UseComThreading;
            return S_OK;
        }

        IFACEMETHODIMP GetPatternProvider(PATTERNID Pattern, IUnknown** Result) override;
        IFACEMETHODIMP GetPropertyValue(PROPERTYID Property, VARIANT* Result) override;

        IFACEMETHODIMP get_HostRawElementProvider(IRawElementProviderSimple** Result) override {
            if (Result == nullptr) { return E_POINTER; }
            *Result                                       = nullptr;
            const std::shared_ptr<AccessibilityCore> Core = _Core.lock();
            if (!Core) { return UIA_E_ELEMENTNOTAVAILABLE; }
            return IsRoot() ? UiaHostProviderFromHwnd(Core->Hwnd, Result) : S_OK;
        }

        // IRawElementProviderFragment
        IFACEMETHODIMP Navigate(NavigateDirection Direction, IRawElementProviderFragment** Result) override;

        IFACEMETHODIMP GetRuntimeId(SAFEARRAY** Result) override {
            if (Result == nullptr) { return E_POINTER; }
            *Result = nullptr;
            if (IsRoot()) { return S_OK; }
            const int Parts[] = {UiaAppendRuntimeId, static_cast<int>(_Id & 0xFFFFFFFF), static_cast<int>(_Id >> 32)};
            SAFEARRAY* Array  = SafeArrayCreateVector(VT_I4, 0, 3);
            if (Array == nullptr) { return E_OUTOFMEMORY; }
            for (LONG i = 0; i < 3; ++i) {
                SafeArrayPutElement(Array, &i, const_cast<int*>(&Parts[i]));
            }
            *Result = Array;
            return S_OK;
        }

        IFACEMETHODIMP get_BoundingRectangle(UiaRect* Result) override {
            if (Result == nullptr) { return E_POINTER; }
            *Result                                       = {};
            const SemanticsNode* Node                     = nullptr;
            const std::shared_ptr<AccessibilityCore> Core = Lookup(Node);
            if (!Core) { return UIA_E_ELEMENTNOTAVAILABLE; }
            if (IsRoot()) { return S_OK; }
            *Result = Core->ToScreen(Node->VisibleBounds.IsEmpty() ? Node->Bounds : Node->VisibleBounds);
            return S_OK;
        }

        IFACEMETHODIMP GetEmbeddedFragmentRoots(SAFEARRAY** Result) override {
            if (Result == nullptr) { return E_POINTER; }
            *Result = nullptr;
            return S_OK;
        }

        IFACEMETHODIMP SetFocus() override {
            return Act([](const SemanticsTree& Tree, uint64_t Id) { return Tree.Focus(Id); });
        }

        IFACEMETHODIMP get_FragmentRoot(IRawElementProviderFragmentRoot** Result) override {
            if (Result == nullptr) { return E_POINTER; }
            *Result                                       = nullptr;
            const std::shared_ptr<AccessibilityCore> Core = _Core.lock();
            if (!Core) { return UIA_E_ELEMENTNOTAVAILABLE; }
            ComPtr<ElementProvider> Root = Core->GetProvider(SemanticsTree::kRootId);
            *Result                      = static_cast<IRawElementProviderFragmentRoot*>(Root.Detach());
            return S_OK;
        }

        // IRawElementProviderFragmentRoot
        IFACEMETHODIMP ElementProviderFromPoint(double X, double Y, IRawElementProviderFragment** Result) override {
            if (Result == nullptr) { return E_POINTER; }
            *Result                                       = nullptr;
            const std::shared_ptr<AccessibilityCore> Core = _Core.lock();
            if (!Core) { return UIA_E_ELEMENTNOTAVAILABLE; }
            Core->EnsureFresh();
            const Vec2 Position =
              Core->Window->ScreenToDips({static_cast<LONG>(std::lround(X)), static_cast<LONG>(std::lround(Y))});
            const uint64_t Hit = Core->Tree.HitTest(Position);
            *Result            = static_cast<IRawElementProviderFragment*>(Core->GetProvider(Hit).Detach());
            return S_OK;
        }

        IFACEMETHODIMP GetFocus(IRawElementProviderFragment** Result) override {
            if (Result == nullptr) { return E_POINTER; }
            *Result                                       = nullptr;
            const std::shared_ptr<AccessibilityCore> Core = _Core.lock();
            if (!Core) { return UIA_E_ELEMENTNOTAVAILABLE; }
            Core->EnsureFresh();
            const uint64_t Focused = Core->Tree.GetFocused();
            if (Focused != SemanticsTree::kRootId) {
                *Result = static_cast<IRawElementProviderFragment*>(Core->GetProvider(Focused).Detach());
            }
            return S_OK;
        }

        // IInvokeProvider
        IFACEMETHODIMP Invoke() override {
            const HRESULT Result = Act([](const SemanticsTree& Tree, uint64_t Id) { return Tree.Invoke(Id); });
            if (SUCCEEDED(Result)) { UiaRaiseAutomationEvent(this, UIA_Invoke_InvokedEventId); }
            return Result;
        }

        // IToggleProvider
        IFACEMETHODIMP Toggle() override {
            return Act([](const SemanticsTree& Tree, uint64_t Id) { return Tree.Toggle(Id); });
        }

        IFACEMETHODIMP get_ToggleState(ToggleState* Result) override {
            if (Result == nullptr) { return E_POINTER; }
            const SemanticsNode* Node = nullptr;
            if (!Lookup(Node)) { return UIA_E_ELEMENTNOTAVAILABLE; }
            *Result = ToToggleState(Node->Properties.Toggled);
            return S_OK;
        }

        // IValueProvider
        IFACEMETHODIMP SetValue(LPCWSTR Value) override {
            const std::string Text = Narrow(Value != nullptr ? Value : L"");
            return Act([&Text](const SemanticsTree& Tree, uint64_t Id) { return Tree.SetValue(Id, Text); });
        }

        IFACEMETHODIMP get_Value(BSTR* Result) override {
            if (Result == nullptr) { return E_POINTER; }
            const SemanticsNode* Node = nullptr;
            if (!Lookup(Node)) { return UIA_E_ELEMENTNOTAVAILABLE; }
            *Result = SysAllocString(Widen(Node->Properties.Value.value_or(std::string())).c_str());
            return S_OK;
        }

        IFACEMETHODIMP get_IsReadOnly(BOOL* Result) override {
            if (Result == nullptr) { return E_POINTER; }
            const SemanticsNode* Node = nullptr;
            if (!Lookup(Node)) { return UIA_E_ELEMENTNOTAVAILABLE; }
            *Result =
              Node->Properties.ReadOnly || !Node->Properties.Enabled || !Node->Properties.OnSetValue ? TRUE : FALSE;
            return S_OK;
        }

        // IExpandCollapseProvider
        IFACEMETHODIMP Expand() override {
            return Act([](const SemanticsTree& Tree, uint64_t Id) { return Tree.SetExpanded(Id, true); });
        }

        IFACEMETHODIMP Collapse() override {
            return Act([](const SemanticsTree& Tree, uint64_t Id) { return Tree.SetExpanded(Id, false); });
        }

        IFACEMETHODIMP get_ExpandCollapseState(ExpandCollapseState* Result) override {
            if (Result == nullptr) { return E_POINTER; }
            const SemanticsNode* Node = nullptr;
            if (!Lookup(Node)) { return UIA_E_ELEMENTNOTAVAILABLE; }
            *Result = ToExpandState(Node->Properties.Expanded);
            return S_OK;
        }

        // ISelectionItemProvider
        IFACEMETHODIMP Select() override {
            return Act([](const SemanticsTree& Tree, uint64_t Id) { return Tree.Select(Id); });
        }

        IFACEMETHODIMP AddToSelection() override { return Select(); }
        IFACEMETHODIMP RemoveFromSelection() override { return UIA_E_INVALIDOPERATION; }

        IFACEMETHODIMP get_IsSelected(BOOL* Result) override {
            if (Result == nullptr) { return E_POINTER; }
            const SemanticsNode* Node = nullptr;
            if (!Lookup(Node)) { return UIA_E_ELEMENTNOTAVAILABLE; }
            *Result = Node->Properties.Selected.value_or(false) ? TRUE : FALSE;
            return S_OK;
        }

        IFACEMETHODIMP get_SelectionContainer(IRawElementProviderSimple** Result) override {
            if (Result == nullptr) { return E_POINTER; }
            *Result                                       = nullptr;
            const SemanticsNode* Node                     = nullptr;
            const std::shared_ptr<AccessibilityCore> Core = Lookup(Node);
            if (!Core) { return UIA_E_ELEMENTNOTAVAILABLE; }
            *Result = static_cast<IRawElementProviderSimple*>(Core->GetProvider(Node->Parent).Detach());
            return S_OK;
        }

        /// @brief The live core and this element's node, or null when either is gone.
        std::shared_ptr<AccessibilityCore> Lookup(const SemanticsNode*& Node) const {
            std::shared_ptr<AccessibilityCore> Core = _Core.lock();
            if (!Core) { return nullptr; }
            Core->EnsureFresh();
            Node = Core->Tree.Find(_Id);
            return Node != nullptr ? Core : nullptr;
        }

    private:
        template<class TAction>
        HRESULT Act(const TAction& Action) {
            const SemanticsNode* Node                     = nullptr;
            const std::shared_ptr<AccessibilityCore> Core = Lookup(Node);
            if (!Core) { return UIA_E_ELEMENTNOTAVAILABLE; }
            if (!Node->Properties.Enabled) { return UIA_E_ELEMENTNOTENABLED; }
            if (!Action(Core->Tree, _Id)) { return UIA_E_INVALIDOPERATION; }
            Core->Dirty = true;
            return S_OK;
        }

        std::weak_ptr<AccessibilityCore> _Core;
        uint64_t _Id                   = 0;
        std::atomic<ULONG> _References = 1;
    };

    /// @brief The range value pattern of an element. Separate because IValueProvider has members of the same name.
    class RangeValueProvider final : public IRangeValueProvider {
    public:
        explicit RangeValueProvider(ComPtr<ElementProvider> Element) : _Element(std::move(Element)) {}

        IFACEMETHODIMP_(ULONG) AddRef() override { return ++_References; }

        IFACEMETHODIMP_(ULONG) Release() override {
            const ULONG Remaining = --_References;
            if (Remaining == 0) { delete this; }
            return Remaining;
        }

        IFACEMETHODIMP QueryInterface(REFIID Riid, void** Result) override {
            if (Result == nullptr) { return E_POINTER; }
            *Result = nullptr;
            if (Riid != __uuidof(IUnknown) && Riid != __uuidof(IRangeValueProvider)) { return E_NOINTERFACE; }
            *Result = static_cast<IRangeValueProvider*>(this);
            AddRef();
            return S_OK;
        }

        IFACEMETHODIMP SetValue(double Value) override {
            const SemanticsNode* Node                     = nullptr;
            const std::shared_ptr<AccessibilityCore> Core = _Element->Lookup(Node);
            if (!Core) { return UIA_E_ELEMENTNOTAVAILABLE; }
            if (!Node->Properties.Enabled) { return UIA_E_ELEMENTNOTENABLED; }
            if (!Core->Tree.SetRangeValue(_Element->GetId(), static_cast<float>(Value))) {
                return UIA_E_INVALIDOPERATION;
            }
            Core->Dirty = true;
            return S_OK;
        }

        IFACEMETHODIMP get_Value(double* Result) override {
            return Read(Result, [](const SemanticsRange& Range) { return Range.Value; });
        }
        IFACEMETHODIMP get_Maximum(double* Result) override {
            return Read(Result, [](const SemanticsRange& Range) { return Range.Max; });
        }
        IFACEMETHODIMP get_Minimum(double* Result) override {
            return Read(Result, [](const SemanticsRange& Range) { return Range.Min; });
        }
        IFACEMETHODIMP get_SmallChange(double* Result) override {
            return Read(Result, [](const SemanticsRange& Range) { return Range.Step; });
        }

        IFACEMETHODIMP get_LargeChange(double* Result) override {
            return Read(Result, [](const SemanticsRange& Range) { return (Range.Max - Range.Min) * 0.1f; });
        }

        IFACEMETHODIMP get_IsReadOnly(BOOL* Result) override {
            if (Result == nullptr) { return E_POINTER; }
            const SemanticsNode* Node = nullptr;
            if (!_Element->Lookup(Node)) { return UIA_E_ELEMENTNOTAVAILABLE; }
            *Result =
              Node->Properties.ReadOnly || !Node->Properties.Enabled || !Node->Properties.OnSetRange ? TRUE : FALSE;
            return S_OK;
        }

    private:
        template<class TField>
        HRESULT Read(double* Result, const TField& Field) const {
            if (Result == nullptr) { return E_POINTER; }
            const SemanticsNode* Node = nullptr;
            if (!_Element->Lookup(Node) || !Node->Properties.Range) { return UIA_E_ELEMENTNOTAVAILABLE; }
            *Result = static_cast<double>(Field(*Node->Properties.Range));
            return S_OK;
        }

        ComPtr<ElementProvider> _Element;
        std::atomic<ULONG> _References = 1;
    };

    HRESULT ElementProvider::GetPatternProvider(PATTERNID Pattern, IUnknown** Result) {
        if (Result == nullptr) { return E_POINTER; }
        *Result                   = nullptr;
        const SemanticsNode* Node = nullptr;
        if (!Lookup(Node)) { return UIA_E_ELEMENTNOTAVAILABLE; }

        const SemanticsProperties& Properties = Node->Properties;
        switch (Pattern) {
            case UIA_InvokePatternId:
                if (Properties.OnInvoke) { *Result = static_cast<IInvokeProvider*>(this); }
                break;
            case UIA_TogglePatternId:
                if (Properties.Toggled) { *Result = static_cast<IToggleProvider*>(this); }
                break;
            case UIA_ValuePatternId:
                if (Properties.Value) { *Result = static_cast<IValueProvider*>(this); }
                break;
            case UIA_ExpandCollapsePatternId:
                if (Properties.Expanded) { *Result = static_cast<IExpandCollapseProvider*>(this); }
                break;
            case UIA_SelectionItemPatternId:
                if (Properties.Selected) { *Result = static_cast<ISelectionItemProvider*>(this); }
                break;
            case UIA_RangeValuePatternId:
                if (Properties.Range) {
                    *Result = new RangeValueProvider(ComPtr<ElementProvider>(this));
                    return S_OK;
                }
                break;
            default:
                break;
        }
        if (*Result != nullptr) { (*Result)->AddRef(); }
        return S_OK;
    }

    HRESULT ElementProvider::GetPropertyValue(PROPERTYID Property, VARIANT* Result) {
        if (Result == nullptr) { return E_POINTER; }
        VariantInit(Result);
        const SemanticsNode* Node                     = nullptr;
        const std::shared_ptr<AccessibilityCore> Core = Lookup(Node);
        if (!Core) { return UIA_E_ELEMENTNOTAVAILABLE; }

        if (Property == UIA_FrameworkIdPropertyId) {
            SetString(*Result, "XenUI");
            return S_OK;
        }
        // The window's own provider supplies the root's name, type and bounds.
        if (IsRoot()) { return S_OK; }

        const SemanticsProperties& Properties = Node->Properties;
        switch (Property) {
            case UIA_ControlTypePropertyId:
                SetInt(*Result, ControlTypeOf(Properties.Role));
                break;
            case UIA_LocalizedControlTypePropertyId:
                if (Properties.Role == SemanticsRole::Switch) { SetString(*Result, "toggle switch"); }
                break;
            case UIA_NamePropertyId:
                if (!Properties.Label.empty()) { SetString(*Result, Properties.Label); }
                break;
            case UIA_HelpTextPropertyId:
                if (!Properties.Description.empty()) { SetString(*Result, Properties.Description); }
                break;
            case UIA_AcceleratorKeyPropertyId:
                if (Properties.Role == SemanticsRole::MenuItem && !Properties.Description.empty()) {
                    SetString(*Result, Properties.Description);
                }
                break;
            case UIA_AutomationIdPropertyId:
                if (!Properties.Identifier.empty()) { SetString(*Result, Properties.Identifier); }
                break;
            case UIA_IsEnabledPropertyId:
                SetBool(*Result, Properties.Enabled);
                break;
            case UIA_IsKeyboardFocusablePropertyId:
                SetBool(*Result, Node->Focusable && Properties.Enabled);
                break;
            case UIA_HasKeyboardFocusPropertyId:
                SetBool(*Result, Core->Tree.GetFocused() == _Id);
                break;
            case UIA_IsOffscreenPropertyId:
                SetBool(*Result, Node->IsOffscreen());
                break;
            case UIA_IsControlElementPropertyId:
            case UIA_IsContentElementPropertyId:
                SetBool(*Result, true);
                break;
            case UIA_ProviderDescriptionPropertyId:
                SetString(*Result, "XenUI semantics");
                break;
            default:
                break;
        }
        return S_OK;
    }

    HRESULT ElementProvider::Navigate(NavigateDirection Direction, IRawElementProviderFragment** Result) {
        if (Result == nullptr) { return E_POINTER; }
        *Result                                       = nullptr;
        const SemanticsNode* Node                     = nullptr;
        const std::shared_ptr<AccessibilityCore> Core = Lookup(Node);
        if (!Core) { return UIA_E_ELEMENTNOTAVAILABLE; }

        std::optional<uint64_t> Target;
        switch (Direction) {
            case NavigateDirection_Parent:
                if (!IsRoot()) { Target = Node->Parent; }
                break;
            case NavigateDirection_FirstChild:
                if (!Node->Children.empty()) { Target = Node->Children.front(); }
                break;
            case NavigateDirection_LastChild:
                if (!Node->Children.empty()) { Target = Node->Children.back(); }
                break;
            case NavigateDirection_NextSibling:
            case NavigateDirection_PreviousSibling: {
                if (IsRoot()) { break; }
                const std::vector<uint64_t>& Siblings = Core->Tree.Find(Node->Parent)->Children;
                const auto Self                       = std::ranges::find(Siblings, _Id);
                if (Self == Siblings.end()) { break; }
                if (Direction == NavigateDirection_NextSibling && Self + 1 != Siblings.end()) { Target = *(Self + 1); }
                if (Direction == NavigateDirection_PreviousSibling && Self != Siblings.begin()) {
                    Target = *(Self - 1);
                }
                break;
            }
            default:
                break;
        }
        if (Target) { *Result = static_cast<IRawElementProviderFragment*>(Core->GetProvider(*Target).Detach()); }
        return S_OK;
    }

    ComPtr<ElementProvider> AccessibilityCore::GetProvider(uint64_t Id) {
        ComPtr<ElementProvider>& Slot = Providers[Id];
        if (!Slot) {
            // The weak pointer is filled in by the bridge, which owns this core through a shared pointer.
            Slot.Attach(new ElementProvider(Self, Id));
        }
        return Slot;
    }

    SemanticsUpdate AccessibilityCore::Rebuild() {
        Dirty            = false;
        WidgetHost* Host = Window->GetContentHost();
        if (Host == nullptr) { return {}; }
        SemanticsUpdate Update = Tree.Rebuild(Host->GetView(), &Host->GetInput().GetFocusManager());
        std::erase_if(Providers, [this](const auto& Entry) { return Tree.Find(Entry.first) == nullptr; });
        return Update;
    }

    bool AccessibilityCore::OwnsForeground() const {
        const HWND Foreground = GetForegroundWindow();
        return Foreground == Hwnd || GetWindow(Hwnd, GW_OWNER) == Foreground;
    }

    UiaRect AccessibilityCore::ToScreen(const Rect& Bounds) const {
        POINT Origin {0, 0};
        ClientToScreen(Hwnd, &Origin);
        const float Scale = Window->GetDpiScale();
        return {
          .left   = Origin.x + Bounds.X * Scale,
          .top    = Origin.y + Bounds.Y * Scale,
          .width  = Bounds.Width * Scale,
          .height = Bounds.Height * Scale,
        };
    }

    void AccessibilityCore::RaiseEvents(const SemanticsUpdate& Update) {
        for (const SemanticsUpdate::PropertyChange& Change : Update.Properties) {
            const SemanticsNode* Node = Tree.Find(Change.Id);
            if (Node == nullptr || Change.Id == SemanticsTree::kRootId) { continue; }
            const SemanticsProperties& Properties = Node->Properties;
            ComPtr<ElementProvider> Element       = GetProvider(Change.Id);

            VARIANT Old;
            VARIANT New;
            VariantInit(&Old);
            VariantInit(&New);
            PROPERTYID Property = 0;
            switch (Change.What) {
                case SemanticsUpdate::Change::Name:
                    Property = UIA_NamePropertyId;
                    SetString(New, Properties.Label);
                    break;
                case SemanticsUpdate::Change::Value:
                    Property = UIA_ValueValuePropertyId;
                    SetString(New, Properties.Value.value_or(std::string()));
                    break;
                case SemanticsUpdate::Change::Range:
                    if (!Properties.Range) { continue; }
                    Property   = UIA_RangeValueValuePropertyId;
                    New.vt     = VT_R8;
                    New.dblVal = Properties.Range->Value;
                    break;
                case SemanticsUpdate::Change::Toggle:
                    Property = UIA_ToggleToggleStatePropertyId;
                    SetInt(New, ToToggleState(Properties.Toggled));
                    break;
                case SemanticsUpdate::Change::Expanded:
                    Property = UIA_ExpandCollapseExpandCollapseStatePropertyId;
                    SetInt(New, ToExpandState(Properties.Expanded));
                    break;
                case SemanticsUpdate::Change::Selected:
                    Property = UIA_SelectionItemIsSelectedPropertyId;
                    SetBool(New, Properties.Selected.value_or(false));
                    if (Properties.Selected.value_or(false)) {
                        UiaRaiseAutomationEvent(Element.Get(), UIA_SelectionItem_ElementSelectedEventId);
                    }
                    break;
                case SemanticsUpdate::Change::Enabled:
                    Property = UIA_IsEnabledPropertyId;
                    SetBool(New, Properties.Enabled);
                    break;
            }
            UiaRaiseAutomationPropertyChangedEvent(Element.Get(), Property, Old, New);
            VariantClear(&New);
        }

        if (!Update.StructureChanged.empty()) {
            constexpr size_t kMaxParents = 8;
            const bool Everything        = Update.StructureChanged.size() > kMaxParents;
            const std::vector<uint64_t> Parents =
              Everything ? std::vector<uint64_t> {SemanticsTree::kRootId} : Update.StructureChanged;
            for (const uint64_t Parent : Parents) {
                ComPtr<ElementProvider> Element = GetProvider(Parent);
                int RuntimeId[]                 = {UiaAppendRuntimeId,
                                                   static_cast<int>(Parent & 0xFFFFFFFF),
                                                   static_cast<int>(Parent >> 32)};
                if (Parent == SemanticsTree::kRootId) {
                    UiaRaiseStructureChangedEvent(Element.Get(), StructureChangeType_ChildrenInvalidated, nullptr, 0);
                } else {
                    UiaRaiseStructureChangedEvent(Element.Get(), StructureChangeType_ChildrenInvalidated, RuntimeId, 3);
                }
            }
        }

        if (Update.FocusChanged && OwnsForeground()) {
            const uint64_t Focused = Tree.GetFocused();
            if (Focused != SemanticsTree::kRootId) {
                UiaRaiseAutomationEvent(GetProvider(Focused).Get(), UIA_AutomationFocusChangedEventId);
            }
        }
    }

    AccessibilityBridge::AccessibilityBridge(WindowImpl& Window) : _Core(std::make_shared<AccessibilityCore>()) {
        _Core->Window = &Window;
        _Core->Hwnd   = Window.GetHwnd();
        _Core->Self   = _Core;
    }

    AccessibilityBridge::~AccessibilityBridge() {
        Disconnect();
    }

    std::optional<LRESULT> AccessibilityBridge::HandleGetObject(WPARAM WParam, LPARAM LParam) {
        if (static_cast<long>(LParam) != static_cast<long>(UiaRootObjectId) || !_Core) { return std::nullopt; }
        _Core->Hwnd      = _Core->Window->GetHwnd();
        _Core->Connected = true;
        _Core->EnsureFresh();
        ComPtr<ElementProvider> Root = _Core->GetProvider(SemanticsTree::kRootId);
        return UiaReturnRawElementProvider(_Core->Hwnd, WParam, LParam, Root.Get());
    }

    void AccessibilityBridge::ContentUpdated() {
        if (!_Core || !_Core->Connected) { return; }
        _Core->Dirty = true;
        if (!UiaClientsAreListening()) { return; }
        _Core->RaiseEvents(_Core->Rebuild());
    }

    void AccessibilityBridge::Disconnect() {
        if (!_Core) { return; }
        if (_Core->Connected && _Core->Hwnd != nullptr) {
            UiaReturnRawElementProvider(_Core->Hwnd, 0, 0, nullptr);
            for (const auto& [Id, Element] : _Core->Providers) {
                UiaDisconnectProvider(Element.Get());
            }
        }
        _Core->Providers.clear();
        _Core->Self.reset();
        _Core.reset();
    }
}  // namespace XenUI
