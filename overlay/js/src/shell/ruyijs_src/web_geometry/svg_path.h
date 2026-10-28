#pragma once
#include "domain/geometry/svg_path.h"
#include "domain/geometry/svg_values.h"

namespace ruyijs::web_geometry {
bool SvgPathBounds(const std::vector<ruyijs::domain::geometry::SvgPathCommand>&,
                   float stroke, ruyijs::domain::geometry::SvgBox*);
}
