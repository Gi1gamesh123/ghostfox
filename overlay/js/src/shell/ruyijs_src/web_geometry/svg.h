#pragma once
#include "web_geometry/internal.h"
#include "document_model/geometry_source.h"
#include "web_styling/computed_source.h"
#include "domain/geometry/svg_values.h"

namespace ruyijs::web_geometry {
namespace svg = ruyijs::domain::geometry;
struct SvgOptions { bool clipped = false, fill = true, markers = false, stroke = false; };
struct SvgInput {
  document_model::GeometryNode node;
  web_styling::ComputedStyle style;
  svg::SvgMatrix transform;
  double viewportWidth = 300, viewportHeight = 150;
};
bool InstallSvgRect(State&);
bool InstallSvgAnimatedRect(State&);
bool NewSvgRectObject(State&, const float*, JS::HandleObject, unsigned mode, JS::MutableHandleObject);
bool SvgViewBoxResult(State&, JS::HandleValue node, unsigned role, JS::MutableHandleValue);
bool SvgMeasure(State&, JS::HandleObject, const SvgOptions&, svg::SvgTextMeasurement*, bool* rendered);
bool SvgShape(const SvgInput&, const SvgOptions&, svg::SvgBox*, bool* present);
std::string SvgAttribute(const SvgInput&, const char*);
double SvgLength(const SvgInput&, const char*, double percent, double fallback = 0);
svg::SvgMatrix SvgTransform(const std::string&);
}
