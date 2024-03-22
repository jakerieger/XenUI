// Author: Jake Rieger
// Created: 3/21/2024.
//

#include "SVG.h"

#include "Color.h"
#include "Renderer.h"
#include "RuntimeError.h"

namespace Xen {
    void SVG::ParseSVGToD2DGeometry() {
        Position.X = Position.X;
        Error::RuntimeError(69, "No SVG parser implemented.");
    }

    void SVG::Update() { ParseSVGToD2DGeometry(); }

    SVG::SVG(const i64 zIndex, const str& fileName, const Offset& position, Element* child)
        : Element(zIndex, {0, 0, 0, 0}), Position(position) {
        this->Children.push_back(child);

        ParseSVGToD2DGeometry();
    }

    SVG::~SVG() {
        for (auto [Brush, Sink, Path] : ShapeData) {
            SafeRelease(&Brush);
            SafeRelease(&Path);
            SafeRelease(&Sink);
        }
        Element::~Element();
    }

    void SVG::Draw() {
        // Update();
        for (auto& [Brush, Sink, Path] : ShapeData) {
            Renderer::GetRenderTarget()->FillGeometry(Path, Brush);
        }
    }
}  // namespace Xen