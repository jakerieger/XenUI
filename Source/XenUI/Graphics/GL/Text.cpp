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


#include "XenUI/Graphics/Text.hpp"
#include "XenUI/AppContext.hpp"
#include "XenUI/Graphics/GL/FontLibrary.hpp"
#include "XenUI/Graphics/GL/TextLayoutData.hpp"

#include <graphemebreak.h>
#include <hb.h>
#include <linebreak.h>
#include <unibreakdef.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include <span>
#include <string_view>
#include <vector>

// Text layout for the GL backend, standing in for DirectWrite: HarfBuzz shapes, libunibreak finds grapheme and line
// break opportunities (UAX #29 and #14), and lines are broken greedily with trailing whitespace hanging past the edge,
// as DirectWrite does.
//
// TODO(linux): bidirectional text. Runs are shaped left to right, so right-to-left scripts display in logical order.

namespace XenUI {
    namespace {
        constexpr float kFitSlack    = 1.0e-3f;
        constexpr float kTabInSpaces = 4.0f;

        struct Grapheme {
            size_t Start = 0;
            size_t End   = 0;
            float Width  = 0.0f;
            bool Space   = false;
        };

        struct ShapedGlyph {
            uint32_t Face    = 0;
            uint32_t Index   = 0;
            size_t Cluster   = 0;
            float Advance    = 0.0f;
            Vec2 Offset;
        };

        /// @brief A line before alignment: a range of the paragraph's graphemes.
        struct PendingLine {
            size_t First = 0;
            size_t Last  = 0;
            size_t End   = 0;
            size_t Next  = 0;
            /// @brief Whether a hard break or the end of the text ends it, rather than wrapping.
            bool EndsParagraph = false;
        };

        uint32_t DecodeAt(std::string_view Text, size_t Offset) {
            size_t Position = Offset;
            return ub_get_next_char_utf8(reinterpret_cast<const utf8_t*>(Text.data()), Text.size(), &Position);
        }

        /// @brief Whitespace that hangs past the end of a line. Non-breaking spaces don't.
        bool IsHangingSpace(uint32_t Codepoint) {
            return Codepoint == 0x20 || Codepoint == 0x09 || Codepoint == 0x1680 ||
                   (Codepoint >= 0x2000 && Codepoint <= 0x200A && Codepoint != 0x2007) || Codepoint == 0x205F ||
                   Codepoint == 0x3000;
        }

        /// @brief Length of the hard line break at Offset, or 0 if none starts there.
        size_t LineBreakLength(std::string_view Text, size_t Offset) {
            const char Lead = Text[Offset];
            if (Lead == '\r') { return Offset + 1 < Text.size() && Text[Offset + 1] == '\n' ? 2 : 1; }
            if (Lead == '\n' || Lead == '\v' || Lead == '\f') { return 1; }
            const uint32_t Codepoint = DecodeAt(Text, Offset);
            if (Codepoint == 0x85) { return 2; }
            if (Codepoint == 0x2028 || Codepoint == 0x2029) { return 3; }
            return 0;
        }

        void InitialiseUnibreak() {
            static std::once_flag Once;
            std::call_once(Once, [] {
                init_linebreak();
                init_graphemebreak();
            });
        }

        class LayoutBuilder {
        public:
            LayoutBuilder(std::string_view Text, const Font& TextFont, const TextOptions& Options, const Size& MaxSize)
                : _Text(Text), _Options(Options), _MaxSize(MaxSize) {
                _Data         = std::make_shared<TextLayoutData>();
                _Data->Fonts  = AppContext::Get().GetGraphicsDevice().GetFonts().GetFontSet(TextFont);
                _Data->EmSize = TextFont.Size;

                const FontFace& Primary = *_Data->Fonts->GetPrimary();
                const float Scale       = Primary.GetScale(TextFont.Size);
                _Ascent                 = Primary.GetAscender() * Scale;
                _LineHeight = (Primary.GetAscender() + Primary.GetDescender() + Primary.GetLineGap()) * Scale;
            }

            std::shared_ptr<TextLayoutData> Build() {
                InitialiseUnibreak();
                FindBreaks();
                Shape();
                MeasureGraphemes();
                BreakLines();
                Trim();
                Place();
                return std::move(_Data);
            }

        private:
            void FindBreaks() {
                const auto* Bytes = reinterpret_cast<const utf8_t*>(_Text.data());
                _LineBreaks.assign(_Text.size(), LINEBREAK_NOBREAK);
                std::vector<char> GraphemeBreaks(_Text.size(), GRAPHEMEBREAK_NOBREAK);
                if (!_Text.empty()) {
                    set_linebreaks_utf8(Bytes, _Text.size(), nullptr, _LineBreaks.data());
                    set_graphemebreaks_utf8(Bytes, _Text.size(), nullptr, GraphemeBreaks.data());
                }

                _Data->Clusters = {0};
                for (size_t Byte = 0; Byte < _Text.size(); ++Byte) {
                    if (GraphemeBreaks[Byte] == GRAPHEMEBREAK_BREAK || Byte + 1 == _Text.size()) {
                        _Data->Clusters.push_back(Byte + 1);
                    }
                }
                for (size_t Index = 0; Index + 1 < _Data->Clusters.size(); ++Index) {
                    const size_t Start = _Data->Clusters[Index];
                    _Graphemes.push_back({.Start = Start,
                                          .End   = _Data->Clusters[Index + 1],
                                          .Space = IsHangingSpace(DecodeAt(_Text, Start))});
                }
            }

            uint32_t FaceIndex(const std::shared_ptr<FontFace>& Face) {
                const auto Found = std::find(_Data->Faces.begin(), _Data->Faces.end(), Face);
                if (Found != _Data->Faces.end()) { return static_cast<uint32_t>(Found - _Data->Faces.begin()); }
                _Data->Faces.push_back(Face);
                return static_cast<uint32_t>(_Data->Faces.size() - 1);
            }

            void Shape() {
                hb_buffer_t* Buffer = hb_buffer_create();
                size_t RunStart     = 0;
                uint32_t RunFace    = 0;
                for (size_t Index = 0; Index <= _Graphemes.size(); ++Index) {
                    const bool AtEnd = Index == _Graphemes.size();
                    const uint32_t Face =
                      AtEnd ? RunFace : FaceIndex(_Data->Fonts->GetFaceFor(DecodeAt(_Text, _Graphemes[Index].Start)));
                    if (Index > RunStart && (AtEnd || Face != RunFace)) {
                        ShapeRun(Buffer, RunFace, _Graphemes[RunStart].Start, _Graphemes[Index - 1].End);
                        RunStart = Index;
                    }
                    if (Index == RunStart) { RunFace = Face; }
                }
                hb_buffer_destroy(Buffer);
            }

            void ShapeRun(hb_buffer_t* Buffer, uint32_t Face, size_t Start, size_t End) {
                const FontFace& Source = *_Data->Faces[Face];
                hb_font_t* Font        = Source.GetFont();
                const float Scale      = Source.GetScale(_Data->EmSize);

                hb_buffer_clear_contents(Buffer);
                hb_buffer_set_cluster_level(Buffer, HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS);
                hb_buffer_add_utf8(Buffer,
                                   _Text.data(),
                                   static_cast<int>(_Text.size()),
                                   static_cast<unsigned>(Start),
                                   static_cast<int>(End - Start));
                hb_buffer_guess_segment_properties(Buffer);
                hb_buffer_set_direction(Buffer, HB_DIRECTION_LTR);
                hb_shape(Font, Buffer, nullptr, 0);

                hb_codepoint_t SpaceGlyph = 0;
                hb_font_get_nominal_glyph(Font, ' ', &SpaceGlyph);
                const float TabAdvance =
                  static_cast<float>(hb_font_get_glyph_h_advance(Font, SpaceGlyph)) * Scale * kTabInSpaces;

                unsigned Count                   = 0;
                const hb_glyph_info_t* Infos     = hb_buffer_get_glyph_infos(Buffer, &Count);
                const hb_glyph_position_t* Moves = hb_buffer_get_glyph_positions(Buffer, &Count);
                for (unsigned Index = 0; Index < Count; ++Index) {
                    ShapedGlyph Glyph {.Face    = Face,
                                       .Index   = Infos[Index].codepoint,
                                       .Cluster = Infos[Index].cluster,
                                       .Advance = static_cast<float>(Moves[Index].x_advance) * Scale,
                                       .Offset  = {static_cast<float>(Moves[Index].x_offset) * Scale,
                                                   -static_cast<float>(Moves[Index].y_offset) * Scale}};
                    if (_Text[Glyph.Cluster] == '\t') {
                        Glyph.Index   = SpaceGlyph;
                        Glyph.Advance = TabAdvance;
                    }
                    _Glyphs.push_back(Glyph);
                }
            }

            /// @brief Spreads each glyph's advance over the graphemes of its HarfBuzz cluster, so a ligature such as
            /// "fi" gives each letter part of its width.
            void MeasureGraphemes() {
                size_t Grapheme = 0;
                for (size_t Index = 0; Index < _Glyphs.size();) {
                    const size_t Cluster = _Glyphs[Index].Cluster;
                    float Total          = 0.0f;
                    size_t After         = Index;
                    for (; After < _Glyphs.size() && _Glyphs[After].Cluster == Cluster; ++After) {
                        Total += _Glyphs[After].Advance;
                    }
                    const size_t ClusterEnd = After < _Glyphs.size() ? _Glyphs[After].Cluster : _Text.size();
                    while (Grapheme < _Graphemes.size() && _Graphemes[Grapheme].End <= Cluster) { ++Grapheme; }
                    size_t Last = Grapheme;
                    while (Last < _Graphemes.size() && _Graphemes[Last].Start < ClusterEnd) { ++Last; }
                    const size_t Span = std::max<size_t>(1, Last - Grapheme);
                    for (size_t Each = Grapheme; Each < Last; ++Each) {
                        _Graphemes[Each].Width += Total / static_cast<float>(Span);
                    }
                    Index = After;
                }

                _Prefix.assign(_Graphemes.size() + 1, 0.0f);
                for (size_t Index = 0; Index < _Graphemes.size(); ++Index) {
                    _Prefix[Index + 1] = _Prefix[Index] + _Graphemes[Index].Width;
                }
            }

            [[nodiscard]] float Width(size_t First, size_t Last) const { return _Prefix[Last] - _Prefix[First]; }

            /// @brief Width of graphemes [First, Last) without the whitespace hanging off the end.
            [[nodiscard]] float ContentWidth(size_t First, size_t Last) const {
                while (Last > First && _Graphemes[Last - 1].Space) { --Last; }
                return Width(First, Last);
            }

            [[nodiscard]] bool CanBreakBefore(size_t Grapheme) const {
                const size_t Start = _Graphemes[Grapheme].Start;
                return Start > 0 && _LineBreaks[Start - 1] == LINEBREAK_ALLOWBREAK;
            }

            void BreakLines() {
                const bool Wrap = _Options.Wrapping == TextWrapping::Wrap && std::isfinite(_MaxSize.Width);
                const float Limit = std::max(_MaxSize.Width, 0.0f) + kFitSlack;

                size_t Grapheme = 0;
                while (true) {
                    // A paragraph runs to the next hard break.
                    size_t Last = Grapheme;
                    size_t Break = 0;
                    while (Last < _Graphemes.size() && (Break = LineBreakLength(_Text, _Graphemes[Last].Start)) == 0) {
                        ++Last;
                    }
                    const bool HardBreak   = Last < _Graphemes.size();
                    const size_t End       = HardBreak ? _Graphemes[Last].Start : _Text.size();
                    const size_t Following = HardBreak ? _Graphemes[Last].End : _Text.size();

                    size_t LineFirst = Grapheme;
                    size_t LastBreak = 0;
                    if (Wrap) {
                        for (size_t Index = Grapheme; Index < Last; ++Index) {
                            if (Index > LineFirst && CanBreakBefore(Index)) { LastBreak = Index; }
                            if (_Graphemes[Index].Space || Index == LineFirst || Width(LineFirst, Index + 1) <= Limit) {
                                continue;
                            }
                            const size_t Cut = LastBreak > LineFirst ? LastBreak : Index;
                            EmitWrapped(LineFirst, Cut);
                            LineFirst = Cut;
                            LastBreak = 0;
                            // The word that didn't fit may be too wide for a line of its own; break inside it.
                            if (Index > LineFirst && Width(LineFirst, Index + 1) > Limit) {
                                EmitWrapped(LineFirst, Index);
                                LineFirst = Index;
                            }
                        }
                    }
                    _Pending.push_back({.First         = LineFirst,
                                        .Last          = Last,
                                        .End           = End,
                                        .Next          = Following,
                                        .EndsParagraph = true});
                    if (!HardBreak) { break; }
                    Grapheme = Last + 1;
                }
            }

            void EmitWrapped(size_t First, size_t Last) {
                const size_t End = _Graphemes[Last].Start;
                _Pending.push_back({.First = First, .Last = Last, .End = End, .Next = End, .EndsParagraph = false});
            }

            /// @brief Applies ellipsis trimming: drops lines below the box and marks where visible lines are cut.
            void Trim() {
                _Cuts.assign(_Pending.size(), std::numeric_limits<size_t>::max());
                if (_Options.Trimming != TextTrimming::Ellipsis || !std::isfinite(_MaxSize.Width)) { return; }

                bool Truncated = false;
                if (std::isfinite(_MaxSize.Height)) {
                    const auto Fit = static_cast<size_t>(std::floor((_MaxSize.Height + kFitSlack) / _LineHeight));
                    const size_t Visible = std::max<size_t>(1, Fit);
                    if (_Pending.size() > Visible) {
                        _Pending.resize(Visible);
                        _Cuts.resize(Visible);
                        Truncated = true;
                    }
                }

                const std::shared_ptr<FontFace>& Face = _Data->Fonts->GetFaceFor(0x2026);
                hb_codepoint_t Glyph                  = 0;
                hb_font_get_nominal_glyph(Face->GetFont(), 0x2026, &Glyph);
                _EllipsisFace  = FaceIndex(Face);
                _EllipsisGlyph = Glyph;
                _EllipsisWidth = static_cast<float>(hb_font_get_glyph_h_advance(Face->GetFont(), Glyph)) *
                                 Face->GetScale(_Data->EmSize);

                const float Limit = std::max(_MaxSize.Width, 0.0f) + kFitSlack;
                for (size_t Index = 0; Index < _Pending.size(); ++Index) {
                    const PendingLine& Entry = _Pending[Index];
                    const bool Overflows     = ContentWidth(Entry.First, Entry.Last) > Limit;
                    if (!Overflows && !(Truncated && Index + 1 == _Pending.size())) { continue; }
                    size_t Keep = Entry.First;
                    while (Keep < Entry.Last && Width(Entry.First, Keep + 1) + _EllipsisWidth <= Limit) { ++Keep; }
                    while (Keep > Entry.First && _Graphemes[Keep - 1].Space) { --Keep; }
                    _Cuts[Index] = Keep;
                }
            }

            void Place() {
                float Widest = 0.0f;
                for (size_t Index = 0; Index < _Pending.size(); ++Index) {
                    Widest = std::max(Widest, LineWidth(Index, true));
                }
                const float TotalHeight = _LineHeight * static_cast<float>(_Pending.size());
                const Size Box {std::isfinite(_MaxSize.Width) ? std::max(_MaxSize.Width, 0.0f) : Widest,
                                std::isfinite(_MaxSize.Height) ? std::max(_MaxSize.Height, 0.0f) : TotalHeight};
                _Data->BoxSize      = Box;
                _Data->MeasuredSize = {Widest, TotalHeight};

                float OffsetY = 0.0f;
                if (_Options.VerticalAlignment == ParagraphAlignment::Center) {
                    OffsetY = (Box.Height - TotalHeight) * 0.5f;
                } else if (_Options.VerticalAlignment == ParagraphAlignment::Bottom) {
                    OffsetY = Box.Height - TotalHeight;
                }

                size_t Glyph = 0;
                float Left   = 0.0f;
                float Top    = 0.0f;
                float Right  = Box.Width;
                float Bottom = Box.Height;
                for (size_t Index = 0; Index < _Pending.size(); ++Index) {
                    const PendingLine& Entry = _Pending[Index];
                    const bool Cut           = _Cuts[Index] != std::numeric_limits<size_t>::max();
                    const size_t Shown       = Cut ? _Cuts[Index] : Entry.Last;

                    TextLayoutData::Line Line {.Start    = Entry.First < _Graphemes.size() ? _Graphemes[Entry.First].Start
                                                                                           : Entry.End,
                                               .End      = Entry.End,
                                               .Next     = Entry.Next,
                                               .Top      = OffsetY + _LineHeight * static_cast<float>(Index),
                                               .Height   = _LineHeight,
                                               .Baseline = _Ascent};
                    if (Entry.First == Entry.Last) { Line.Start = Entry.End; }

                    // Alignment ignores hanging whitespace. Justified lines spread the slack over their inner spaces,
                    // except the last line of a paragraph.
                    const float Content = LineWidth(Index, false);
                    float StartX        = 0.0f;
                    float PerSpace      = 0.0f;
                    switch (_Options.Alignment) {
                        case TextAlignment::Center:
                            StartX = (Box.Width - Content) * 0.5f;
                            break;
                        case TextAlignment::Trailing:
                            StartX = Box.Width - Content;
                            break;
                        case TextAlignment::Justified:
                            if (!Entry.EndsParagraph && !Cut) {
                                size_t Inner = Entry.Last;
                                while (Inner > Entry.First && _Graphemes[Inner - 1].Space) { --Inner; }
                                const auto Spaces = static_cast<size_t>(
                                  std::count_if(_Graphemes.begin() + static_cast<ptrdiff_t>(Entry.First),
                                                _Graphemes.begin() + static_cast<ptrdiff_t>(Inner),
                                                [](const Grapheme& Each) { return Each.Space; }));
                                if (Spaces > 0) {
                                    PerSpace = std::max(0.0f, Box.Width - Content) / static_cast<float>(Spaces);
                                }
                            }
                            break;
                        default:
                            break;
                    }

                    float X = StartX;
                    for (size_t Each = Entry.First; Each < Shown; ++Each) {
                        Line.Stops.push_back({_Graphemes[Each].Start, X});
                        X += _Graphemes[Each].Width + (_Graphemes[Each].Space ? PerSpace : 0.0f);
                    }
                    Line.Stops.push_back({Cut ? (Shown < _Graphemes.size() ? _Graphemes[Shown].Start : Entry.End)
                                              : Entry.End,
                                          X});

                    // Glyphs come in text order, so the ones on this line follow the previous line's.
                    const size_t ShownEnd = Line.Stops.back().Offset;
                    const float Baseline  = Line.Top + Line.Baseline;
                    float LineX           = StartX;
                    for (; Glyph < _Glyphs.size() && _Glyphs[Glyph].Cluster < Entry.Next; ++Glyph) {
                        const ShapedGlyph& Shaped = _Glyphs[Glyph];
                        if (Shaped.Cluster < Line.Start || (Cut && Shaped.Cluster >= ShownEnd) ||
                            Shaped.Cluster >= Entry.End) {
                            continue;
                        }
                        AddGlyph(Shaped.Face, Shaped.Index, {LineX + Shaped.Offset.X, Baseline + Shaped.Offset.Y},
                                 Left, Top, Right, Bottom);
                        LineX += Shaped.Advance;
                        if (PerSpace > 0.0f && IsHangingSpace(DecodeAt(_Text, Shaped.Cluster))) { LineX += PerSpace; }
                    }
                    if (Cut) {
                        AddGlyph(_EllipsisFace, _EllipsisGlyph, {X, Baseline}, Left, Top, Right, Bottom);
                    }
                    _Data->Lines.push_back(std::move(Line));
                }

                _Data->InkBounds = Rect::FromEdges(std::min(0.0f, Left),
                                                   std::min(0.0f, Top),
                                                   std::max(Box.Width, Right),
                                                   std::max(Box.Height, Bottom));
                _Data->Baseline  = _Ascent;
                _Data->LineCount = static_cast<uint32_t>(_Data->Lines.size());
            }

            /// @brief A line's width as shown, including the ellipsis when trimmed.
            [[nodiscard]] float LineWidth(size_t Index, bool IncludeTrailingSpace) const {
                const PendingLine& Entry = _Pending[Index];
                if (_Cuts[Index] != std::numeric_limits<size_t>::max()) {
                    return Width(Entry.First, _Cuts[Index]) + _EllipsisWidth;
                }
                return IncludeTrailingSpace ? Width(Entry.First, Entry.Last) : ContentWidth(Entry.First, Entry.Last);
            }

            void AddGlyph(uint32_t Face, uint32_t Index, const Vec2& Origin, float& Left, float& Top, float& Right,
                          float& Bottom) {
                _Data->Glyphs.push_back({.Face = Face, .Index = Index, .Origin = Origin});
                hb_glyph_extents_t Extents {};
                const FontFace& Source = *_Data->Faces[Face];
                if (hb_font_get_glyph_extents(Source.GetFont(), Index, &Extents) == 0 || Extents.width == 0) {
                    return;
                }
                const float Scale = Source.GetScale(_Data->EmSize);
                const float X0    = Origin.X + static_cast<float>(Extents.x_bearing) * Scale;
                const float Y0    = Origin.Y - static_cast<float>(Extents.y_bearing) * Scale;
                const float X1    = X0 + static_cast<float>(Extents.width) * Scale;
                const float Y1    = Y0 - static_cast<float>(Extents.height) * Scale;
                Left              = std::min({Left, X0, X1});
                Top               = std::min({Top, Y0, Y1});
                Right             = std::max({Right, X0, X1});
                Bottom            = std::max({Bottom, Y0, Y1});
            }

            std::string_view _Text;
            TextOptions _Options;
            Size _MaxSize;
            std::shared_ptr<TextLayoutData> _Data;
            float _Ascent     = 0.0f;
            float _LineHeight = 0.0f;

            std::vector<char> _LineBreaks;
            std::vector<Grapheme> _Graphemes;
            std::vector<float> _Prefix;
            std::vector<ShapedGlyph> _Glyphs;
            std::vector<PendingLine> _Pending;
            std::vector<size_t> _Cuts;

            uint32_t _EllipsisFace  = 0;
            uint32_t _EllipsisGlyph = 0;
            float _EllipsisWidth    = 0.0f;
        };
    }  // namespace

    const TextLayoutData::Line& TextLayoutData::LineAt(size_t Offset) const {
        const auto After = std::upper_bound(
          Lines.begin(), Lines.end(), Offset, [](size_t Value, const Line& Entry) { return Value < Entry.Start; });
        return After == Lines.begin() ? Lines.front() : *(After - 1);
    }

    float TextLayoutData::CaretX(const Line& Entry, size_t Offset) {
        const auto After = std::upper_bound(Entry.Stops.begin(),
                                            Entry.Stops.end(),
                                            Offset,
                                            [](size_t Value, const CaretStop& Stop) { return Value < Stop.Offset; });
        return After == Entry.Stops.begin() ? Entry.Stops.front().X : (After - 1)->X;
    }

    TextLayout::TextLayout() = default;

    TextLayout::TextLayout(std::string_view Text,
                           const Font& TextFont,
                           const TextOptions& Options,
                           const Size& MaxSize) {
        _Data = LayoutBuilder(Text, TextFont, Options, MaxSize).Build();
    }

    size_t TextLayout::HitTest(const Vec2& Point) const {
        if (!_Data || _Data->Lines.empty()) { return 0; }
        const auto Below = std::find_if(_Data->Lines.begin(),
                                        _Data->Lines.end(),
                                        [&](const TextLayoutData::Line& Entry) {
                                            return Point.Y < Entry.Top + Entry.Height;
                                        });
        const TextLayoutData::Line& Entry = Below == _Data->Lines.end() ? _Data->Lines.back() : *Below;

        // The nearest stop: past the middle of a grapheme snaps to its trailing edge. Points beyond either end,
        // including infinite ones, clamp to it.
        const float X  = std::clamp(Point.X, Entry.Stops.front().X, Entry.Stops.back().X);
        size_t Best    = Entry.Stops.front().Offset;
        float Distance = std::numeric_limits<float>::infinity();
        for (const TextLayoutData::CaretStop& Stop : Entry.Stops) {
            const float Gap = std::abs(Stop.X - X);
            if (Gap < Distance) {
                Distance = Gap;
                Best     = Stop.Offset;
            }
        }
        return Best;
    }

    Rect TextLayout::GetCaretBounds(size_t Offset) const {
        if (!_Data || _Data->Lines.empty()) { return {}; }
        const TextLayoutData::Line& Entry = _Data->LineAt(Offset);
        return {TextLayoutData::CaretX(Entry, Offset), Entry.Top, 0.0f, Entry.Height};
    }

    std::vector<Rect> TextLayout::GetRangeBounds(size_t Start, size_t End) const {
        std::vector<Rect> Result;
        if (!_Data || End <= Start) { return Result; }
        for (const TextLayoutData::Line& Entry : _Data->Lines) {
            const size_t From = std::max(Start, Entry.Start);
            const size_t To   = std::min(End, Entry.Next);
            if (From >= To) { continue; }
            const float Left  = TextLayoutData::CaretX(Entry, From);
            const float Right = TextLayoutData::CaretX(Entry, std::min(To, Entry.End));
            Result.push_back({Left, Entry.Top, std::max(0.0f, Right - Left), Entry.Height});
        }
        return Result;
    }

    std::vector<size_t> TextLayout::GetCaretStops() const {
        return _Data ? _Data->Clusters : std::vector<size_t> {0};
    }

    Size TextLayout::GetBoxSize() const {
        return _Data ? _Data->BoxSize : Size {};
    }

    Size TextLayout::GetMeasuredSize() const {
        return _Data ? _Data->MeasuredSize : Size {};
    }

    Rect TextLayout::GetInkBounds() const {
        return _Data ? _Data->InkBounds : Rect {};
    }

    float TextLayout::GetBaseline() const {
        return _Data ? _Data->Baseline : 0.0f;
    }

    uint32_t TextLayout::GetLineCount() const {
        return _Data ? _Data->LineCount : 0;
    }
}  // namespace XenUI
