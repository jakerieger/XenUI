//
// Created by conta on 3/21/2024.
//

#include "SVG.h"

#include "Color.h"
#include "Renderer.h"
#include "RuntimeError.h"

namespace Xen {
    void SVG::ParseSVGToD2DGeometry() {
        for (auto shape = SVGData->shapes; shape != nullptr; shape = shape->next) {
            for (auto path = shape->paths; path != nullptr; path = path->next) {
                Geometry geometry;
                auto hr =
                  Renderer::GetRenderTarget()->CreateSolidColorBrush(Colors::Black.GetD2DColor(),
                                                                     &geometry.Brush);
                if (FAILED(hr)) {
                    Error::RuntimeError(1, "Unable to create solid color brush for SVG shape");
                }
                hr = Renderer::GetD2DFactory()->CreatePathGeometry(&geometry.Path);
                if (FAILED(hr)) {
                    Error::RuntimeError(1, "Unable to create path geometry for SVG shape");
                }
                hr = geometry.Path->Open(&geometry.Sink);
                if (FAILED(hr)) {
                    Error::RuntimeError(1, "Unable to open geometry path sink");
                }
                geometry.Sink->SetFillMode(D2D1_FILL_MODE_ALTERNATE);

                for (int i = 0; i < path->npts - 1; i += 3) {
                    const float* point = &path->pts[i * 2];
                    // TODO: Figure out how to adjust position on window Resize
                    const float x = point[0];  // + Position.X / 2;
                    const float y = point[1];  // + Position.Y / 2;
                    if (i == 0) {
                        geometry.Sink->BeginFigure(D2D1::Point2F(x, y), D2D1_FIGURE_BEGIN_FILLED);
                    } else {
                        geometry.Sink->AddLine(D2D1::Point2F(x, y));
                    }
                }

                geometry.Sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                hr = geometry.Sink->Close();
                if (FAILED(hr)) {
                    Error::RuntimeError(1, "Unable to close geometry sink");
                }

                ShapeData.push_back(geometry);
            }
        }
    }

    void SVG::Update() { ParseSVGToD2DGeometry(); }

    SVG::SVG(const i64 zIndex, const str& fileName, const Offset& position, Element* child)
        : Element(zIndex, {0, 0, 0, 0}), Position(position) {
        this->Children.push_back(child);

        SVGData = nsvgParseFromFile(fileName.c_str(), "px", 96);
        if (!SVGData) {
            Error::RuntimeError(1, "Unable to parse SVG file: '%s'\n", fileName.c_str());
        }
        this->Size = Rect(position, {SVGData->width, SVGData->height});

        ParseSVGToD2DGeometry();
    }

    SVG::~SVG() {
        nsvgDelete(SVGData);
        for (auto [Brush, Sink, Path] : ShapeData) {
            SafeRelease(&Brush);
            SafeRelease(&Path);
            SafeRelease(&Sink);
        }
        Element::~Element();
    }

    void SVG::Draw() {
        this->Size = Rect(Position, {SVGData->width, SVGData->height});
        // Update();
        for (auto& [Brush, Sink, Path] : ShapeData) {
            Renderer::GetRenderTarget()->FillGeometry(Path, Brush);
        }
    }
}  // namespace Xen