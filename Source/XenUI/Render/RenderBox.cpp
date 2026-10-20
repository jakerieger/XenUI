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

#include "XenUI/Render/RenderBox.hpp"

#include <algorithm>
#include <array>

namespace XenUI {
    void HitTestResult::Add(RenderBox& Target, const Vec2& LocalPosition) {
        _Entries.push_back({&Target, Target.GetLifetimeToken(), _Position - LocalPosition});
    }

    RenderBox::RenderBox() = default;

    RenderBox::~RenderBox() {
        if (GetOwner() != nullptr) { GetOwner()->Unschedule(*this); }
    }

    void RenderBox::Layout(const BoxConstraints& Constraints, bool ParentUsesSize) {
        _RelayoutBoundary = !ParentUsesSize || Constraints.IsTight() || GetParent() == nullptr;
        if (!_NeedsLayout && _HasLaidOut && Constraints == _Constraints) { return; }

        _Constraints = Constraints;
        _InLayout    = true;
        PerformLayout();
        _InLayout    = false;
        _NeedsLayout = false;
        _HasLaidOut  = true;
        MarkNeedsPaint();
    }

    void RenderBox::MarkNeedsLayout() {
        if (_NeedsLayout || _InLayout) { return; }
        _NeedsLayout = true;

        auto* ParentBox = dynamic_cast<RenderBox*>(GetParent());
        if (_RelayoutBoundary || ParentBox == nullptr) {
            if (GetOwner() != nullptr) { GetOwner()->ScheduleLayout(*this); }
            return;
        }
        ParentBox->MarkNeedsLayout();
    }

    void RenderBox::MarkNeedsPaint() {
        if (_NeedsPaint) { return; }
        _NeedsPaint = true;

        if (IsRepaintBoundary()) {
            if (GetOwner() != nullptr) { GetOwner()->SchedulePaint(*this); }
            return;
        }
        if (auto* ParentBox = dynamic_cast<RenderBox*>(GetParent())) { ParentBox->MarkNeedsPaint(); }
    }

    void RenderBox::Paint(PaintContext& Context) {
        for (size_t i = 0; i < GetChildCount(); ++i) {
            Context.PaintChild(GetChild(i));
        }
    }

    bool RenderBox::HitTest(HitTestResult& Result, const Vec2& Position) {
        if (!Rect {0.0f, 0.0f, _Size.Width, _Size.Height}.Contains(Position)) { return false; }
        if (HitTestChildren(Result, Position) || HitTestSelf(Position)) {
            Result.Add(*this, Position);
            return true;
        }
        return false;
    }

    bool RenderBox::HitTestChildren(HitTestResult& Result, const Vec2& Position) {
        for (size_t i = GetChildCount(); i-- > 0;) {
            RenderBox& Child = GetChild(i);
            if (Child.HitTest(Result, Position - Child.GetOffset())) { return true; }
        }
        return false;
    }

    std::weak_ptr<void> RenderBox::GetLifetimeToken() {
        if (!_Lifetime) { _Lifetime = std::make_shared<int>(0); }
        return _Lifetime;
    }

    Vec2 RenderBox::GetGlobalOffset() const {
        Vec2 Result = _Offset;
        for (const RenderObject* Ancestor = GetParent(); Ancestor != nullptr; Ancestor = Ancestor->GetParent()) {
            if (const auto* Box = dynamic_cast<const RenderBox*>(Ancestor)) { Result += Box->GetOffset(); }
        }
        return Result;
    }

    RenderBox* RenderBox::GetFirstChild() const {
        return GetChildren().empty() ? nullptr : &GetChild(0);
    }

    const std::shared_ptr<Layer>& RenderBox::GetLayer() {
        if (!_Layer) { _Layer = Layer::Create(); }
        return _Layer;
    }

    void RenderBox::RepaintLayer() {
        PaintContext Context(*this);
        Paint(Context);
        Context.Finish();
        _NeedsPaint = false;
    }

    void RenderBox::RelayoutAsBoundary() {
        if (!_NeedsLayout) { return; }
        if (!_HasLaidOut && GetParent() != nullptr) { return; }

        _InLayout = true;
        PerformLayout();
        _InLayout    = false;
        _NeedsLayout = false;
        _HasLaidOut  = true;
        MarkNeedsPaint();
    }

    void RenderBox::PerformLayout() {
        if (RenderBox* Child = GetFirstChild()) {
            Child->Layout(_Constraints, true);
            Child->SetOffset({});
            SetSize(Child->GetSize());
            return;
        }
        SetSize(_Constraints.GetSmallest());
    }

    void RenderBox::ChildrenChanged() {
        MarkNeedsLayout();
    }

    void RenderBox::OnDetached() {
        GetOwner()->Unschedule(*this);
    }

    PaintContext::PaintContext(RenderBox& Boundary) : _Boundary(Boundary) {}

    PaintContext::~PaintContext() = default;

    Painter& PaintContext::GetPainter() {
        if (!_Painter) { StartRecording(); }
        return *_Painter;
    }

    void PaintContext::PaintChild(RenderBox& Child) {
        const Vec2 Delta = Child.GetOffset();

        if (Child.IsRepaintBoundary()) {
            StopRecording();
            if (Child.NeedsPaint()) { Child.RepaintLayer(); }
            const std::shared_ptr<Layer>& ChildLayer = Child.GetLayer();
            ChildLayer->SetOffset(_Offset + Delta);
            _Layers.push_back(ChildLayer);
            return;
        }

        const bool Translates          = Delta != Vec2 {};
        const uint64_t RecordingAtPush = _Painter ? _RecordingId : 0;
        if (Translates) {
            if (_Painter) { _Painter->PushTransform(Matrix3x2::Translation(Delta)); }
            _Offset += Delta;
        }

        Child.Paint(*this);
        Child._NeedsPaint = false;

        if (Translates) {
            _Offset -= Delta;
            if (_Painter) {
                if (RecordingAtPush == _RecordingId) {
                    _Painter->PopTransform();
                } else {
                    ResetBaseTransform();
                }
            }
        }
    }

    void PaintContext::Finish() {
        StopRecording();
        _Boundary._PictureLayers.resize(_PictureIndex);
        _Boundary.GetLayer()->SetChildren(_Layers);
    }

    void PaintContext::StartRecording() {
        _List.emplace();
        _Painter.emplace(*_List);
        ++_RecordingId;
        _BaseTransformPushed = false;
        ResetBaseTransform();
    }

    void PaintContext::StopRecording() {
        if (!_Painter) { return; }
        _Painter.reset();

        if (!_List->GetBounds().IsEmpty()) {
            std::vector<std::shared_ptr<Layer>>& Pool = _Boundary._PictureLayers;
            if (_PictureIndex == Pool.size()) { Pool.push_back(Layer::Create()); }
            const std::shared_ptr<Layer>& Picture = Pool[_PictureIndex++];
            Picture->SetContent(std::move(*_List));
            _Layers.push_back(Picture);
        }
        _List.reset();
    }

    void PaintContext::ResetBaseTransform() {
        if (_BaseTransformPushed) { _Painter->PopTransform(); }
        _BaseTransformPushed = _Offset != Vec2 {};
        if (_BaseTransformPushed) { _Painter->PushTransform(Matrix3x2::Translation(_Offset)); }
    }

    void PipelineOwner::ScheduleLayout(RenderBox& Box) {
        if (std::find(_LayoutQueue.begin(), _LayoutQueue.end(), &Box) != _LayoutQueue.end()) { return; }
        const bool WasIdle = !HasPendingWork();
        _LayoutQueue.push_back(&Box);
        if (WasIdle) { NotifyNeedsUpdate(); }
    }

    void PipelineOwner::SchedulePaint(RenderBox& Box) {
        if (std::find(_PaintQueue.begin(), _PaintQueue.end(), &Box) != _PaintQueue.end()) { return; }
        const bool WasIdle = !HasPendingWork();
        _PaintQueue.push_back(&Box);
        if (WasIdle) { NotifyNeedsUpdate(); }
    }

    void PipelineOwner::Unschedule(RenderBox& Box) {
        std::erase(_LayoutQueue, &Box);
        std::erase(_PaintQueue, &Box);
    }

    void PipelineOwner::FlushLayout() {
        while (!_LayoutQueue.empty()) {
            std::vector<RenderBox*> Queue = std::move(_LayoutQueue);
            _LayoutQueue.clear();
            std::sort(Queue.begin(), Queue.end(), [](const RenderBox* Left, const RenderBox* Right) {
                return Left->GetDepth() < Right->GetDepth();
            });
            for (RenderBox* Box : Queue) {
                if (Box->GetOwner() == this) { Box->RelayoutAsBoundary(); }
            }
        }
    }

    void PipelineOwner::FlushPaint() {
        std::vector<RenderBox*> Queue = std::move(_PaintQueue);
        _PaintQueue.clear();
        std::sort(Queue.begin(), Queue.end(), [](const RenderBox* Left, const RenderBox* Right) {
            return Left->GetDepth() < Right->GetDepth();
        });
        for (RenderBox* Box : Queue) {
            if (Box->NeedsPaint() && Box->GetOwner() == this) { Box->RepaintLayer(); }
        }
    }

    void PipelineOwner::NotifyNeedsUpdate() {
        if (_OnNeedsUpdate) { _OnNeedsUpdate(); }
    }

    void RenderView::SetViewportSize(const Size& Viewport) {
        if (Viewport == _Viewport && !_SizeToContent) { return; }
        _Viewport      = Viewport;
        _SizeToContent = false;
        MarkNeedsLayout();
    }

    void RenderView::SetSizeToContent(const Size& Maximum) {
        if (Maximum == _Viewport && _SizeToContent) { return; }
        _Viewport      = Maximum;
        _SizeToContent = true;
        MarkNeedsLayout();
    }

    void RenderView::SetChild(RenderBox* Child) {
        if (Child == nullptr) {
            SetChildren({});
            return;
        }
        const std::array<RenderObject*, 1> Children {Child};
        SetChildren(Children);
    }

    void RenderView::PerformLayout() {
        RenderBox* Child = GetFirstChild();
        if (_SizeToContent) {
            if (Child != nullptr) {
                Child->Layout(BoxConstraints::Loose(_Viewport), true);
                Child->SetOffset({});
            }
            SetSize(Child != nullptr ? Child->GetSize() : Size {});
            return;
        }

        SetSize(_Viewport);
        if (Child != nullptr) {
            Child->Layout(BoxConstraints::Tight(_Viewport));
            Child->SetOffset({});
        }
    }
}  // namespace XenUI
