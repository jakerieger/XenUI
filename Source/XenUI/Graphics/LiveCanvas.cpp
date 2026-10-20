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

#include "XenUI/Graphics/LiveCanvas.hpp"
#include "XenUI/Core/Error.hpp"
#include "XenUI/Graphics/LiveRecording.hpp"

#include <algorithm>
#include <cstring>

namespace XenUI {
    namespace {
        LiveInstance
        MakeInstance(const Vec2& Start, const Vec2& End, float Width, LivePrimitiveKind Kind, const Color& Tint) {
            return {
              .Start = {Start.X, Start.Y},
              .End   = {End.X, End.Y},
              .Color = {Tint.R, Tint.G, Tint.B, Tint.A},
              .Width = Width,
              .Kind  = static_cast<float>(Kind),
            };
        }
    }  // namespace

    LiveTexture::LiveTexture(uint32_t Width, uint32_t Height)
        : _Data(std::make_unique<LiveTextureData>(Width, Height)) {}

    LiveTexture::~LiveTexture() = default;

    uint32_t LiveTexture::GetWidth() const {
        return _Data->Width;
    }

    uint32_t LiveTexture::GetHeight() const {
        return _Data->Height;
    }

    void LiveTexture::WriteColumn(uint32_t X, std::span<const uint32_t> Pixels) {
        if (X >= _Data->Width || Pixels.size() != _Data->Height) {
            throw Error("LiveTexture::WriteColumn: column out of range");
        }
        for (uint32_t y = 0; y < _Data->Height; ++y) {
            _Data->Pixels[static_cast<size_t>(y) * _Data->Width + X] = Pixels[y];
        }
        _Data->Dirty = true;
    }

    void LiveTexture::WriteAll(std::span<const uint32_t> Pixels) {
        if (Pixels.size() != _Data->Pixels.size()) { throw Error("LiveTexture::WriteAll: pixel count does not match"); }
        std::memcpy(_Data->Pixels.data(), Pixels.data(), Pixels.size_bytes());
        _Data->Dirty = true;
    }

    LiveCanvas::LiveCanvas(LiveRecording& Recording, const Size& Extent) : _Recording(Recording), _Extent(Extent) {}

    void LiveCanvas::FillRect(const Rect& Bounds, const Color& Fill) {
        if (Bounds.IsEmpty() || Fill.A <= 0.0f) { return; }
        _Recording.AddInstance(
          MakeInstance(Bounds.Origin(), {Bounds.Right(), Bounds.Bottom()}, 0.0f, LivePrimitiveKind::Rect, Fill));
    }

    void LiveCanvas::DrawLine(const Vec2& Start, const Vec2& End, float Width, const Color& Stroke) {
        if (Width <= 0.0f || Stroke.A <= 0.0f) { return; }
        _Recording.AddInstance(MakeInstance(Start, End, Width, LivePrimitiveKind::Line, Stroke));
    }

    void LiveCanvas::DrawPolyline(std::span<const Vec2> Points, float Width, const Color& Stroke) {
        if (Points.size() < 2 || Width <= 0.0f || Stroke.A <= 0.0f) { return; }
        _Recording.Instances.reserve(_Recording.Instances.size() + Points.size() - 1);
        for (size_t i = 1; i < Points.size(); ++i) {
            _Recording.AddInstance(MakeInstance(Points[i - 1], Points[i], Width, LivePrimitiveKind::Line, Stroke));
        }
    }

    void LiveCanvas::FillColumns(
      float Left, float ColumnWidth, float Baseline, std::span<const float> Tops, const Color& Fill) {
        if (Fill.A <= 0.0f || ColumnWidth <= 0.0f) { return; }
        _Recording.Instances.reserve(_Recording.Instances.size() + Tops.size());
        for (size_t i = 0; i < Tops.size(); ++i) {
            const float X      = Left + static_cast<float>(i) * ColumnWidth;
            const float Top    = std::min(Tops[i], Baseline);
            const float Bottom = std::max(Tops[i], Baseline);
            if (Bottom - Top <= 0.0f) { continue; }
            _Recording.AddInstance(
              MakeInstance({X, Top}, {X + ColumnWidth, Bottom}, 0.0f, LivePrimitiveKind::Rect, Fill));
        }
    }

    void
    LiveCanvas::DrawTexture(const LiveTexture& Texture, const Rect& Destination, float SourceOffset, float Opacity) {
        if (Destination.IsEmpty() || Opacity <= 0.0f) { return; }
        _Recording.AddTexture({&Texture.GetData(), Destination, SourceOffset, Opacity});
    }

    size_t LiveCanvas::GetPrimitiveCount() const {
        return _Recording.Instances.size();
    }
}  // namespace XenUI
