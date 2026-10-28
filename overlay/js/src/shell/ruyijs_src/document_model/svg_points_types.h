// Engine-facing SVG point/length value types. Only forward declarations of the
// domain records, so no-exception translation units can include it.
#pragma once
#include "domain/geometry/svg_length_value.h"
#include <memory>
#include <vector>
namespace ruyijs::domain::document_model { struct SvgPointValue; }
namespace ruyijs::document_model {
using SvgPointRef = std::shared_ptr<::ruyijs::domain::document_model::SvgPointValue>;
using SvgLengthValue = ::ruyijs::domain::geometry::SvgLengthValue;
struct SvgPointsSnapshot { bool supported = false; std::vector<SvgPointRef> base, animated; };
enum SvgPointsOperation : unsigned { SvgPointsClear, SvgPointsInitialize, SvgPointsInsert,
                                     SvgPointsReplace, SvgPointsRemove, SvgPointsAppend };
}
