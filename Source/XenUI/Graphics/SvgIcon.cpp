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

#include "XenUI/Graphics/SvgIcon.hpp"
#include "XenUI/Core/Error.hpp"
#include "XenUI/Core/Matrix.hpp"
#include "XenUI/Graphics/Painter.hpp"
#include "XenUI/Graphics/Path.hpp"
#include "XenUI/Graphics/StrokeStyle.hpp"

#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <numbers>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace XenUI {
    /// @brief A fill or stroke as the SVG wrote it, for drawing in the icon's own colors.
    struct SvgColorSource {
        enum class Kind : uint8_t {
            Default,  // No color given: black
            Current,  // currentColor
            Solid,
            Gradient,
        };
        Kind Type = Kind::Default;
        Color Value;
        std::string Gradient;  // The id a url(#id) reference names
    };

    struct SvgGradient {
        bool Radial    = false;
        bool UserSpace = false;  // gradientUnits="userSpaceOnUse"; otherwise fractions of the shape's bounds
        // Linear gradients run from (X1, Y1) to (X2, Y2); radial ones fill the circle at (Cx, Cy), focused at (Fx, Fy)
        float X1 = 0.0f, Y1 = 0.0f, X2 = 1.0f, Y2 = 0.0f;
        float Cx = 0.5f, Cy = 0.5f, R = 0.5f;
        std::optional<float> Fx;
        std::optional<float> Fy;
        Matrix3x2 Transform;
        GradientExtend Extend = GradientExtend::Clamp;
        std::vector<GradientStop> Stops;
        std::string Href;  // Stops come from this gradient when there are none of its own
    };

    /// @brief How a shape is painted: everything an element inherits from its ancestors, plus its own attributes.
    struct SvgPaint {
        bool Fill           = true;
        bool Stroke         = false;
        SvgColorSource FillSource;
        SvgColorSource StrokeSource;
        float StrokeWidth   = 1.0f;
        LineCap Cap         = LineCap::Flat;
        LineJoin Join       = LineJoin::MiterOrBevel;
        float MiterLimit    = 4.0f;
        FillRule Rule       = FillRule::NonZero;
        float FillOpacity   = 1.0f;
        float StrokeOpacity = 1.0f;
        /// @brief The product of the opacity of the element and its groups.
        float Opacity = 1.0f;
        /// @brief From the element's user space to the viewBox.
        Matrix3x2 Transform;
    };

    struct SvgShape {
        PathBuilder Builder;
        SvgPaint Paint;
        /// @brief Created on first draw, so parsing needs no App.
        mutable Path Geometry;
        mutable bool Built = false;
    };

    struct SvgIconData {
        Rect ViewBox {0.0f, 0.0f, 24.0f, 24.0f};
        std::vector<SvgShape> Shapes;
        std::vector<std::pair<std::string, SvgGradient>> Gradients;
    };

    namespace {
        bool IsSpace(char Character) {
            return Character == ' ' || Character == '\t' || Character == '\n' || Character == '\r';
        }

        std::string_view Trim(std::string_view Text) {
            while (!Text.empty() && IsSpace(Text.front())) {
                Text.remove_prefix(1);
            }
            while (!Text.empty() && IsSpace(Text.back())) {
                Text.remove_suffix(1);
            }
            return Text;
        }

        /// @brief Reads the numbers of SVG attribute values and path data: separated by spaces and commas, or by
        /// nothing at all when the next sign or decimal point starts a new number, as in "1-2.5.5".
        class NumberScanner {
        public:
            explicit NumberScanner(std::string_view Text) : _Text(Text) {}

            void SkipSeparators() {
                while (_Position < _Text.size() && (IsSpace(_Text[_Position]) || _Text[_Position] == ',')) {
                    ++_Position;
                }
            }

            [[nodiscard]] bool AtEnd() {
                SkipSeparators();
                return _Position >= _Text.size();
            }

            [[nodiscard]] bool AtNumber() {
                SkipSeparators();
                if (_Position >= _Text.size()) { return false; }
                const char Next = _Text[_Position];
                return (Next >= '0' && Next <= '9') || Next == '-' || Next == '+' || Next == '.';
            }

            [[nodiscard]] char Peek() {
                SkipSeparators();
                return _Position < _Text.size() ? _Text[_Position] : '\0';
            }

            void Advance() { ++_Position; }

            [[nodiscard]] float Number() {
                SkipSeparators();
                if (_Position < _Text.size() && _Text[_Position] == '+') { ++_Position; }
                float Value       = 0.0f;
                const char* Start = _Text.data() + _Position;
                const char* End   = _Text.data() + _Text.size();
                const auto Result = std::from_chars(Start, End, Value);
                if (Result.ec != std::errc {} || Result.ptr == Start) { Fail("a number"); }
                _Position = static_cast<size_t>(Result.ptr - _Text.data());
                return Value;
            }

            /// @brief An arc flag: a single 0 or 1, which may run straight into the next number.
            [[nodiscard]] bool Flag() {
                SkipSeparators();
                if (_Position >= _Text.size() || (_Text[_Position] != '0' && _Text[_Position] != '1')) {
                    Fail("an arc flag");
                }
                return _Text[_Position++] == '1';
            }

            /// @brief The rest of the text from the current position, for unit suffixes.
            [[nodiscard]] std::string_view Rest() const { return _Text.substr(_Position); }

            [[noreturn]] void Fail(std::string_view Expected) const {
                throw Error(std::format("SVG: expected {} at offset {} of \"{}\"", Expected, _Position, _Text));
            }

        private:
            std::string_view _Text;
            size_t _Position = 0;
        };

        std::optional<float> ParseNumber(std::string_view Text) {
            Text = Trim(Text);
            if (Text.empty()) { return std::nullopt; }
            NumberScanner Scanner(Text);
            if (!Scanner.AtNumber()) { return std::nullopt; }
            const float Value = Scanner.Number();
            if (Trim(Scanner.Rest()).starts_with('%')) { return std::nullopt; }
            return Value;
        }

        float NumberOr(std::string_view Text, float Fallback) {
            return ParseNumber(Text).value_or(Fallback);
        }

        std::vector<float> ParseNumbers(std::string_view Text) {
            std::vector<float> Values;
            NumberScanner Scanner(Text);
            while (Scanner.AtNumber()) {
                Values.push_back(Scanner.Number());
            }
            return Values;
        }

        float Radians(float Degrees) {
            return Degrees * std::numbers::pi_v<float> / 180.0f;
        }

        /// @brief Reads a transform list. Items apply right to left, as in SVG.
        Matrix3x2 ParseTransform(std::string_view Text) {
            Matrix3x2 Total;
            size_t Position = 0;
            while (true) {
                const size_t Open = Text.find('(', Position);
                if (Open == std::string_view::npos) { break; }
                const size_t Close = Text.find(')', Open);
                if (Close == std::string_view::npos) { throw Error("SVG: unterminated transform"); }

                std::string_view Name = Trim(Text.substr(Position, Open - Position));
                while (!Name.empty() && Name.front() == ',') {
                    Name = Trim(Name.substr(1));
                }
                const std::vector<float> Args = ParseNumbers(Text.substr(Open + 1, Close - Open - 1));
                const auto Arg                = [&Args](size_t Index, float Fallback) {
                    return Index < Args.size() ? Args[Index] : Fallback;
                };

                Matrix3x2 Item;
                if (Name == "matrix" && Args.size() == 6) {
                    Item = {Args[0], Args[1], Args[2], Args[3], Args[4], Args[5]};
                } else if (Name == "translate") {
                    Item = Matrix3x2::Translation(Arg(0, 0.0f), Arg(1, 0.0f));
                } else if (Name == "scale") {
                    Item = Matrix3x2::Scaling(Arg(0, 1.0f), Arg(1, Arg(0, 1.0f)));
                } else if (Name == "rotate") {
                    Item = Matrix3x2::Rotation(Radians(Arg(0, 0.0f)), {Arg(1, 0.0f), Arg(2, 0.0f)});
                } else if (Name == "skewX") {
                    Item = {1.0f, 0.0f, std::tan(Radians(Arg(0, 0.0f))), 1.0f, 0.0f, 0.0f};
                } else if (Name == "skewY") {
                    Item = {1.0f, std::tan(Radians(Arg(0, 0.0f))), 0.0f, 1.0f, 0.0f, 0.0f};
                } else {
                    throw Error(std::format("SVG: unsupported transform \"{}\"", Name));
                }
                Total    = Item * Total;
                Position = Close + 1;
            }
            return Total;
        }

        bool IsPainted(std::string_view Value) {
            Value = Trim(Value);
            return Value != "none" && Value != "transparent";
        }

        float Clamp01(float Value) {
            return std::clamp(Value, 0.0f, 1.0f);
        }

        bool EqualsIgnoreCase(std::string_view Left, std::string_view Right) {
            return Left.size() == Right.size() && std::equal(Left.begin(), Left.end(), Right.begin(), [](char A, char B) {
                       return std::tolower(static_cast<unsigned char>(A)) == std::tolower(static_cast<unsigned char>(B));
                   });
        }

        std::optional<uint32_t> ParseHexDigits(std::string_view Digits) {
            uint32_t Value = 0;
            for (const char Digit : Digits) {
                const auto Lower = static_cast<char>(std::tolower(static_cast<unsigned char>(Digit)));
                if (Lower >= '0' && Lower <= '9') {
                    Value = Value * 16 + static_cast<uint32_t>(Lower - '0');
                } else if (Lower >= 'a' && Lower <= 'f') {
                    Value = Value * 16 + static_cast<uint32_t>(Lower - 'a' + 10);
                } else {
                    return std::nullopt;
                }
            }
            return Value;
        }

        /// @brief An rgb() or rgba() channel: 0 to 255, or a percentage.
        float ParseChannel(std::string_view Text) {
            Text = Trim(Text);
            const bool Percent = Text.ends_with('%');
            if (Percent) { Text.remove_suffix(1); }
            const float Value = NumberOr(Text, 0.0f);
            return Clamp01(Percent ? Value / 100.0f : Value / 255.0f);
        }

        /// @brief A CSS color as SVG uses it. Returns nothing for none, currentColor and anything not understood.
        std::optional<Color> ParseColor(std::string_view Text) {
            Text = Trim(Text);
            if (Text.starts_with('#')) {
                const std::string_view Digits     = Text.substr(1);
                const std::optional<uint32_t> Hex = ParseHexDigits(Digits);
                if (!Hex) { return std::nullopt; }
                switch (Digits.size()) {
                    case 3: {
                        const auto Expand = [](uint32_t Nibble) { return static_cast<uint8_t>(Nibble * 17); };
                        return Color::FromRgba8(Expand(*Hex >> 8), Expand((*Hex >> 4) & 0xF), Expand(*Hex & 0xF));
                    }
                    case 6: return Color::FromHex(*Hex);
                    case 8: return Color::FromHex(*Hex >> 8, static_cast<float>(*Hex & 0xFF) / 255.0f);
                    default: return std::nullopt;
                }
            }
            if (Text.starts_with("rgb")) {
                const size_t Open  = Text.find('(');
                const size_t Close = Text.find(')');
                if (Open == std::string_view::npos || Close == std::string_view::npos || Close < Open) { return std::nullopt; }
                std::vector<std::string_view> Parts;
                std::string_view Inner = Text.substr(Open + 1, Close - Open - 1);
                while (!Inner.empty()) {
                    const size_t Comma = Inner.find_first_of(",/");
                    Parts.push_back(Inner.substr(0, Comma));
                    if (Comma == std::string_view::npos) { break; }
                    Inner.remove_prefix(Comma + 1);
                }
                if (Parts.size() == 1) {  // Space-separated: rgb(255 0 0)
                    Parts.clear();
                    for (std::string_view Rest = Trim(Text.substr(Open + 1, Close - Open - 1)); !Rest.empty();) {
                        const size_t Space = Rest.find(' ');
                        Parts.push_back(Rest.substr(0, Space));
                        Rest = Space == std::string_view::npos ? std::string_view {} : Trim(Rest.substr(Space));
                    }
                }
                if (Parts.size() < 3) { return std::nullopt; }
                Color Result {ParseChannel(Parts[0]), ParseChannel(Parts[1]), ParseChannel(Parts[2]), 1.0f};
                if (Parts.size() >= 4) {
                    std::string_view Alpha = Trim(Parts[3]);
                    const bool Percent     = Alpha.ends_with('%');
                    if (Percent) { Alpha.remove_suffix(1); }
                    Result.A = Clamp01(NumberOr(Alpha, 1.0f) / (Percent ? 100.0f : 1.0f));
                }
                return Result;
            }

            static constexpr std::pair<std::string_view, uint32_t> kNamed[] = {
              {"black", 0x000000},  {"white", 0xFFFFFF},   {"red", 0xFF0000},    {"green", 0x008000},
              {"lime", 0x00FF00},   {"blue", 0x0000FF},    {"yellow", 0xFFFF00}, {"cyan", 0x00FFFF},
              {"aqua", 0x00FFFF},   {"magenta", 0xFF00FF}, {"fuchsia", 0xFF00FF}, {"gray", 0x808080},
              {"grey", 0x808080},   {"silver", 0xC0C0C0},  {"maroon", 0x800000}, {"olive", 0x808000},
              {"navy", 0x000080},   {"purple", 0x800080},  {"teal", 0x008080},   {"orange", 0xFFA500},
            };
            for (const auto& [Name, Rgb] : kNamed) {
                if (EqualsIgnoreCase(Text, Name)) { return Color::FromHex(Rgb); }
            }
            return std::nullopt;
        }

        /// @brief What a fill or stroke value paints with. Values that aren't understood keep the inherited source.
        SvgColorSource ColorSourceOf(std::string_view Value, const SvgColorSource& Inherited) {
            Value = Trim(Value);
            if (EqualsIgnoreCase(Value, "currentColor")) { return {.Type = SvgColorSource::Kind::Current}; }
            if (Value.starts_with("url(")) {
                const size_t Hash  = Value.find('#');
                const size_t Close = Value.find(')');
                if (Hash == std::string_view::npos || Close == std::string_view::npos || Close < Hash) { return Inherited; }
                return {.Type = SvgColorSource::Kind::Gradient, .Gradient = std::string(Trim(Value.substr(Hash + 1, Close - Hash - 1)))};
            }
            if (const std::optional<Color> Solid = ParseColor(Value)) {
                return {.Type = SvgColorSource::Kind::Solid, .Value = *Solid};
            }
            return Inherited;
        }

        /// @brief Applies one presentation attribute or style property. Unknown ones are ignored.
        void ApplyProperty(SvgPaint& Paint, std::string_view Name, std::string_view Value) {
            Value = Trim(Value);
            if (Value == "inherit") { return; }
            if (Name == "fill") {
                Paint.Fill       = IsPainted(Value);
                Paint.FillSource = ColorSourceOf(Value, Paint.FillSource);
            } else if (Name == "stroke") {
                Paint.Stroke       = IsPainted(Value);
                Paint.StrokeSource = ColorSourceOf(Value, Paint.StrokeSource);
            } else if (Name == "stroke-width") {
                Paint.StrokeWidth = NumberOr(Value, Paint.StrokeWidth);
            } else if (Name == "stroke-linecap") {
                Paint.Cap = Value == "round" ? LineCap::Round : (Value == "square" ? LineCap::Square : LineCap::Flat);
            } else if (Name == "stroke-linejoin") {
                Paint.Join =
                  Value == "round" ? LineJoin::Round : (Value == "bevel" ? LineJoin::Bevel : LineJoin::MiterOrBevel);
            } else if (Name == "stroke-miterlimit") {
                Paint.MiterLimit = NumberOr(Value, Paint.MiterLimit);
            } else if (Name == "fill-rule") {
                Paint.Rule = Value == "evenodd" ? FillRule::EvenOdd : FillRule::NonZero;
            } else if (Name == "fill-opacity") {
                Paint.FillOpacity = Clamp01(NumberOr(Value, 1.0f));
            } else if (Name == "stroke-opacity") {
                Paint.StrokeOpacity = Clamp01(NumberOr(Value, 1.0f));
            } else if (Name == "opacity") {
                Paint.Opacity *= Clamp01(NumberOr(Value, 1.0f));
            }
        }

        using Attributes = std::vector<std::pair<std::string_view, std::string_view>>;

        std::string_view Find(const Attributes& List, std::string_view Name) {
            for (const auto& [Key, Value] : List) {
                if (Key == Name) { return Value; }
            }
            return {};
        }

        /// @brief The element's paint: inherited from its parent, then its attributes, then its style attribute,
        /// which wins as in CSS.
        SvgPaint Resolve(const SvgPaint& Parent, const Attributes& List) {
            SvgPaint Result = Parent;
            for (const auto& [Key, Value] : List) {
                if (Key == "transform") {
                    Result.Transform = ParseTransform(Value) * Parent.Transform;
                } else {
                    ApplyProperty(Result, Key, Value);
                }
            }
            std::string_view Style = Find(List, "style");
            while (!Style.empty()) {
                const size_t End                   = Style.find(';');
                const std::string_view Declaration = Style.substr(0, End);
                if (const size_t Colon = Declaration.find(':'); Colon != std::string_view::npos) {
                    ApplyProperty(Result, Trim(Declaration.substr(0, Colon)), Declaration.substr(Colon + 1));
                }
                if (End == std::string_view::npos) { break; }
                Style.remove_prefix(End + 1);
            }
            return Result;
        }

        struct XmlTag {
            std::string_view Name;
            Attributes List;
            bool Closing     = false;
            bool SelfClosing = false;
        };

        /// @brief Reads the tags of an XML document, skipping text, comments, processing instructions and CDATA.
        class XmlReader {
        public:
            explicit XmlReader(std::string_view Text) : _Text(Text) {}

            bool Next(XmlTag& Tag) {
                while (true) {
                    const size_t Open = _Text.find('<', _Position);
                    if (Open == std::string_view::npos) { return false; }
                    _Position = Open;
                    if (Skip("<!--", "-->") || Skip("<?", "?>") || Skip("<![CDATA[", "]]>") || Skip("<!", ">")) {
                        continue;
                    }
                    break;
                }

                Tag = {};
                ++_Position;
                if (Peek() == '/') {
                    Tag.Closing = true;
                    ++_Position;
                }
                Tag.Name = LocalName(ReadName());
                while (true) {
                    SkipSpace();
                    const char Next = Peek();
                    if (Next == '\0') { throw Error("SVG: unterminated tag"); }
                    if (Next == '>') {
                        ++_Position;
                        return true;
                    }
                    if (Next == '/') {
                        Tag.SelfClosing = true;
                        ++_Position;
                        continue;
                    }
                    const std::string_view Key = LocalName(ReadName());
                    if (Key.empty()) { throw Error(std::format("SVG: malformed tag <{}>", Tag.Name)); }
                    SkipSpace();
                    if (Peek() != '=') {
                        Tag.List.emplace_back(Key, std::string_view {});
                        continue;
                    }
                    ++_Position;
                    SkipSpace();
                    const char Quote = Peek();
                    if (Quote != '"' && Quote != '\'') { throw Error(std::format("SVG: unquoted attribute {}", Key)); }
                    const size_t End = _Text.find(Quote, _Position + 1);
                    if (End == std::string_view::npos) { throw Error("SVG: unterminated attribute value"); }
                    Tag.List.emplace_back(Key, _Text.substr(_Position + 1, End - _Position - 1));
                    _Position = End + 1;
                }
            }

        private:
            bool Skip(std::string_view Start, std::string_view End) {
                if (_Text.substr(_Position).starts_with(Start)) {
                    const size_t Found = _Text.find(End, _Position + Start.size());
                    _Position          = Found == std::string_view::npos ? _Text.size() : Found + End.size();
                    return true;
                }
                return false;
            }

            [[nodiscard]] char Peek() const { return _Position < _Text.size() ? _Text[_Position] : '\0'; }

            void SkipSpace() {
                while (_Position < _Text.size() && IsSpace(_Text[_Position])) {
                    ++_Position;
                }
            }

            std::string_view ReadName() {
                const size_t Start = _Position;
                while (_Position < _Text.size()) {
                    const char C = _Text[_Position];
                    if (IsSpace(C) || C == '=' || C == '>' || C == '/' || C == '"' || C == '\'') { break; }
                    ++_Position;
                }
                return _Text.substr(Start, _Position - Start);
            }

            /// @brief Drops a namespace prefix, so svg:path reads as path.
            static std::string_view LocalName(std::string_view Name) {
                const size_t Colon = Name.find(':');
                return Colon == std::string_view::npos ? Name : Name.substr(Colon + 1);
            }

            std::string_view _Text;
            size_t _Position = 0;
        };

        PathBuilder Ellipse(float CenterX, float CenterY, float RadiusX, float RadiusY) {
            PathBuilder Builder;
            Builder.MoveTo({CenterX + RadiusX, CenterY})
              .ArcTo({CenterX - RadiusX, CenterY}, {RadiusX, RadiusY})
              .ArcTo({CenterX + RadiusX, CenterY}, {RadiusX, RadiusY})
              .Close();
            return Builder;
        }

        PathBuilder RoundedRect(float X, float Y, float Width, float Height, float RadiusX, float RadiusY) {
            PathBuilder Builder;
            if (RadiusX <= 0.0f || RadiusY <= 0.0f) {
                Builder.MoveTo({X, Y}).LineTo({X + Width, Y}).LineTo({X + Width, Y + Height}).LineTo({X, Y + Height});
                Builder.Close();
                return Builder;
            }
            const Vec2 Radius {RadiusX, RadiusY};
            Builder.MoveTo({X + RadiusX, Y})
              .LineTo({X + Width - RadiusX, Y})
              .ArcTo({X + Width, Y + RadiusY}, Radius)
              .LineTo({X + Width, Y + Height - RadiusY})
              .ArcTo({X + Width - RadiusX, Y + Height}, Radius)
              .LineTo({X + RadiusX, Y + Height})
              .ArcTo({X, Y + Height - RadiusY}, Radius)
              .LineTo({X, Y + RadiusY})
              .ArcTo({X + RadiusX, Y}, Radius)
              .Close();
            return Builder;
        }

        PathBuilder Points(std::string_view Text, bool Closed) {
            const std::vector<float> Values = ParseNumbers(Text);
            PathBuilder Builder;
            for (size_t i = 0; i + 1 < Values.size(); i += 2) {
                const Vec2 Point {Values[i], Values[i + 1]};
                if (i == 0) {
                    Builder.MoveTo(Point);
                } else {
                    Builder.LineTo(Point);
                }
            }
            if (Closed && Values.size() >= 2) { Builder.Close(); }
            return Builder;
        }

        /// @brief The outline of a shape element, or nothing for elements that aren't shapes.
        std::optional<PathBuilder> ShapeOf(std::string_view Name, const Attributes& List, SvgPaint& Paint) {
            const auto Number = [&List](std::string_view Key, float Fallback = 0.0f) {
                return NumberOr(Find(List, Key), Fallback);
            };
            if (Name == "path") { return PathBuilder::FromSvgPathData(Find(List, "d")); }
            if (Name == "rect") {
                const float Width  = Number("width");
                const float Height = Number("height");
                if (Width <= 0.0f || Height <= 0.0f) { return std::nullopt; }
                std::optional<float> RadiusX = ParseNumber(Find(List, "rx"));
                std::optional<float> RadiusY = ParseNumber(Find(List, "ry"));
                if (!RadiusX) { RadiusX = RadiusY; }
                if (!RadiusY) { RadiusY = RadiusX; }
                return RoundedRect(Number("x"),
                                   Number("y"),
                                   Width,
                                   Height,
                                   std::min(RadiusX.value_or(0.0f), Width / 2.0f),
                                   std::min(RadiusY.value_or(0.0f), Height / 2.0f));
            }
            if (Name == "circle") {
                const float Radius = Number("r");
                if (Radius <= 0.0f) { return std::nullopt; }
                return Ellipse(Number("cx"), Number("cy"), Radius, Radius);
            }
            if (Name == "ellipse") {
                const float RadiusX = Number("rx");
                const float RadiusY = Number("ry");
                if (RadiusX <= 0.0f || RadiusY <= 0.0f) { return std::nullopt; }
                return Ellipse(Number("cx"), Number("cy"), RadiusX, RadiusY);
            }
            if (Name == "line") {
                Paint.Fill = false;
                PathBuilder Builder;
                Builder.MoveTo({Number("x1"), Number("y1")}).LineTo({Number("x2"), Number("y2")});
                return Builder;
            }
            if (Name == "polyline") { return Points(Find(List, "points"), false); }
            if (Name == "polygon") { return Points(Find(List, "points"), true); }
            return std::nullopt;
        }

        /// @brief Elements whose content is never drawn directly: definitions, metadata and unsupported features.
        bool IsSkipped(std::string_view Name) {
            constexpr std::string_view kSkipped[] = {"defs",
                                                     "clipPath",
                                                     "mask",
                                                     "pattern",
                                                     "linearGradient",
                                                     "radialGradient",
                                                     "symbol",
                                                     "marker",
                                                     "filter",
                                                     "style",
                                                     "title",
                                                     "desc",
                                                     "metadata",
                                                     "text",
                                                     "foreignObject"};
            return std::find(std::begin(kSkipped), std::end(kSkipped), Name) != std::end(kSkipped);
        }

        /// @brief A gradient coordinate: a number, or a percentage of Extent (1 for bounding box units).
        float ParseCoordinate(std::string_view Text, float Fallback, float Extent) {
            Text = Trim(Text);
            if (Text.ends_with('%')) {
                Text.remove_suffix(1);
                return NumberOr(Text, Fallback * 100.0f / Extent) / 100.0f * Extent;
            }
            return NumberOr(Text, Fallback);
        }

        SvgGradient ReadGradient(const XmlTag& Tag, const Rect& ViewBox) {
            SvgGradient Result;
            Result.Radial    = Tag.Name == "radialGradient";
            Result.UserSpace = Find(Tag.List, "gradientUnits") == "userSpaceOnUse";
            const float Width  = Result.UserSpace ? ViewBox.Width : 1.0f;
            const float Height = Result.UserSpace ? ViewBox.Height : 1.0f;
            const float Size   = Result.UserSpace ? std::sqrt((Width * Width + Height * Height) / 2.0f) : 1.0f;
            const auto Coordinate = [&](std::string_view Key, float Fallback, float Extent) {
                return ParseCoordinate(Find(Tag.List, Key), Fallback * Extent, Extent);
            };

            if (Result.Radial) {
                Result.Cx = Coordinate("cx", 0.5f, Width);
                Result.Cy = Coordinate("cy", 0.5f, Height);
                Result.R  = Coordinate("r", 0.5f, Size);
                if (!Find(Tag.List, "fx").empty()) { Result.Fx = Coordinate("fx", 0.5f, Width); }
                if (!Find(Tag.List, "fy").empty()) { Result.Fy = Coordinate("fy", 0.5f, Height); }
            } else {
                Result.X1 = Coordinate("x1", 0.0f, Width);
                Result.Y1 = Coordinate("y1", 0.0f, Height);
                Result.X2 = Coordinate("x2", 1.0f, Width);
                Result.Y2 = Coordinate("y2", 0.0f, Height);
            }
            if (const std::string_view Transform = Find(Tag.List, "gradientTransform"); !Transform.empty()) {
                Result.Transform = ParseTransform(Transform);
            }
            const std::string_view Spread = Find(Tag.List, "spreadMethod");
            Result.Extend = Spread == "reflect" ? GradientExtend::Mirror
                                                : (Spread == "repeat" ? GradientExtend::Repeat : GradientExtend::Clamp);
            if (const std::string_view Href = Find(Tag.List, "href"); Href.starts_with('#')) {
                Result.Href = std::string(Href.substr(1));
            }
            return Result;
        }

        /// @brief One stop, kept in order: SVG treats an offset before the previous one as equal to it.
        void ReadStop(SvgGradient& Target, const Attributes& List) {
            std::string_view Offset = Trim(Find(List, "offset"));
            const bool Percent      = Offset.ends_with('%');
            if (Percent) { Offset.remove_suffix(1); }
            float Position = Clamp01(NumberOr(Offset, 0.0f) / (Percent ? 100.0f : 1.0f));
            if (!Target.Stops.empty()) { Position = std::max(Position, Target.Stops.back().Position); }

            // The color and opacity may be attributes or style properties; style wins
            std::string_view StopColor   = Find(List, "stop-color");
            std::string_view StopOpacity = Find(List, "stop-opacity");
            std::string_view Style       = Find(List, "style");
            while (!Style.empty()) {
                const size_t End                   = Style.find(';');
                const std::string_view Declaration = Style.substr(0, End);
                if (const size_t Colon = Declaration.find(':'); Colon != std::string_view::npos) {
                    const std::string_view Key = Trim(Declaration.substr(0, Colon));
                    if (Key == "stop-color") { StopColor = Declaration.substr(Colon + 1); }
                    if (Key == "stop-opacity") { StopOpacity = Declaration.substr(Colon + 1); }
                }
                if (End == std::string_view::npos) { break; }
                Style.remove_prefix(End + 1);
            }

            Color Value = ParseColor(StopColor).value_or(Color::FromHex(0x000000));
            Value.A *= Clamp01(NumberOr(StopOpacity, 1.0f));
            Target.Stops.push_back({.Position = Position, .Value = Value});
        }

        const SvgGradient* FindGradient(const SvgIconData& Data, std::string_view Id) {
            for (const auto& [Name, Gradient] : Data.Gradients) {
                if (Name == Id) { return &Gradient; }
            }
            return nullptr;
        }

        /// @brief The brush for a gradient fill or stroke, in the local space of a shape with the given bounds.
        std::optional<Brush> GradientBrush(const SvgIconData& Data, const SvgGradient& Gradient, const Rect& Bounds, float Alpha) {
            // Stops may come from the gradients this one refers to
            const SvgGradient* WithStops = &Gradient;
            for (int Depth = 0; WithStops != nullptr && WithStops->Stops.empty() && !WithStops->Href.empty() && Depth < 8; ++Depth) {
                WithStops = FindGradient(Data, WithStops->Href);
            }
            if (WithStops == nullptr || WithStops->Stops.empty()) { return std::nullopt; }

            std::vector<GradientStop> Stops = WithStops->Stops;
            for (GradientStop& Stop : Stops) {
                Stop.Value.A *= Alpha;
            }
            if (Stops.size() == 1) { return Brush(Stops.front().Value); }  // A single stop paints a solid color

            // Gradient space to the shape's space: gradientTransform, then bounding box units if those are used
            Matrix3x2 Space = Gradient.Transform;
            if (!Gradient.UserSpace) {
                if (Bounds.Width <= 0.0f || Bounds.Height <= 0.0f) { return std::nullopt; }
                Space = Space * Matrix3x2::Scaling(Bounds.Width, Bounds.Height) * Matrix3x2::Translation(Bounds.X, Bounds.Y);
            }

            if (!Gradient.Radial) {
                return Brush(LinearGradient {.Start  = Space.TransformPoint({Gradient.X1, Gradient.Y1}),
                                             .End    = Space.TransformPoint({Gradient.X2, Gradient.Y2}),
                                             .Stops  = std::move(Stops),
                                             .Extend = Gradient.Extend});
            }
            const Vec2 Center = Space.TransformPoint({Gradient.Cx, Gradient.Cy});
            const Vec2 Focus  = Space.TransformPoint({Gradient.Fx.value_or(Gradient.Cx), Gradient.Fy.value_or(Gradient.Cy)});
            const Vec2 EdgeX  = Space.TransformPoint({Gradient.Cx + Gradient.R, Gradient.Cy});
            const Vec2 EdgeY  = Space.TransformPoint({Gradient.Cx, Gradient.Cy + Gradient.R});
            const auto Distance = [](const Vec2& A, const Vec2& B) { return std::hypot(A.X - B.X, A.Y - B.Y); };
            return Brush(RadialGradient {.Center      = Center,
                                         .Radius      = {Distance(EdgeX, Center), Distance(EdgeY, Center)},
                                         .FocusOffset = {Focus.X - Center.X, Focus.Y - Center.Y},
                                         .Stops       = std::move(Stops),
                                         .Extend      = Gradient.Extend});
        }

        /// @brief The brush a shape's fill or stroke uses when drawn in its own colors, or nothing to skip it.
        std::optional<Brush> ColorBrush(const SvgIconData& Data,
                                        const SvgColorSource& Source,
                                        const Rect& Bounds,
                                        float Alpha,
                                        const Color& CurrentColor) {
            switch (Source.Type) {
                case SvgColorSource::Kind::Default: return Brush(Color::FromHex(0x000000, Alpha));
                case SvgColorSource::Kind::Current: return Brush(CurrentColor.WithAlpha(CurrentColor.A * Alpha));
                case SvgColorSource::Kind::Solid: return Brush(Source.Value.WithAlpha(Source.Value.A * Alpha));
                case SvgColorSource::Kind::Gradient: {
                    // A reference to a gradient that doesn't exist paints nothing
                    const SvgGradient* Gradient = FindGradient(Data, Source.Gradient);
                    return Gradient != nullptr ? GradientBrush(Data, *Gradient, Bounds, Alpha) : std::nullopt;
                }
            }
            return std::nullopt;
        }

        /// @brief Calls Paint for each shape, with its geometry built and its transform (fitted to Bounds) pushed.
        template<class TPaint>
        void ForEachShape(const SvgIconData& Data, Painter& Canvas, const Rect& Bounds, TPaint&& Paint) {
            const Rect& Box   = Data.ViewBox;
            const float Scale = std::min(Bounds.Width / Box.Width, Bounds.Height / Box.Height);
            const Vec2 Offset {Bounds.X + (Bounds.Width - Box.Width * Scale) / 2.0f,
                               Bounds.Y + (Bounds.Height - Box.Height * Scale) / 2.0f};
            const Matrix3x2 Fit =
              Matrix3x2::Translation(-Box.X, -Box.Y) * Matrix3x2::Scaling(Scale, Scale) * Matrix3x2::Translation(Offset);

            for (const SvgShape& Shape : Data.Shapes) {
                if (!Shape.Built) {
                    Shape.Geometry = Shape.Builder.Build();
                    Shape.Built    = true;
                }
                if (Shape.Geometry.IsEmpty()) { continue; }
                Canvas.PushTransform(Shape.Paint.Transform * Fit);
                Paint(Shape);
                Canvas.PopTransform();
            }
        }

        StrokeStyle StrokeOf(const SvgPaint& Paint) {
            return {.Width = Paint.StrokeWidth, .Cap = Paint.Cap, .Join = Paint.Join, .MiterLimit = Paint.MiterLimit};
        }

        void ReadRoot(SvgIconData& Data, const Attributes& List) {
            const std::vector<float> Box = ParseNumbers(Find(List, "viewBox"));
            if (Box.size() == 4 && Box[2] > 0.0f && Box[3] > 0.0f) {
                Data.ViewBox = {Box[0], Box[1], Box[2], Box[3]};
                return;
            }
            const std::optional<float> Width  = ParseNumber(Find(List, "width"));
            const std::optional<float> Height = ParseNumber(Find(List, "height"));
            if (Width && Height && *Width > 0.0f && *Height > 0.0f) { Data.ViewBox = {0.0f, 0.0f, *Width, *Height}; }
        }

        std::string ReadFile(const std::filesystem::path& Path) {
            std::ifstream Stream(Path, std::ios::binary);
            if (!Stream) { throw Error(std::format("SVG: couldn't open {}", Path.string())); }
            std::ostringstream Contents;
            Contents << Stream.rdbuf();
            return std::move(Contents).str();
        }
    }  // namespace

    PathBuilder PathBuilder::FromSvgPathData(std::string_view Data) {
        PathBuilder Result;
        NumberScanner Input(Data);
        Vec2 Current;
        Vec2 FigureStart;
        Vec2 LastControl;
        char Command  = '\0';
        char Previous = '\0';

        while (!Input.AtEnd()) {
            if (!Input.AtNumber()) {
                Command = Input.Peek();
                Input.Advance();
            } else if (Command == '\0') {
                Input.Fail("a path command");
            }

            const bool Relative = Command >= 'a' && Command <= 'z';
            const char Kind     = Relative ? static_cast<char>(Command - 'a' + 'A') : Command;
            const Vec2 Base     = Relative ? Current : Vec2 {};
            const auto Point    = [&Input, &Base] {
                const float X = Input.Number();
                const float Y = Input.Number();
                return Vec2 {Base.X + X, Base.Y + Y};
            };
            const auto Reflected = [&](char First, char Second) {
                const bool Smooth = Previous == First || Previous == Second;
                return Smooth ? Vec2 {2.0f * Current.X - LastControl.X, 2.0f * Current.Y - LastControl.Y} : Current;
            };

            switch (Kind) {
                case 'M':
                    Current     = Point();
                    FigureStart = Current;
                    Result.MoveTo(Current);
                    Command = Relative ? 'l' : 'L';
                    break;
                case 'L':
                    Current = Point();
                    Result.LineTo(Current);
                    break;
                case 'H':
                    Current.X = Base.X + Input.Number();
                    Result.LineTo(Current);
                    break;
                case 'V':
                    Current.Y = Base.Y + Input.Number();
                    Result.LineTo(Current);
                    break;
                case 'C': {
                    const Vec2 First  = Point();
                    const Vec2 Second = Point();
                    const Vec2 End    = Point();
                    Result.CubicTo(First, Second, End);
                    LastControl = Second;
                    Current     = End;
                    break;
                }
                case 'S': {
                    const Vec2 First  = Reflected('C', 'S');
                    const Vec2 Second = Point();
                    const Vec2 End    = Point();
                    Result.CubicTo(First, Second, End);
                    LastControl = Second;
                    Current     = End;
                    break;
                }
                case 'Q': {
                    const Vec2 Control = Point();
                    const Vec2 End     = Point();
                    Result.QuadTo(Control, End);
                    LastControl = Control;
                    Current     = End;
                    break;
                }
                case 'T': {
                    const Vec2 Control = Reflected('Q', 'T');
                    const Vec2 End     = Point();
                    Result.QuadTo(Control, End);
                    LastControl = Control;
                    Current     = End;
                    break;
                }
                case 'A': {
                    const float RadiusX   = std::abs(Input.Number());
                    const float RadiusY   = std::abs(Input.Number());
                    const float Rotation  = Input.Number();
                    const bool Large      = Input.Flag();
                    const bool Sweep      = Input.Flag();
                    const Vec2 End        = Point();
                    const bool Degenerate = End.X == Current.X && End.Y == Current.Y;
                    if (Degenerate) { break; }
                    if (RadiusX == 0.0f || RadiusY == 0.0f) {
                        Result.LineTo(End);
                    } else {
                        Result.ArcTo(End,
                                     {RadiusX, RadiusY},
                                     Rotation,
                                     Sweep ? ArcSweep::Clockwise : ArcSweep::CounterClockwise,
                                     Large ? ArcSize::Large : ArcSize::Small);
                    }
                    Current = End;
                    break;
                }
                case 'Z':
                    Result.Close();
                    Current = FigureStart;
                    Command = '\0';
                    break;
                default:
                    throw Error(std::format("SVG: unknown path command '{}' in \"{}\"", Command, Data));
            }
            Previous = Kind;
        }
        return Result;
    }

    SvgIcon::SvgIcon() = default;

    SvgIcon::SvgIcon(std::shared_ptr<const SvgIconData> Data) : _Data(std::move(Data)) {}

    SvgIcon SvgIcon::Parse(std::string_view Markup) {
        auto Data = std::make_shared<SvgIconData>();
        XmlReader Reader(Markup);
        XmlTag Tag;
        std::vector<SvgPaint> Stack;
        int Skipping = 0;
        bool SawRoot = false;
        std::optional<std::pair<std::string, SvgGradient>> OpenGradient;  // Collecting its stops

        while (Reader.Next(Tag)) {
            // Gradients are read wherever they are, usually inside <defs>, and never drawn themselves
            if (Tag.Name == "linearGradient" || Tag.Name == "radialGradient") {
                if (Tag.Closing) {
                    if (OpenGradient) { Data->Gradients.push_back(std::move(*OpenGradient)); }
                    OpenGradient.reset();
                } else {
                    std::pair<std::string, SvgGradient> Entry {std::string(Find(Tag.List, "id")), ReadGradient(Tag, Data->ViewBox)};
                    if (Tag.SelfClosing) {
                        Data->Gradients.push_back(std::move(Entry));
                    } else {
                        OpenGradient = std::move(Entry);
                    }
                }
                continue;
            }
            if (Tag.Name == "stop") {
                if (!Tag.Closing && OpenGradient) { ReadStop(OpenGradient->second, Tag.List); }
                continue;
            }

            if (Tag.Closing) {
                if (Skipping > 0) {
                    --Skipping;
                } else if (!Stack.empty()) {
                    Stack.pop_back();
                }
                continue;
            }
            if (Skipping > 0 || IsSkipped(Tag.Name)) {
                if (!Tag.SelfClosing) { ++Skipping; }
                continue;
            }

            if (!SawRoot) {
                if (Tag.Name != "svg") { throw Error("SVG: the document doesn't start with an <svg> element"); }
                SawRoot = true;
                ReadRoot(*Data, Tag.List);
            }

            SvgPaint Paint = Resolve(Stack.empty() ? SvgPaint {} : Stack.back(), Tag.List);
            if (std::optional<PathBuilder> Outline = ShapeOf(Tag.Name, Tag.List, Paint)) {
                if (Paint.Fill || Paint.Stroke) {
                    Outline->SetFillRule(Paint.Rule);
                    Data->Shapes.push_back({.Builder = std::move(*Outline), .Paint = Paint});
                }
            }
            if (!Tag.SelfClosing) { Stack.push_back(Paint); }
        }

        if (!SawRoot) { throw Error("SVG: no <svg> element found"); }
        return SvgIcon(std::move(Data));
    }

    SvgIcon SvgIcon::FromFile(std::string_view FilePath) {
        return Parse(ReadFile(std::filesystem::path(std::u8string(FilePath.begin(), FilePath.end()))));
    }

    SvgIcon SvgIcon::FromResource(int ResourceId) {
        const HMODULE Executable = GetModuleHandleW(nullptr);
        const HRSRC Found        = FindResourceW(Executable, MAKEINTRESOURCEW(ResourceId), RT_RCDATA);
        const HGLOBAL Loaded     = Found != nullptr ? LoadResource(Executable, Found) : nullptr;
        const void* Bytes        = Loaded != nullptr ? LockResource(Loaded) : nullptr;
        if (Bytes == nullptr) { throw Error(std::format("SVG: no RCDATA resource {}", ResourceId)); }
        return Parse({static_cast<const char*>(Bytes), SizeofResource(Executable, Found)});
    }

    SvgIcon SvgIcon::FromPathData(std::string_view PathData, const Size& ViewBox) {
        auto Data     = std::make_shared<SvgIconData>();
        Data->ViewBox = {0.0f, 0.0f, ViewBox.Width, ViewBox.Height};
        Data->Shapes.push_back({.Builder = PathBuilder::FromSvgPathData(PathData)});
        return SvgIcon(std::move(Data));
    }

    bool SvgIcon::IsEmpty() const {
        return _Data == nullptr || _Data->Shapes.empty();
    }

    Rect SvgIcon::GetViewBox() const {
        return _Data ? _Data->ViewBox : Rect {};
    }

    void SvgIcon::Draw(Painter& Canvas, const Rect& Bounds, const Color& Tint) const {
        if (IsEmpty() || Bounds.Width <= 0.0f || Bounds.Height <= 0.0f) { return; }
        ForEachShape(*_Data, Canvas, Bounds, [&](const SvgShape& Shape) {
            const SvgPaint& Paint = Shape.Paint;
            if (Paint.Fill) {
                Canvas.FillPath(Shape.Geometry, Tint.WithAlpha(Tint.A * Paint.Opacity * Paint.FillOpacity));
            }
            if (Paint.Stroke && Paint.StrokeWidth > 0.0f) {
                Canvas.StrokePath(Shape.Geometry, Tint.WithAlpha(Tint.A * Paint.Opacity * Paint.StrokeOpacity), StrokeOf(Paint));
            }
        });
    }

    void SvgIcon::DrawInColor(Painter& Canvas, const Rect& Bounds, const Color& CurrentColor) const {
        if (IsEmpty() || Bounds.Width <= 0.0f || Bounds.Height <= 0.0f) { return; }
        ForEachShape(*_Data, Canvas, Bounds, [&](const SvgShape& Shape) {
            const SvgPaint& Paint = Shape.Paint;
            const Rect Local      = Shape.Geometry.GetBounds();
            if (Paint.Fill) {
                if (const std::optional<Brush> Fill =
                      ColorBrush(*_Data, Paint.FillSource, Local, Paint.Opacity * Paint.FillOpacity, CurrentColor)) {
                    Canvas.FillPath(Shape.Geometry, *Fill);
                }
            }
            if (Paint.Stroke && Paint.StrokeWidth > 0.0f) {
                if (const std::optional<Brush> Stroke =
                      ColorBrush(*_Data, Paint.StrokeSource, Local, Paint.Opacity * Paint.StrokeOpacity, CurrentColor)) {
                    Canvas.StrokePath(Shape.Geometry, *Stroke, StrokeOf(Paint));
                }
            }
        });
    }
}  // namespace XenUI
