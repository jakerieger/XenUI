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

#include "XenUI/Widgets/WindowChrome.hpp"
#include "XenUI/Graphics/Painter.hpp"
#include "XenUI/Graphics/Path.hpp"
#include "XenUI/Widgets/Basic.hpp"
#include "XenUI/Widgets/Theme.hpp"

namespace XenUI {
    namespace {
        constexpr float kGlyphSize    = 10.0f;
        constexpr Color kCloseHover   = Color::FromHex(0xC42B1C);
        constexpr Color kClosePressed = Color::FromHex(0xA02215);

        void PaintGlyph(Painter& Canvas, const Size& Extent, WindowRegion Region, bool Maximized, const Color& Ink) {
            const float Left = std::floor((Extent.Width - kGlyphSize) * 0.5f) + 0.5f;
            const float Top  = std::floor((Extent.Height - kGlyphSize) * 0.5f) + 0.5f;
            const StrokeStyle Stroke {.Width = 1.0f};
            switch (Region) {
                case WindowRegion::Minimize:
                    Canvas.DrawLine({Left, Top + kGlyphSize * 0.5f},
                                    {Left + kGlyphSize, Top + kGlyphSize * 0.5f},
                                    Ink,
                                    Stroke);
                    break;
                case WindowRegion::Maximize:
                    if (Maximized) {
                        Canvas.StrokeRoundedRect({Left, Top + 2.0f, kGlyphSize - 2.0f, kGlyphSize - 2.0f},
                                                 1.0f,
                                                 Ink,
                                                 Stroke);
                        Canvas.DrawLine({Left + 2.0f, Top}, {Left + kGlyphSize, Top}, Ink, Stroke);
                        Canvas.DrawLine({Left + kGlyphSize, Top},
                                        {Left + kGlyphSize, Top + kGlyphSize - 2.0f},
                                        Ink,
                                        Stroke);
                    } else {
                        Canvas.StrokeRoundedRect({Left, Top, kGlyphSize, kGlyphSize}, 1.0f, Ink, Stroke);
                    }
                    break;
                case WindowRegion::Close:
                    Canvas.DrawLine({Left, Top}, {Left + kGlyphSize, Top + kGlyphSize}, Ink, Stroke);
                    Canvas.DrawLine({Left + kGlyphSize, Top}, {Left, Top + kGlyphSize}, Ink, Stroke);
                    break;
                default:
                    break;
            }
        }

        Node CaptionButton(WindowChrome& Chrome, const Theme& Style, WindowRegion Region, float Width, float Height) {
            const bool Hovered   = Chrome.Hovered() == Region;
            const bool Pressed   = Chrome.Pressed() == Region && Hovered;
            const bool Maximized = Chrome.Maximized();
            const bool Active    = Chrome.Active();
            const bool Closing   = Region == WindowRegion::Close;

            Color Fill = Style.Hover.WithAlpha(0.0f);
            Color Ink  = Active ? Style.Text : Style.TextMuted;
            if (Closing && (Hovered || Pressed)) {
                Fill = Pressed ? kClosePressed : kCloseHover;
                Ink  = Color::FromHex(0xFFFFFF);
            } else if (Pressed) {
                Fill = Style.Pressed;
            } else if (Hovered) {
                Fill = Style.Hover;
            }

            return RawCaptionButton {
              .Region = Region,
              .Child =
                Container {
                  .Width      = Width,
                  .Height     = Height,
                  .Background = Fill,
                  .Child      = CustomPaint {.OnPaint =
                                          [Region, Maximized, Ink](Painter& Canvas, const Size& Extent) {
                                              PaintGlyph(Canvas, Extent, Region, Maximized, Ink);
                                          }},
                },
            };
        }
    }  // namespace

    WindowChrome* WindowChrome::Of(BuildContext& Context) {
        const WindowChromeHandle* Handle = Context.Find<WindowChromeHandle>();
        return Handle != nullptr ? Handle->Chrome : nullptr;
    }

    Node WindowControls::Build(BuildContext& Context) const {
        WindowChrome* Chrome = WindowChrome::Of(Context);
        if (Chrome == nullptr) { return SizedBox {}; }
        const Theme& Style = Theme::Of(Context);
        return Row {
          .MainSize = MainAxisSize::Min,
          .Children =
            {
              CaptionButton(*Chrome, Style, WindowRegion::Minimize, ButtonWidth, Height),
              CaptionButton(*Chrome, Style, WindowRegion::Maximize, ButtonWidth, Height),
              CaptionButton(*Chrome, Style, WindowRegion::Close, ButtonWidth, Height),
            },
        };
    }
}  // namespace XenUI
