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

#include <Windows.h>
#include <d2d1_1.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace XenUI {
    class GraphicsDevice;
    class Layer;
    class LayerImpl;
    class SwapChain;
    class WindowImpl;

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

        explicit Compositor(WindowImpl& Owner);
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
        [[nodiscard]] std::vector<RECT> ToPixelRects(const DamageRegion& Region, uint32_t Width, uint32_t Height) const;

        WindowImpl& _Owner;
        std::shared_ptr<Layer> _Root;
        DamageRegion _Pending;
        std::vector<DamageRegion> _History;
        std::vector<Flash> _Flashes;
        uint32_t _FlashCounter   = 0;
        bool _ShowDamage         = false;
        bool _LiveActive         = false;
        uint64_t _PlacementFrame = 0;

        ID2D1DeviceContext* _Context = nullptr;
        GraphicsDevice* _Device      = nullptr;
        float _DpiScale              = 1.0f;
        Timings _LastTimings;
    };
}  // namespace XenUI
