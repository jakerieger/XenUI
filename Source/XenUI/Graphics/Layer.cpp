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

#include "XenUI/Graphics/Layer.hpp"
#include "XenUI/Graphics/Compositor.hpp"
#include "XenUI/Graphics/LayerImpl.hpp"

#include <algorithm>

namespace XenUI {
    LayerImpl::LayerImpl(Layer& Owner) : _Owner(Owner) {}

    namespace {
        std::vector<LayerImpl*>& AnchoredLayers() {
            static std::vector<LayerImpl*> Layers;
            return Layers;
        }
    }  // namespace

    const std::vector<LayerImpl*>& LayerImpl::GetAnchoredLayers() {
        return AnchoredLayers();
    }

    void LayerImpl::SetNativeAnchor(const Size& Extent, NativePlacementCallback OnPlaced) {
        if (!_Native) {
            _Native = std::make_unique<NativeAnchor>();
            AnchoredLayers().push_back(this);
        }
        if (_Native->Extent != Extent) { _Native->Last.reset(); }
        _Native->Extent   = Extent;
        _Native->OnPlaced = std::move(OnPlaced);
        DamageLocal({0.0f, 0.0f, Extent.Width, Extent.Height});
    }

    void LayerImpl::ClearNativeAnchor() {
        if (!_Native) { return; }
        std::erase(AnchoredLayers(), this);
        _Native.reset();
    }

    LayerImpl::~LayerImpl() {
        ClearNativeAnchor();
        for (const std::shared_ptr<Layer>& Child : _Children) {
            Child->GetImpl()._Parent = nullptr;
        }
    }

    void LayerImpl::SetContent(DisplayList Content) {
        DamageLocal(_Content.GetBounds());
        _Content = std::move(Content);
        InvalidateBounds();
        InvalidateCaches(true);
        DamageLocal(_Content.GetBounds());
    }

    void LayerImpl::SetLiveContent(const Size& Extent, LiveDrawer Drawer, bool Continuous) {
        if (_Live) { DamageLocal({0.0f, 0.0f, _Live->Extent.Width, _Live->Extent.Height}); }
        if (!_Live) { _Live = std::make_unique<LiveState>(); }
        _Live->Extent     = Extent;
        _Live->Drawer     = std::move(Drawer);
        _Live->Continuous = Continuous;
        _Live->Dirty      = true;
        InvalidateBounds();
        InvalidateCaches(true);
        DamageLocal({0.0f, 0.0f, Extent.Width, Extent.Height});
    }

    void LayerImpl::ClearLiveContent() {
        if (!_Live) { return; }
        DamageLocal({0.0f, 0.0f, _Live->Extent.Width, _Live->Extent.Height});
        _Live.reset();
        InvalidateBounds();
        InvalidateCaches(true);
    }

    void LayerImpl::InvalidateLive() {
        if (!_Live) { return; }
        _Live->Dirty = true;
        InvalidateCaches(true);
        DamageLocal({0.0f, 0.0f, _Live->Extent.Width, _Live->Extent.Height});
    }

    void LayerImpl::SetOffset(const Vec2& Offset) {
        if (Offset == _Offset) { return; }
        DamageSubtree();
        _Offset = Offset;
        if (_Parent != nullptr) { _Parent->InvalidateBounds(); }
        InvalidateCaches(false);
        DamageSubtree();
    }

    void LayerImpl::SetTransform(const Matrix3x2& Transform) {
        if (Transform == _Transform) { return; }
        DamageSubtree();
        _Transform = Transform;
        if (_Parent != nullptr) { _Parent->InvalidateBounds(); }
        InvalidateCaches(false);
        DamageSubtree();
    }

    void LayerImpl::SetOpacity(float Opacity) {
        Opacity = std::clamp(Opacity, 0.0f, 1.0f);
        if (Opacity == _Opacity) { return; }
        _Opacity = Opacity;
        InvalidateCaches(false);
        DamageSubtree();
    }

    void LayerImpl::SetClip(const std::optional<Rect>& Clip) {
        if (Clip == _Clip) { return; }
        DamageSubtree();
        _Clip = Clip;
        InvalidateBounds();
        InvalidateCaches(true);
        DamageSubtree();
    }

    void LayerImpl::SetVisible(bool Visible) {
        if (Visible == _Visible) { return; }
        if (_Visible) { DamageSubtree(); }
        _Visible = Visible;
        if (_Parent != nullptr) { _Parent->InvalidateBounds(); }
        InvalidateCaches(false);
        if (_Visible) { DamageSubtree(); }
    }

    void LayerImpl::SetCached(bool Cached) {
        if (Cached == _Cached) { return; }
        _Cached = Cached;
        _Cache  = {};
    }

    void LayerImpl::AddChild(std::shared_ptr<Layer> Child) {
        if (!Child || &Child->GetImpl() == this) { return; }

        LayerImpl& ChildImpl = Child->GetImpl();
        if (ChildImpl._Parent != nullptr) { ChildImpl._Parent->DetachChild(ChildImpl); }

        ChildImpl._Parent = this;
        _Children.push_back(std::move(Child));
        InvalidateBounds();
        InvalidateCaches(true);
        ChildImpl.DamageSubtree();
    }

    void LayerImpl::RemoveChild(Layer& Child) {
        LayerImpl& ChildImpl = Child.GetImpl();
        if (ChildImpl._Parent == this) { DetachChild(ChildImpl); }
    }

    void LayerImpl::SetChildren(const std::vector<std::shared_ptr<Layer>>& Children) {
        if (Children == _Children) { return; }
        RemoveAllChildren();
        for (const std::shared_ptr<Layer>& Child : Children) {
            AddChild(Child);
        }
    }

    void LayerImpl::RemoveAllChildren() {
        for (const std::shared_ptr<Layer>& Child : _Children) {
            LayerImpl& ChildImpl = Child->GetImpl();
            ChildImpl.DamageSubtree();
            ChildImpl._Parent = nullptr;
        }
        _Children.clear();
        InvalidateBounds();
        InvalidateCaches(true);
    }

    void LayerImpl::RemoveFromParent() {
        if (_Parent != nullptr) { _Parent->DetachChild(*this); }
    }

    void LayerImpl::AttachHost(Compositor* Host) {
        _Host = Host;
    }

    Matrix3x2 LayerImpl::GetLocalTransform() const {
        return _Transform * Matrix3x2::Translation(_Offset);
    }

    Matrix3x2 LayerImpl::GetWorldTransform() const {
        Matrix3x2 World = GetLocalTransform();
        for (const LayerImpl* Ancestor = _Parent; Ancestor != nullptr; Ancestor = Ancestor->_Parent) {
            World = World * Ancestor->GetLocalTransform();
        }
        return World;
    }

    const Rect& LayerImpl::GetSubtreeBounds() const {
        if (!_SubtreeBounds) {
            Rect Bounds = _Content.GetBounds();
            if (_Live) { Bounds = Bounds.Union({0.0f, 0.0f, _Live->Extent.Width, _Live->Extent.Height}); }
            for (const std::shared_ptr<Layer>& Child : _Children) {
                const LayerImpl& ChildImpl = Child->GetImpl();
                if (!ChildImpl._Visible) { continue; }
                Bounds = Bounds.Union(ChildImpl.GetLocalTransform().TransformRect(ChildImpl.GetSubtreeBounds()));
            }
            if (_Clip) { Bounds = Bounds.Intersection(*_Clip); }
            _SubtreeBounds = Bounds;
        }
        return *_SubtreeBounds;
    }

    void LayerImpl::DamageSubtree() {
        DamageLocal(GetSubtreeBounds());
    }

    void LayerImpl::DamageLocal(const Rect& LocalArea) {
        for (const LayerImpl* Ancestor = this; Ancestor != nullptr; Ancestor = Ancestor->_Parent) {
            if (!Ancestor->_Visible) { return; }
        }

        Compositor* Host = FindHost();
        if (Host == nullptr) { return; }

        const Rect Area = _Clip ? LocalArea.Intersection(*_Clip) : LocalArea;
        if (Area.IsEmpty()) { return; }
        Host->AddDamage(GetWorldTransform().TransformRect(Area));
    }

    void LayerImpl::InvalidateBounds() {
        for (LayerImpl* Ancestor = this; Ancestor != nullptr && Ancestor->_SubtreeBounds;
             Ancestor            = Ancestor->_Parent) {
            Ancestor->_SubtreeBounds.reset();
        }
    }

    void LayerImpl::InvalidateCaches(bool IncludeSelf) {
        for (LayerImpl* Ancestor = IncludeSelf ? this : _Parent; Ancestor != nullptr; Ancestor = Ancestor->_Parent) {
            if (Ancestor->_Cached) { Ancestor->_Cache.Dirty = true; }
        }
    }

    void LayerImpl::DetachChild(LayerImpl& Child) {
        const auto It = std::find_if(_Children.begin(), _Children.end(), [&Child](const std::shared_ptr<Layer>& Entry) {
            return &Entry->GetImpl() == &Child;
        });
        if (It == _Children.end()) { return; }

        Child.DamageSubtree();
        const std::shared_ptr<Layer> KeepAlive = *It;
        _Children.erase(It);
        Child._Parent = nullptr;
        InvalidateBounds();
        InvalidateCaches(true);
    }

    Compositor* LayerImpl::FindHost() const {
        const LayerImpl* Root = this;
        while (Root->_Parent != nullptr) {
            Root = Root->_Parent;
        }
        return Root->_Host;
    }

    std::shared_ptr<Layer> Layer::Create() {
        return std::shared_ptr<Layer>(new Layer());
    }

    Layer::Layer() : _Impl(std::make_unique<LayerImpl>(*this)) {}

    Layer::~Layer() = default;

    void Layer::SetContent(DisplayList Content) {
        _Impl->SetContent(std::move(Content));
    }

    const DisplayList& Layer::GetContent() const {
        return _Impl->GetContent();
    }

    void Layer::SetLiveContent(const Size& Extent, LiveDrawer Drawer, bool Continuous) {
        _Impl->SetLiveContent(Extent, std::move(Drawer), Continuous);
    }

    void Layer::ClearLiveContent() {
        _Impl->ClearLiveContent();
    }

    void Layer::SetNativeAnchor(const Size& Extent, NativePlacementCallback OnPlaced) {
        _Impl->SetNativeAnchor(Extent, std::move(OnPlaced));
    }

    void Layer::ClearNativeAnchor() {
        _Impl->ClearNativeAnchor();
    }

    void Layer::InvalidateLive() {
        _Impl->InvalidateLive();
    }

    void Layer::SetOffset(const Vec2& Offset) {
        _Impl->SetOffset(Offset);
    }

    Vec2 Layer::GetOffset() const {
        return _Impl->GetOffset();
    }

    void Layer::SetTransform(const Matrix3x2& Transform) {
        _Impl->SetTransform(Transform);
    }

    const Matrix3x2& Layer::GetTransform() const {
        return _Impl->GetTransform();
    }

    void Layer::SetOpacity(float Opacity) {
        _Impl->SetOpacity(Opacity);
    }

    float Layer::GetOpacity() const {
        return _Impl->GetOpacity();
    }

    void Layer::SetClip(const std::optional<Rect>& Clip) {
        _Impl->SetClip(Clip);
    }

    const std::optional<Rect>& Layer::GetClip() const {
        return _Impl->GetClip();
    }

    void Layer::SetVisible(bool Visible) {
        _Impl->SetVisible(Visible);
    }

    bool Layer::IsVisible() const {
        return _Impl->IsVisible();
    }

    void Layer::SetCached(bool Cached) {
        _Impl->SetCached(Cached);
    }

    bool Layer::IsCached() const {
        return _Impl->IsCached();
    }

    void Layer::AddChild(std::shared_ptr<Layer> Child) {
        _Impl->AddChild(std::move(Child));
    }

    void Layer::RemoveChild(Layer& Child) {
        _Impl->RemoveChild(Child);
    }

    void Layer::SetChildren(const std::vector<std::shared_ptr<Layer>>& Children) {
        _Impl->SetChildren(Children);
    }

    void Layer::RemoveAllChildren() {
        _Impl->RemoveAllChildren();
    }

    void Layer::RemoveFromParent() {
        _Impl->RemoveFromParent();
    }

    const std::vector<std::shared_ptr<Layer>>& Layer::GetChildren() const {
        return _Impl->GetChildren();
    }

    Layer* Layer::GetParent() const {
        LayerImpl* Parent = _Impl->GetParent();
        return Parent != nullptr ? &Parent->GetOwner() : nullptr;
    }
}  // namespace XenUI
