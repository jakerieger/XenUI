// Copyright (C) 2024 Jake Rieger
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

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