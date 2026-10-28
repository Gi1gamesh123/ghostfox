// Entry geometry shared by the Intersection and Resize observers. There is no
// layout engine (D20), so every element-dependent measure is the deterministic
// zero the deviations record, and the one viewport-derived value is read from
// the installed window facade so the configured screen profile stays its single
// source. The DOMRect owner does not exist yet (T33), so a rect is a plain
// object carrying the eight DOMRectReadOnly members.
#include "web_observers/internal.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include <cmath>

namespace ruyijs::web_observers {
namespace {
bool Member(JSContext* cx, JS::HandleObject object, const char* name, double value) {
  return JS_DefineProperty(cx, object, name, value, JSPROP_ENUMERATE);
}
// One Window facade member as a non-negative finite number; zero otherwise.
bool Measure(JSContext* cx, JS::HandleObject page, const char* name, double* result) {
  *result = 0;
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, page, name, &value)) return false;
  double number = 0;
  if (!JS::ToNumber(cx, value, &number)) return false;
  if (std::isfinite(number) && number > 0) *result = number;
  return true;
}
}  // namespace

bool State::Rect(double x, double y, double width, double height,
                 JS::MutableHandleObject result) {
  JS::RootedObject object(cx, JS_NewPlainObject(cx));
  if (!object || !Member(cx, object, "x", x) || !Member(cx, object, "y", y) ||
      !Member(cx, object, "width", width) || !Member(cx, object, "height", height) ||
      !Member(cx, object, "top", y) || !Member(cx, object, "right", x + width) ||
      !Member(cx, object, "bottom", y + height) || !Member(cx, object, "left", x))
    return false;
  result.set(object);
  return true;
}

bool State::Size(double inline_size, double block_size, JS::MutableHandleObject result) {
  JS::RootedObject object(
      cx, JS_NewObjectWithGivenProto(cx, &kResizeSizeClass, resizeSizePrototype));
  if (!object) return false;
  JS::SetReservedSlot(object, 0, JS::NumberValue(inline_size));
  JS::SetReservedSlot(object, 1, JS::NumberValue(block_size));
  if (!helpers->Object("ResizeObserverSize", object)) return false;
  result.set(object);
  return true;
}

bool State::Viewport(double* width, double* height) {
  return Measure(cx, page, "innerWidth", width) && Measure(cx, page, "innerHeight", height);
}

double State::Time() { return clock ? clock(clockContext) : 0.0; }
}  // namespace ruyijs::web_observers
