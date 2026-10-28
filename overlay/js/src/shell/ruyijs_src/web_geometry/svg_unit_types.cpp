// SVGUnitTypes is a non-concrete, constants-only Window interface in the donor.
// Its generated binding has no interface prototype object.
#include "web_geometry/internal.h"
#include "runtime/page_helpers.h"

namespace ruyijs::web_geometry {
bool InstallSvgUnitTypes(State& state) {
  JSContext* cx = state.cx;
  JS::RootedObject constructor(cx, state.kit.Function(
      WebKit::Reject, "SVGUnitTypes", 0, 0, JSFUN_CONSTRUCTOR));
  if (!constructor) return false;
  // Resolve the engine's lazy metadata before installing WebIDL constants.
  bool resolved = false;
  if (!JS_HasOwnProperty(cx, constructor, "length", &resolved) ||
      !JS_HasOwnProperty(cx, constructor, "name", &resolved)) return false;
  const char* names[] = {
      "SVG_UNIT_TYPE_UNKNOWN", "SVG_UNIT_TYPE_USERSPACEONUSE",
      "SVG_UNIT_TYPE_OBJECTBOUNDINGBOX"};
  const unsigned flags = JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT;
  for (int32_t value = 0; value < 3; ++value)
    if (!JS_DefineProperty(cx, constructor, names[value], value, flags)) return false;
  return JS_DefineProperty(cx, state.kit.page, "SVGUnitTypes", constructor, 0) &&
         state.kit.helpers->Constructor("SVGUnitTypes", constructor);
}
}
