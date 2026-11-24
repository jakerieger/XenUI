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

#include "XenUI/Core/Color.hpp"
#include "XenUI/Core/Math.hpp"
#include "XenUI/Core/Matrix.hpp"
#include "XenUI/Graphics/DamageRegion.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace XenUI {
    class CompositorBackend;
    class GraphicsDevice;
    class Layer;
    class LayerImpl;
    class SwapChain;

    /// @brief Renders a window's layer tree, repainting only damaged regions. Tracks damage across the swapchain's
    /// buffers so that each back buffer is brought up to date with everything that changed since it was last shown.
    class Compositor {
    public:
        struct Timings {
            float WaitMs    = 0.0f;
            float DrawMs    = 0.0f;
            float PresentMs = 0.0f;
            /// @brief Fraction of the window that changed, from 0 to 1.
            float Damage = 0.0f;
        };

        /// @param RequestFrame Asks the window for another frame, while flashes fade or live layers animate.
        explicit Compositor(std::function<void()> RequestFrame);
        ~Compositor();

        Compositor(const Compositor&)            = delete;
        Compositor& operator=(const Compositor&) = delete;

        void SetRoot(std::shared_ptr<Layer> Root);
        /// @brief Timings of the last Render.
        [[nodiscard]] const Timings& GetLastTimings() const { return _LastTimings; }
        [[nodiscard]] const std::shared_ptr<Layer>& GetRoot() const { return _Root; }

        /// @param Area Window-space rectangle in device-independent pixels.
        void AddDamage(const Rect& Area);
        void AddFullDamage();

        /// @brief Forgets what the back buffers contain. Call after the swapchain is created or resized.
        void ResetBufferHistory();

        void SetShowDamage(bool Enabled);
        /// @brief Draws a one-pixel line of Border just inside the window's edge, above all content, for windows the
        /// system doesn't frame (borderless windows on Linux). Nothing when unset.
        void SetFrameBorder(const std::optional<Color>& Border);

        [[nodiscard]] bool HasPendingDamage() const { return !_Pending.IsEmpty(); }

        void Render(SwapChain& Target, GraphicsDevice& Device, const Color& Background);

    private:
        struct Flash {
            Rect Area;
            Color Tint;
            uint32_t Age = 0;
        };

        void UpdateFlashes(DamageRegion& Changed);
        void UpdateCaches(LayerImpl& Node, const Matrix3x2& ParentWorld);
        void PlaceNativeAnchors(LayerImpl& Node, const Matrix3x2& ParentWorld, const std::optional<Rect>& ParentClip);
        void HideUnplacedAnchors();
        void PrepareLiveLayers(LayerImpl& Node, const Matrix3x2& ParentWorld);
        void RenderLive(LayerImpl& Node, const Matrix3x2& World);
        void DrawLayer(LayerImpl& Node, const Matrix3x2& ParentWorld, const Rect& Damage);
        void DrawLayerContents(LayerImpl& Node, const Matrix3x2& World, const Rect& Damage);
        void DrawFlashes(const Rect& Area);
        [[nodiscard]] std::vector<PixelRect> ToPixelRects(const DamageRegion& Region) const;

        std::function<void()> _RequestFrame;
        std::unique_ptr<CompositorBackend> _Backend;
        std::shared_ptr<Layer> _Root;
        DamageRegion _Pending;
        std::vector<DamageRegion> _History;
        std::vector<Flash> _Flashes;
        uint32_t _FlashCounter   = 0;
        bool _ShowDamage         = false;
        std::optional<Color> _FrameBorder;
        bool _LiveActive         = false;
        uint64_t _PlacementFrame = 0;

        Timings _LastTimings;
    };
}  // namespace XenUI
