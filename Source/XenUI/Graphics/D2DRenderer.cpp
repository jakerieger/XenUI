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

#include "XenUI/Graphics/D2DRenderer.hpp"
#include "XenUI/Graphics/D2DConvert.hpp"
#include "XenUI/Graphics/GraphicsDevice.hpp"
#include "XenUI/Graphics/ImageData.hpp"
#include "XenUI/Graphics/PathData.hpp"
#include "XenUI/Graphics/TextLayoutData.hpp"
#include "XenUI/Platform/HResult.hpp"

#include <bit>
#include <functional>

namespace XenUI {
    namespace {
        constexpr size_t kMaxCachedGradients    = 256;
        constexpr size_t kMaxCachedStrokeStyles = 256;

        void HashCombine(size_t& Seed, size_t Value) {
            Seed ^= Value + 0x9E3779B97F4A7C15ull + (Seed << 6) + (Seed >> 2);
        }

        void HashFloat(size_t& Seed, float Value) {
            HashCombine(Seed, std::bit_cast<uint32_t>(Value));
        }

        D2D1_EXTEND_MODE ToD2D(GradientExtend Extend) {
            switch (Extend) {
                case GradientExtend::Repeat:
                    return D2D1_EXTEND_MODE_WRAP;
                case GradientExtend::Mirror:
                    return D2D1_EXTEND_MODE_MIRROR;
                default:
                    return D2D1_EXTEND_MODE_CLAMP;
            }
        }

        D2D1_CAP_STYLE ToD2D(LineCap Cap) {
            switch (Cap) {
                case LineCap::Square:
                    return D2D1_CAP_STYLE_SQUARE;
                case LineCap::Round:
                    return D2D1_CAP_STYLE_ROUND;
                case LineCap::Triangle:
                    return D2D1_CAP_STYLE_TRIANGLE;
                default:
                    return D2D1_CAP_STYLE_FLAT;
            }
        }

        D2D1_LINE_JOIN ToD2D(LineJoin Join) {
            switch (Join) {
                case LineJoin::Bevel:
                    return D2D1_LINE_JOIN_BEVEL;
                case LineJoin::Round:
                    return D2D1_LINE_JOIN_ROUND;
                case LineJoin::MiterOrBevel:
                    return D2D1_LINE_JOIN_MITER_OR_BEVEL;
                default:
                    return D2D1_LINE_JOIN_MITER;
            }
        }

        D2D1_INTERPOLATION_MODE ToD2D(ImageInterpolation Interpolation) {
            switch (Interpolation) {
                case ImageInterpolation::NearestNeighbor:
                    return D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
                case ImageInterpolation::HighQuality:
                    return D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC;
                default:
                    return D2D1_INTERPOLATION_MODE_LINEAR;
            }
        }

        StrokeStyle MakeStrokeKey(const StrokeStyle& Stroke) {
            StrokeStyle Key = Stroke;
            Key.Width       = 0.0f;
            return Key;
        }

        bool IsDefaultStroke(const StrokeStyle& Stroke) {
            return Stroke.Cap == LineCap::Flat && Stroke.Join == LineJoin::Miter && Stroke.MiterLimit == 10.0f &&
                   Stroke.Dashes.empty();
        }

        enum class StackEntry : uint8_t { Transform, Clip, Opacity };

        class Replayer {
        public:
            Replayer(ID2D1DeviceContext& Context, GraphicsDevice& Device, const Matrix3x2& Base)
                : _Context(Context), _Device(Device), _Resources(Device.GetResources()), _Base(Base) {
                ApplyTransform();
            }

            ~Replayer() {
                while (!_Stack.empty()) {
                    PopEntry(_Stack.back());
                }
            }

            Replayer(const Replayer&)            = delete;
            Replayer& operator=(const Replayer&) = delete;

            void operator()(const PushTransformCommand& Command) {
                _TransformStack.push_back(_Local);
                _Local = Command.Transform * _Local;
                _Stack.push_back(StackEntry::Transform);
                ApplyTransform();
            }

            void operator()(const PopTransformCommand&) { PopIfTop(StackEntry::Transform); }

            void operator()(const PushClipCommand& Command) {
                _Context.PushAxisAlignedClip(ToD2D(Command.Bounds), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                _Stack.push_back(StackEntry::Clip);
            }

            void operator()(const PopClipCommand&) { PopIfTop(StackEntry::Clip); }

            void operator()(const PushOpacityCommand& Command) {
                _Context.PushLayer(D2D1::LayerParameters1(D2D1::InfiniteRect(),
                                                          nullptr,
                                                          D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                                                          D2D1::IdentityMatrix(),
                                                          Command.Opacity),
                                   nullptr);
                _Stack.push_back(StackEntry::Opacity);
            }

            void operator()(const PopOpacityCommand&) { PopIfTop(StackEntry::Opacity); }

            void operator()(const FillRectCommand& Command) {
                _Context.FillRectangle(ToD2D(Command.Bounds), GetBrush(Command.Paint));
            }

            void operator()(const StrokeRectCommand& Command) {
                _Context.DrawRectangle(ToD2D(Command.Bounds),
                                       GetBrush(Command.Paint),
                                       Command.Stroke.Width,
                                       GetStroke(Command.Stroke));
            }

            void operator()(const FillRoundedRectCommand& Command) {
                _Context.FillRoundedRectangle(D2D1::RoundedRect(ToD2D(Command.Bounds), Command.Radius, Command.Radius),
                                              GetBrush(Command.Paint));
            }

            void operator()(const StrokeRoundedRectCommand& Command) {
                _Context.DrawRoundedRectangle(D2D1::RoundedRect(ToD2D(Command.Bounds), Command.Radius, Command.Radius),
                                              GetBrush(Command.Paint),
                                              Command.Stroke.Width,
                                              GetStroke(Command.Stroke));
            }

            void operator()(const FillEllipseCommand& Command) {
                _Context.FillEllipse(D2D1::Ellipse(ToD2D(Command.Center), Command.Radius.X, Command.Radius.Y),
                                     GetBrush(Command.Paint));
            }

            void operator()(const StrokeEllipseCommand& Command) {
                _Context.DrawEllipse(D2D1::Ellipse(ToD2D(Command.Center), Command.Radius.X, Command.Radius.Y),
                                     GetBrush(Command.Paint),
                                     Command.Stroke.Width,
                                     GetStroke(Command.Stroke));
            }

            void operator()(const DrawLineCommand& Command) {
                _Context.DrawLine(ToD2D(Command.Start),
                                  ToD2D(Command.End),
                                  GetBrush(Command.Paint),
                                  Command.Stroke.Width,
                                  GetStroke(Command.Stroke));
            }

            void operator()(const FillPathCommand& Command) {
                _Context.FillGeometry(Command.Geometry.GetData()->Geometry.Get(), GetBrush(Command.Paint));
            }

            void operator()(const StrokePathCommand& Command) {
                _Context.DrawGeometry(Command.Geometry.GetData()->Geometry.Get(),
                                      GetBrush(Command.Paint),
                                      Command.Stroke.Width,
                                      GetStroke(Command.Stroke));
            }

            void operator()(const DrawTextLayoutCommand& Command) {
                _Context.DrawTextLayout(ToD2D(Command.Origin),
                                        Command.Layout.GetData()->Layout.Get(),
                                        GetBrush(Command.Paint),
                                        D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
            }

            void operator()(const DrawImageCommand& Command) {
                ID2D1Bitmap1* Bitmap = Command.Source.GetData()->GetBitmap(_Context, _Device.GetGeneration());
                const ImageDrawOptions& Options = Command.Options;
                const D2D1_RECT_F Destination   = ToD2D(Command.Destination);
                const D2D1_RECT_F Source        = Options.SourceRect ? ToD2D(*Options.SourceRect) : D2D1_RECT_F {};
                _Context.DrawBitmap(Bitmap,
                                    &Destination,
                                    Options.Opacity,
                                    ToD2D(Options.Interpolation),
                                    Options.SourceRect ? &Source : nullptr);
            }

        private:
            ID2D1Brush* GetBrush(const Brush& Paint) { return _Resources.GetBrush(_Context, Paint); }

            ID2D1StrokeStyle* GetStroke(const StrokeStyle& Stroke) {
                return _Resources.GetStrokeStyle(*_Device.GetD2DFactory(), Stroke);
            }

            void ApplyTransform() { _Context.SetTransform(ToD2D(_Local * _Base)); }

            void PopIfTop(StackEntry Entry) {
                if (!_Stack.empty() && _Stack.back() == Entry) { PopEntry(Entry); }
            }

            void PopEntry(StackEntry Entry) {
                _Stack.pop_back();
                switch (Entry) {
                    case StackEntry::Transform:
                        _Local = _TransformStack.back();
                        _TransformStack.pop_back();
                        ApplyTransform();
                        break;
                    case StackEntry::Clip:
                        _Context.PopAxisAlignedClip();
                        break;
                    case StackEntry::Opacity:
                        _Context.PopLayer();
                        break;
                }
            }

            ID2D1DeviceContext& _Context;
            GraphicsDevice& _Device;
            D2DResources& _Resources;
            Matrix3x2 _Base;
            Matrix3x2 _Local;
            std::vector<Matrix3x2> _TransformStack;
            std::vector<StackEntry> _Stack;
        };
    }  // namespace

    size_t GradientKeyHash::operator()(const GradientKey& Key) const noexcept {
        size_t Seed = static_cast<size_t>(Key.Extend);
        for (const GradientStop& Stop : Key.Stops) {
            HashFloat(Seed, Stop.Position);
            HashFloat(Seed, Stop.Value.R);
            HashFloat(Seed, Stop.Value.G);
            HashFloat(Seed, Stop.Value.B);
            HashFloat(Seed, Stop.Value.A);
        }
        return Seed;
    }

    size_t StrokeKeyHash::operator()(const StrokeStyle& Key) const noexcept {
        size_t Seed = static_cast<size_t>(Key.Cap);
        HashCombine(Seed, static_cast<size_t>(Key.Join));
        HashFloat(Seed, Key.MiterLimit);
        HashFloat(Seed, Key.DashOffset);
        for (const float Dash : Key.Dashes) {
            HashFloat(Seed, Dash);
        }
        return Seed;
    }

    void D2DResources::Reset() {
        _SolidBrush.Reset();
        _Gradients.clear();
        _StrokeStyles.clear();
    }

    ID2D1Brush* D2DResources::GetBrush(ID2D1DeviceContext& Context, const Brush& Paint) {
        if (const auto* Solid = std::get_if<Color>(&Paint)) {
            if (!_SolidBrush) {
                ThrowIfFailed(Context.CreateSolidColorBrush(ToD2D(*Solid), &_SolidBrush),
                              "ID2D1DeviceContext::CreateSolidColorBrush");
            } else {
                _SolidBrush->SetColor(ToD2D(*Solid));
            }
            return _SolidBrush.Get();
        }

        if (const auto* Linear = std::get_if<LinearGradient>(&Paint)) {
            GradientBrushes& Entry = GetGradient(Context, Linear->Stops, Linear->Extend);
            if (!Entry.Linear) {
                ThrowIfFailed(Context.CreateLinearGradientBrush(
                                D2D1::LinearGradientBrushProperties(ToD2D(Linear->Start), ToD2D(Linear->End)),
                                Entry.Stops.Get(),
                                &Entry.Linear),
                              "ID2D1DeviceContext::CreateLinearGradientBrush");
            } else {
                Entry.Linear->SetStartPoint(ToD2D(Linear->Start));
                Entry.Linear->SetEndPoint(ToD2D(Linear->End));
            }
            return Entry.Linear.Get();
        }

        const auto& Radial     = std::get<RadialGradient>(Paint);
        GradientBrushes& Entry = GetGradient(Context, Radial.Stops, Radial.Extend);
        if (!Entry.Radial) {
            ThrowIfFailed(
              Context.CreateRadialGradientBrush(D2D1::RadialGradientBrushProperties(ToD2D(Radial.Center),
                                                                                    ToD2D(Radial.FocusOffset),
                                                                                    Radial.Radius.X,
                                                                                    Radial.Radius.Y),
                                                Entry.Stops.Get(),
                                                &Entry.Radial),
              "ID2D1DeviceContext::CreateRadialGradientBrush");
        } else {
            Entry.Radial->SetCenter(ToD2D(Radial.Center));
            Entry.Radial->SetGradientOriginOffset(ToD2D(Radial.FocusOffset));
            Entry.Radial->SetRadiusX(Radial.Radius.X);
            Entry.Radial->SetRadiusY(Radial.Radius.Y);
        }
        return Entry.Radial.Get();
    }

    ID2D1StrokeStyle* D2DResources::GetStrokeStyle(ID2D1Factory1& Factory, const StrokeStyle& Stroke) {
        if (IsDefaultStroke(Stroke)) { return nullptr; }

        StrokeStyle Key = MakeStrokeKey(Stroke);
        if (const auto It = _StrokeStyles.find(Key); It != _StrokeStyles.end()) { return It->second.Get(); }
        if (_StrokeStyles.size() >= kMaxCachedStrokeStyles) { _StrokeStyles.clear(); }

        const D2D1_STROKE_STYLE_PROPERTIES1 Properties =
          D2D1::StrokeStyleProperties1(ToD2D(Stroke.Cap),
                                       ToD2D(Stroke.Cap),
                                       ToD2D(Stroke.Cap),
                                       ToD2D(Stroke.Join),
                                       Stroke.MiterLimit,
                                       Stroke.Dashes.empty() ? D2D1_DASH_STYLE_SOLID : D2D1_DASH_STYLE_CUSTOM,
                                       Stroke.DashOffset);

        Microsoft::WRL::ComPtr<ID2D1StrokeStyle1> Style;
        ThrowIfFailed(Factory.CreateStrokeStyle(Properties,
                                                Stroke.Dashes.empty() ? nullptr : Stroke.Dashes.data(),
                                                static_cast<UINT32>(Stroke.Dashes.size()),
                                                &Style),
                      "ID2D1Factory1::CreateStrokeStyle");
        return _StrokeStyles.emplace(std::move(Key), std::move(Style)).first->second.Get();
    }

    D2DResources::GradientBrushes& D2DResources::GetGradient(ID2D1DeviceContext& Context,
                                                             const std::vector<GradientStop>& Stops,
                                                             GradientExtend Extend) {
        GradientKey Key {Stops, Extend};
        if (const auto It = _Gradients.find(Key); It != _Gradients.end()) { return It->second; }
        if (_Gradients.size() >= kMaxCachedGradients) { _Gradients.clear(); }

        std::vector<D2D1_GRADIENT_STOP> NativeStops;
        NativeStops.reserve(Stops.size());
        for (const GradientStop& Stop : Stops) {
            NativeStops.push_back({Stop.Position, ToD2D(Stop.Value)});
        }

        GradientBrushes Entry;
        ThrowIfFailed(Context.CreateGradientStopCollection(NativeStops.data(),
                                                           static_cast<UINT32>(NativeStops.size()),
                                                           D2D1_GAMMA_2_2,
                                                           ToD2D(Extend),
                                                           &Entry.Stops),
                      "ID2D1DeviceContext::CreateGradientStopCollection");
        return _Gradients.emplace(std::move(Key), std::move(Entry)).first->second;
    }

    void ReplayDisplayList(ID2D1DeviceContext& Context,
                           GraphicsDevice& Device,
                           const DisplayList& List,
                           const Matrix3x2& Base) {
        Replayer Visitor(Context, Device, Base);
        for (const DrawCommand& Command : List.GetCommands()) {
            std::visit(Visitor, Command);
        }
    }
}  // namespace XenUI
