//
// Created by Jake Rieger on 3/13/2024.
//

#include "Color.h"

#include <cmath>

namespace Xen {
    Color::Color() {
        InternalColor.r = 0;
        InternalColor.g = 0;
        InternalColor.b = 0;
        InternalColor.a = 0;
    }

    Color::Color(const f32 r, const f32 g, const f32 b, const f32 a) {
        InternalColor.r = r;
        InternalColor.g = g;
        InternalColor.b = b;
        InternalColor.a = a;
    }
    Color::Color(const u32 color) {
        Converters::HexToRGBA(color,
                              InternalColor.r,
                              InternalColor.g,
                              InternalColor.b,
                              InternalColor.a);
    }

    u32 Color::Value() const { return Converters::RGBAToHex(Red(), Green(), Blue(), Alpha()); }

    f32 Color::Luminance() const {
        // See <https://www.w3.org/TR/WCAG20/#relativeluminancedef>
        const auto uRed   = static_cast<u32>(Red() * 255);
        const auto uGreen = static_cast<u32>(Green() * 255);
        const auto uBlue  = static_cast<u32>(Blue() * 255);

        const auto R = LinearizeComponent(static_cast<f32>(uRed) / 0xFF);
        const auto G = LinearizeComponent(static_cast<f32>(uGreen) / 0xFF);
        const auto B = LinearizeComponent(static_cast<f32>(uBlue) / 0xFF);

        return 0.2126f * R + 0.7152f * G + 0.0722f * B;
    }

    Color Color::WithAlpha(const u32 a) const {
        return {Red(), Green(), Blue(), static_cast<f32>(a) / 255.f};
    }

    Color Color::WithAlpha(f32 a) const { return {Red(), Green(), Blue(), a}; }

    Color Color::WithRed(const u32 r) const {
        return {static_cast<f32>(r) / 255.f, Green(), Blue(), Alpha()};
    }

    Color Color::WithRed(f32 r) const { return {r, Green(), Blue(), Alpha()}; }

    Color Color::WithGreen(const u32 g) const {
        return {Red(), static_cast<f32>(g) / 255.f, Blue(), Alpha()};
    }

    Color Color::WithGreen(f32 g) const { return {Red(), g, Blue(), Alpha()}; }

    Color Color::WithBlue(const u32 b) const {
        return {Red(), Green(), static_cast<f32>(b) / 255.f, Alpha()};
    }

    Color Color::WithBlue(f32 b) const { return {Red(), Green(), b, Alpha()}; }

    Color Color::Greyscale() const {
        auto lum = Luminance();
        return {lum, lum, lum, Alpha()};
    }

    Color Color::AlphaBlend(const Color& foreground, const Color& background) {
        const f32 alpha = foreground.Alpha();
        if (alpha == 0.f) {
            return background;
        }
        const f32 invAlpha = 1.f - alpha;

        f32 backAlpha = background.Alpha();
        if (backAlpha == 1.f) {
            return {alpha * foreground.Red() + invAlpha * background.Red(),
                    alpha * foreground.Green() + invAlpha * background.Green(),
                    alpha * foreground.Blue() + invAlpha * background.Blue(),
                    1.f};
        }

        backAlpha          = (backAlpha * invAlpha) / 1.f;
        const f32 outAlpha = alpha + backAlpha;
        assert(outAlpha != 0.f);
        return {alpha * foreground.Red() + invAlpha * background.Red() / outAlpha,
                alpha * foreground.Green() + invAlpha * background.Green() / outAlpha,
                alpha * foreground.Blue() + invAlpha * background.Blue() / outAlpha,
                1.f};
    }

    u32 Color::GetAlphaFromOpacity(const f32 opacity) { return static_cast<u32>(opacity * 255); }

    Color Color::Lerp(const Color& a, const Color& b, const f32 t) {
        return {Math::Lerp(a.Red(), b.Red(), t),
                Math::Lerp(a.Green(), b.Green(), t),
                Math::Lerp(a.Blue(), b.Blue(), t),
                Math::Lerp(a.Alpha(), b.Alpha(), t)};
    }

    f32 Color::LinearizeComponent(const f32 c) {
        // https://stackoverflow.com/questions/61138110/what-is-the-correct-gamma-correction-function
        if (c <= 0.04045f) {
            return c / 12.92f;
        }

        return std::pow((c + 0.055f) / 1.055f, 2.4f);
    }
}  // namespace Xen