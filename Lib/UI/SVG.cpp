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
    void SVG::SVGToBitmap() {
        const auto rast = nsvgCreateRasterizer();
        if (!rast)
            Error::RuntimeError(1, "Error rasterizing SVG data");

        const auto w   = static_cast<int>(Size.Width());
        const auto h   = static_cast<int>(Size.Height());
        const auto img = static_cast<unsigned char*>(malloc(w * h * 4));
        nsvgRasterize(rast, SvgData, 0, 0, 1, img, w, h, w * 4);
        const auto hr = Renderer::GetRenderTarget()->CreateBitmap(
          D2D1::SizeU(w, h),
          img,
          w * 4,
          D2D1::BitmapProperties(
            D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
          &Bitmap);
        if (FAILED(hr)) {
            free(img);
            nsvgDeleteRasterizer(rast);
            nsvgDelete(SvgData);
            Error::RuntimeError(1, "Error creating D2D bitmap");
        }

        free(img);
        nsvgDeleteRasterizer(rast);
    }

    void SVG::Update() {}

    SVG::SVG(const i64 zIndex, const str& fileName, const Offset& position, Element* child)
        : Element(zIndex, {0, 0, 0, 0}), Position(position) {
        this->Children.push_back(child);

        SvgData = nsvgParseFromFile(fileName.c_str(), "px", 96);
        if (!SvgData) {
            Error::RuntimeError(1, "Could not parse SVG file.");
        }

        Size = {0, 0, SvgData->width, SvgData->height};

        SVGToBitmap();
    }

    SVG::~SVG() { Element::~Element(); }

    void SVG::Draw() {
        if (Bitmap) {
            Renderer::GetRenderTarget()->DrawBitmap(Bitmap, Size.GetD2DRect());
        }
    }
}  // namespace Xen