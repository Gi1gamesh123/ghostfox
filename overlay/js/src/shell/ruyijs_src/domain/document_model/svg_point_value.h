// One SVG point record: a detached value or an item of an Element's live list.
// No throwing dependencies, so engine adapters may include it.
#pragma once
#include <memory>
namespace ruyijs::domain::document_model {
struct SvgPointValue { float x = 0, y = 0; bool attached = false, animated = false; };
using SvgPointRef = std::shared_ptr<SvgPointValue>;
inline SvgPointRef SvgPointCopy(const SvgPointValue& value, bool attached, bool animated) {
  auto result = std::make_shared<SvgPointValue>(value);
  result->attached = attached; result->animated = animated; return result;
}
inline void DetachSvgPoint(const SvgPointRef& point) { point->attached = point->animated = false; }
}
