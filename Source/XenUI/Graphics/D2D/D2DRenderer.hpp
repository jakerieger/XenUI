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

#include "XenUI/Core/Matrix.hpp"
#include "XenUI/Graphics/Brush.hpp"
#include "XenUI/Graphics/DisplayList.hpp"
#include "XenUI/Graphics/StrokeStyle.hpp"

#include <d2d1_1.h>
#include <wrl/client.h>

#include <unordered_map>
#include <vector>

namespace XenUI {
    class GraphicsDevice;

    struct GradientKey {
        std::vector<GradientStop> Stops;
        GradientExtend Extend = GradientExtend::Clamp;

        bool operator==(const GradientKey&) const = default;
    };

    struct GradientKeyHash {
        size_t operator()(const GradientKey& Key) const noexcept;
    };

    struct StrokeKeyHash {
        size_t operator()(const StrokeStyle& Key) const noexcept;
    };

    /// @brief Device-dependent brushes and device-independent stroke styles, reused across frames and windows.
    class D2DResources {
    public:
        void Reset();

        /// @brief The returned brush is reconfigured on every call; use it before requesting another.
        ID2D1Brush* GetBrush(ID2D1DeviceContext& Context, const Brush& Paint);

        /// @return nullptr for the default style, which Direct2D treats as solid with flat caps.
        ID2D1StrokeStyle* GetStrokeStyle(ID2D1Factory1& Factory, const StrokeStyle& Stroke);

    private:
        struct GradientBrushes {
            Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> Stops;
            Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> Linear;
            Microsoft::WRL::ComPtr<ID2D1RadialGradientBrush> Radial;
        };

        GradientBrushes&
        GetGradient(ID2D1DeviceContext& Context, const std::vector<GradientStop>& Stops, GradientExtend Extend);

        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> _SolidBrush;
        std::unordered_map<GradientKey, GradientBrushes, GradientKeyHash> _Gradients;
        std::unordered_map<StrokeStyle, Microsoft::WRL::ComPtr<ID2D1StrokeStyle1>, StrokeKeyHash> _StrokeStyles;
    };

    /// @brief Draws List with every coordinate mapped through Base, leaving the context's transform unspecified.
    void ReplayDisplayList(ID2D1DeviceContext& Context,
                           GraphicsDevice& Device,
                           const DisplayList& List,
                           const Matrix3x2& Base);
}  // namespace XenUI
