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

#pragma once

#include "XenUI/Api.hpp"
#include "XenUI/Core/Math.hpp"
#include "XenUI/Graphics/DisplayList.hpp"
#include "XenUI/Graphics/Layer.hpp"
#include "XenUI/Graphics/Painter.hpp"
#include "XenUI/Layout/BoxConstraints.hpp"
#include "XenUI/Render/RenderObject.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace XenUI {
    class FocusManager;
    class PaintContext;
    struct SemanticsProperties;
    class RenderBox;

    /// @brief Boxes under a point, deepest first, with each one's origin in window space.
    class XENUI_API HitTestResult {
    public:
        struct Entry {
            RenderBox* Target = nullptr;
            std::weak_ptr<void> Lifetime;
            /// @brief Window-space position of the target's top-left corner.
            Vec2 Origin;

            [[nodiscard]] bool IsAlive() const { return !Lifetime.expired(); }
        };

        explicit HitTestResult(const Vec2& Position) : _Position(Position) {}

        /// @param LocalPosition The hit position in Target's coordinate space.
        void Add(RenderBox& Target, const Vec2& LocalPosition);

        [[nodiscard]] const std::vector<Entry>& GetEntries() const { return _Entries; }
        [[nodiscard]] const Vec2& GetPosition() const { return _Position; }

    private:
        Vec2 _Position;
        std::vector<Entry> _Entries;
    };

    /// @brief Render object laid out with box constraints. Parents call Layout on each child with constraints,
    /// read the child's size, then position it with SetOffset. Painting happens in local coordinates.
    class XENUI_API RenderBox : public RenderObject {
    public:
        RenderBox();
        ~RenderBox() override;

        /// @param ParentUsesSize Whether the caller's own layout depends on this box's size. When false, or when
        /// the constraints are tight, this box becomes a relayout boundary and can be laid out on its own.
        void Layout(const BoxConstraints& Constraints, bool ParentUsesSize = false);

        [[nodiscard]] const Size& GetSize() const { return _Size; }
        [[nodiscard]] const BoxConstraints& GetConstraints() const { return _Constraints; }

        /// @brief Position of this box's top-left corner in its parent's coordinate space.
        [[nodiscard]] const Vec2& GetOffset() const { return _Offset; }
        void SetOffset(const Vec2& Offset) { _Offset = Offset; }

        /// @brief Position of this box's top-left corner in window space, as of the last layout.
        [[nodiscard]] Vec2 GetGlobalOffset() const;

        [[nodiscard]] bool NeedsLayout() const { return _NeedsLayout; }
        [[nodiscard]] bool NeedsPaint() const { return _NeedsPaint; }

        void MarkNeedsLayout();
        void MarkNeedsPaint();

        /// @brief Draws this box and its children in local coordinates. The default paints only the children.
        virtual void Paint(PaintContext& Context);

        /// @brief A repaint boundary records into its own layer, so repainting it leaves its ancestors untouched
        /// and repainting an ancestor reuses its recording.
        [[nodiscard]] virtual bool IsRepaintBoundary() const { return false; }

        /// @brief Adds this box and any hit descendants to Result if Position (in local coordinates) hits it.
        virtual bool HitTest(HitTestResult& Result, const Vec2& Position);

        /// @brief Describes this box to assistive technology such as screen readers. Return true to make it a node
        /// of the accessibility tree; the default contributes nothing, so its children are described in its place.
        [[nodiscard]] virtual bool DescribeSemantics(SemanticsProperties& Properties) const {
            (void)Properties;
            return false;
        }

        /// @brief Whether children are drawn only inside this box's bounds, so parts outside it are hidden.
        [[nodiscard]] virtual bool ClipsChildren() const { return false; }

        /// @brief Expires when this box is destroyed, so event routing can hold on to boxes safely.
        [[nodiscard]] std::weak_ptr<void> GetLifetimeToken();

        [[nodiscard]] size_t GetChildCount() const { return GetChildren().size(); }
        [[nodiscard]] RenderBox& GetChild(size_t Index) const { return *static_cast<RenderBox*>(GetChildren()[Index]); }
        [[nodiscard]] RenderBox* GetFirstChild() const;

        /// @internal
        [[nodiscard]] const std::shared_ptr<Layer>& GetLayer();
        /// @internal
        void RepaintLayer();
        /// @internal
        void RelayoutAsBoundary();

    protected:
        /// @brief Computes the size and lays out children. The default sizes to the first child, or to the
        /// smallest allowed size without one.
        virtual void PerformLayout();

        void SetSize(const Size& NewSize) { _Size = NewSize; }

        /// @brief Tests children from topmost to bottommost, stopping at the first hit.
        virtual bool HitTestChildren(HitTestResult& Result, const Vec2& Position);

        /// @brief Whether this box itself is hit at Position, for boxes that draw something there.
        [[nodiscard]] virtual bool HitTestSelf(const Vec2& Position) const {
            (void)Position;
            return false;
        }

        void ChildrenChanged() override;
        void OnDetached() override;

    private:
        friend class PaintContext;

        BoxConstraints _Constraints;
        Size _Size;
        Vec2 _Offset;
        bool _NeedsLayout      = true;
        bool _NeedsPaint       = true;
        bool _RelayoutBoundary = false;
        bool _HasLaidOut       = false;
        bool _InLayout         = false;

        std::shared_ptr<Layer> _Layer;
        std::vector<std::shared_ptr<Layer>> _PictureLayers;
        std::shared_ptr<int> _Lifetime;
    };

    /// @brief Records a repaint boundary's subtree. Ordinary children draw into the current recording;
    /// child repaint boundaries contribute their own layers, preserving paint order.
    class XENUI_API PaintContext {
    public:
        explicit PaintContext(RenderBox& Boundary);
        ~PaintContext();

        PaintContext(const PaintContext&)            = delete;
        PaintContext& operator=(const PaintContext&) = delete;

        /// @brief Painter for the current box, in its local coordinates.
        [[nodiscard]] Painter& GetPainter();

        /// @brief Paints Child at its offset.
        void PaintChild(RenderBox& Child);

        /// @internal Publishes the recording to the boundary's layer.
        void Finish();

    private:
        void StartRecording();
        void StopRecording();
        void ResetBaseTransform();

        RenderBox& _Boundary;
        std::vector<std::shared_ptr<Layer>> _Layers;
        size_t _PictureIndex = 0;
        std::optional<DisplayList> _List;
        std::optional<Painter> _Painter;
        uint64_t _RecordingId     = 0;
        bool _BaseTransformPushed = false;
        Vec2 _Offset;
    };

    /// @brief Schedules and flushes layout and painting for one render tree.
    class XENUI_API PipelineOwner {
    public:
        void ScheduleLayout(RenderBox& Box);
        void SchedulePaint(RenderBox& Box);
        void Unschedule(RenderBox& Box);

        void FlushLayout();
        void FlushPaint();

        [[nodiscard]] bool HasPendingWork() const { return !_LayoutQueue.empty() || !_PaintQueue.empty(); }

        /// @brief Invoked when work is first scheduled, so the host can request a frame.
        void SetOnNeedsUpdate(std::function<void()> Callback) { _OnNeedsUpdate = std::move(Callback); }

        void SetFocusManager(FocusManager* Manager) { _FocusManager = Manager; }
        [[nodiscard]] FocusManager* GetFocusManager() const { return _FocusManager; }

        /// @brief The platform window (an HWND) that hosts this tree, for render objects that embed native windows.
        void SetNativeParent(void* Parent) { _NativeParent = Parent; }
        [[nodiscard]] void* GetNativeParent() const { return _NativeParent; }

    private:
        void NotifyNeedsUpdate();

        std::vector<RenderBox*> _LayoutQueue;
        std::vector<RenderBox*> _PaintQueue;
        std::function<void()> _OnNeedsUpdate;
        FocusManager* _FocusManager = nullptr;
        void* _NativeParent         = nullptr;
    };

    /// @brief Root of a render tree: fills the viewport with its child and owns the root layer.
    class XENUI_API RenderView final : public RenderBox {
    public:
        /// @brief Lays the child out to fill Viewport exactly.
        void SetViewportSize(const Size& Viewport);

        /// @brief Lays the child out at its natural size, up to Maximum, and takes that size. Used by popups.
        void SetSizeToContent(const Size& Maximum);

        void SetChild(RenderBox* Child);

        [[nodiscard]] bool IsRepaintBoundary() const override { return true; }
        [[nodiscard]] bool IsSizedToContent() const { return _SizeToContent; }

    protected:
        void PerformLayout() override;

    private:
        Size _Viewport;
        bool _SizeToContent = false;
    };
}  // namespace XenUI
