#include "web_geometry/svg_points.h"
#include "js/HeapAPI.h"
#include "js/Realm.h"
#include "js/TracingAPI.h"
#include "js/Wrapper.h"
#include <cmath>
#include <new>
namespace ruyijs::web_geometry {
namespace {
void Finalize(JS::GCContext*, JSObject* object) { delete SvgPointData(object); }
void Trace(JSTracer* tracer, JSObject* object) {
  auto* point = SvgPointData(object);
  if (!point) return;
  if (point->value->attached) JS::TraceEdge(tracer, &point->list, "SVGPoint original list");
  else point->list.unbarrieredSet(nullptr);
}
const JSClassOps pointOps = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, Finalize,
                            nullptr, nullptr, Trace};
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp); unsigned field = WebKit::Slot(args);
  JS::RootedObject object(cx);
  if (!SvgPointReceiver(cx, args.thisv(), field ? "get y" : "get x", &object)) return false;
  auto& point = SvgPointData(object)->value;
  args.rval().setDouble(field ? point->y : point->x); return true;
}
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp); unsigned field = WebKit::Slot(args);
  JS::RootedObject object(cx);
  if (!SvgPointReceiver(cx, args.thisv(), field ? "set y" : "set x", &object)) return false;
  double number;
  if (!JS::ToNumber(cx, args.get(0), &number)) return false;
  float value = float(number);
  const char* member = field ? "SVGPoint.y setter" : "SVGPoint.x setter";
  if (!std::isfinite(value)) {
    std::string error = std::string(member) + ": Value being assigned is not a finite floating-point value.";
    return WebKit::Fail(cx, error.c_str());
  }
  auto* point = SvgPointData(object);
  if (point->value->animated)
    return SvgPointException(Self(args), "NoModificationAllowedError", member, "Animated values cannot be set");
  if (point->value->attached) {
    JS::RootedObject list(cx, js::CheckedUnwrapStatic(point->list));
    JS::RootedValue node(cx, js::GetProxyReservedSlot(list, 0));
    State& s = Self(args);
    if (!JS_WrapValue(cx, &node) || !s.svgPorts.pointsWrite ||
        !s.svgPorts.pointsWrite(s.svgPorts.context, node, point->value, field, value)) return false;
  } else {
    // Firefox avoids replacing an equal float, preserving positive zero.
    float& current = field ? point->value->y : point->value->x;
    if (current != value) current = value;
    point->list = nullptr;
  }
  args.rval().setUndefined(); return true;
}
}
const JSClass kSvgPointClass = {"SVGPoint", JSCLASS_HAS_RESERVED_SLOTS(1) | JSCLASS_FOREGROUND_FINALIZE, &pointOps};
bool SvgPointReceiver(JSContext* cx, JS::HandleValue value, const char* member, JS::MutableHandleObject out) {
  out.set(value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  if (out && JS::GetClass(out) == &kSvgPointClass && SvgPointData(out)) return true;
  const std::string error = std::string("'") + member + "' called on an object that does not implement interface SVGPoint.";
  return WebKit::Fail(cx, error.c_str());
}
bool SvgPointException(State& s, const char* type, const char* member, const char* detail) {
  const std::string error = std::string(member) + ": " + detail;
  return s.exception ? s.exception(s.exceptionContext, type, error.c_str()) : WebKit::Fail(s.cx, error.c_str());
}
bool NewSvgPointObject(State& s, const SvgPointRef& value, JS::HandleObject list, JS::MutableHandleObject out) {
  JS::RootedObject object(s.cx, JS_NewObjectWithGivenProto(s.cx, &kSvgPointClass, s.svgPoint.prototype));
  if (!object) return false;
  auto point = std::unique_ptr<SvgPointNative>(
      new(std::nothrow) SvgPointNative{value, JS::Heap<JSObject*>(list.get())});
  if (!point) { JS_ReportOutOfMemory(s.cx); return false; }
  JS::SetReservedSlot(object, 0, JS::PrivateValue(point.release())); out.set(object); return true;
}
bool Realm::NewSvgPointValue(float x, float y, JS::MutableHandleValue result) {
  State& s = *state_;
  {
    JSAutoRealm realm(s.cx, s.kit.page);
    SvgPointRef point;
    point = std::make_shared<domain::document_model::SvgPointValue>();
    point->x = x; point->y = y;
    JS::RootedObject object(s.cx), empty(s.cx);
    if (!NewSvgPointObject(s, point, empty, &object)) return false;
    result.setObject(*object);
  }
  return JS_WrapValue(s.cx, result);
}
bool InstallSvgPoints(State& s) {
  JS::RootedObject plain(s.cx, JS::GetRealmObjectPrototype(s.cx));
  return s.kit.Define("SVGPoint", &kSvgPointClass, plain, WebKit::Reject, &s.svgPoint) &&
      JS_DeleteProperty(s.cx, s.svgPoint.prototype, "constructor") &&
      s.kit.Method(s.svgPoint, "SVGPoint", "matrixTransform", SvgPointMatrixTransform, 0, 0) &&
      s.kit.Accessor(s.svgPoint, "SVGPoint", "x", Get, Set, 0) &&
      s.kit.Accessor(s.svgPoint, "SVGPoint", "y", Get, Set, 1) &&
      JS_DefineProperty(s.cx, s.svgPoint.prototype, "constructor", s.svgPoint.constructor, 0) && InstallSvgPointList(s);
}
}
