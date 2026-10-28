#include "web_geometry/svg_points.h"
#include <cmath>
namespace ruyijs::web_geometry {
bool SvgPointMatrixTransform(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject point(cx);
  if (!SvgPointReceiver(cx, args.thisv(), "matrixTransform", &point)) return false;
  double matrix[12] = {}; bool present[12] = {};
  static const char* fields[] = {"a", "b", "c", "d", "e", "f", "m11", "m12", "m21", "m22", "m41", "m42"};
  if (!args.get(0).isNullOrUndefined()) {
    if (!args[0].isObject())
      return WebKit::Fail(cx, "SVGPoint.matrixTransform: Argument 1 can't be converted to a dictionary.");
    JS::RootedObject input(cx, &args[0].toObject());
    for (unsigned i = 0; i < 12; ++i) {
      JS::RootedValue value(cx);
      if (!JS_GetProperty(cx, input, fields[i], &value)) return false;
      present[i] = !value.isUndefined();
      if (present[i] && !JS::ToNumber(cx, value, &matrix[i])) return false;
    }
  }
  for (unsigned i = 0; i < 6; ++i) {
    if (present[i] && present[i + 6] && matrix[i] != matrix[i + 6] &&
        !(std::isnan(matrix[i]) && std::isnan(matrix[i + 6]))) {
      const auto error = std::string("SVGPoint.matrixTransform: Matrix init unexpectedly got different values for '") +
          fields[i + 6] + "' and '" + fields[i] + "'.";
      return WebKit::Fail(cx, error.c_str());
    }
    matrix[i] = present[i + 6] ? matrix[i + 6] : present[i] ? matrix[i] : (i == 0 || i == 3 ? 1 : 0);
  }
  for (unsigned i = 0; i < 6; ++i) if (!std::isfinite(matrix[i]))
    return WebKit::Fail(cx, "SVGPoint.matrixTransform: MatrixTransform matrix is not a finite floating-point value.");
  const auto& source = *SvgPointData(point)->value;
  SvgPointRef value;
  value = std::make_shared<domain::document_model::SvgPointValue>();
  value->x = float(matrix[0] * source.x + matrix[2] * source.y + matrix[4]);
  value->y = float(matrix[1] * source.x + matrix[3] * source.y + matrix[5]);
  JS::RootedObject result(cx), empty(cx);
  if (!NewSvgPointObject(Self(args), value, empty, &result)) return false;
  args.rval().setObject(*result); return true;
}
}
