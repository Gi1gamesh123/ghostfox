#pragma once
#include "web_geometry/svg_length.h"
namespace ruyijs::web_geometry {
bool SvgLengthViewport(State&, JS::HandleObject node, unsigned axis, float*);
bool SvgLengthFontFactor(State&, JS::HandleObject node, unsigned unit, float*);
}
