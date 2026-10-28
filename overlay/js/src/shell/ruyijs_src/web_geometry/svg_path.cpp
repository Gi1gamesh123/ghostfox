#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d2d1.h>
#include <wrl/client.h>
#include "web_geometry/svg_path.h"
#pragma comment(lib, "d2d1.lib")

namespace ruyijs::web_geometry {
bool SvgPathBounds(const std::vector<ruyijs::domain::geometry::SvgPathCommand>& commands,
                   float stroke, ruyijs::domain::geometry::SvgBox* out) {
  using Microsoft::WRL::ComPtr;
  ComPtr<ID2D1Factory> factory;
  if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory.GetAddressOf()))) return false;
  ComPtr<ID2D1PathGeometry> path;
  ComPtr<ID2D1GeometrySink> sink;
  if (FAILED(factory->CreatePathGeometry(&path)) || FAILED(path->Open(&sink))) return false;
  bool open = false;
  for (const auto& command : commands) {
    const auto& v = command.values;
    switch (command.kind) {
      case 'M':
        if (open) sink->EndFigure(D2D1_FIGURE_END_OPEN);
        sink->BeginFigure({v[0],v[1]}, D2D1_FIGURE_BEGIN_FILLED); open=true; break;
      case 'Z':
        if (open) sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        open=false; break;
      case 'L': sink->AddLine({v[0],v[1]}); break;
      case 'C': sink->AddBezier({{v[0],v[1]},{v[2],v[3]},{v[4],v[5]}}); break;
      case 'Q': sink->AddQuadraticBezier({{v[0],v[1]},{v[2],v[3]}}); break;
      case 'A': sink->AddArc({{v[5],v[6]},{std::abs(v[0]),std::abs(v[1])},v[2],
          v[4] ? D2D1_SWEEP_DIRECTION_CLOCKWISE : D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,
          v[3] ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL}); break;
      default: return false;
    }
  }
  if (open) sink->EndFigure(D2D1_FIGURE_END_OPEN);
  if (FAILED(sink->Close())) return false;
  D2D1_RECT_F bounds{};
  HRESULT status;
  if (stroke > 0) {
    ComPtr<ID2D1StrokeStyle> style;
    auto properties = D2D1::StrokeStyleProperties(); properties.miterLimit=4;
    properties.lineJoin=D2D1_LINE_JOIN_MITER_OR_BEVEL;
    if (FAILED(factory->CreateStrokeStyle(properties, nullptr, 0, &style))) return false;
    status=path->GetWidenedBounds(stroke, style.Get(), nullptr, .01f, &bounds);
  } else status=path->GetBounds(nullptr, &bounds);
  if (FAILED(status)) return false;
  *out = bounds.right < bounds.left || bounds.bottom < bounds.top ? ruyijs::domain::geometry::SvgBox{}
      : ruyijs::domain::geometry::SvgBox{bounds.left,bounds.top,bounds.right-bounds.left,bounds.bottom-bounds.top};
  return true;
}
}
