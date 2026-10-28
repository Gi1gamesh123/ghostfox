#include "web_geometry/svg_dom_types.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#include <cmath>

namespace ruyijs::web_geometry {
const JSClass kSvgNumberClass = {"SVGNumber", JSCLASS_HAS_RESERVED_SLOTS(1)};
bool SvgBrandReceiver(JSContext* cx, JS::HandleValue value, const JSClass* clasp,
                      const char* member, JS::MutableHandleObject out) {
  out.set(value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  if (out && JS::GetClass(out) == clasp) return true;
  const std::string error = std::string("'") + member +
      "' called on an object that does not implement interface " + clasp->name + ".";
  return WebKit::Fail(cx, error.c_str());
}
namespace {
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject object(cx);
  if (!SvgBrandReceiver(cx, args.thisv(), &kSvgNumberClass, "get value", &object)) return false;
  args.rval().setDouble(JS::GetReservedSlot(object, 0).toDouble()); return true;
}
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject object(cx);
  if (!SvgBrandReceiver(cx, args.thisv(), &kSvgNumberClass, "set value", &object)) return false;
  double number;
  if (!JS::ToNumber(cx, args.get(0), &number)) return false;
  const float value = float(number);
  if (!std::isfinite(value)) return WebKit::Fail(cx,
      "SVGNumber.value setter: Value being assigned is not a finite floating-point value.");
  JS::SetReservedSlot(object, 0, JS::DoubleValue(value)); args.rval().setUndefined(); return true;
}
}
bool NewSvgNumberObject(State& s, double value, JS::MutableHandleObject out) {
  out.set(JS_NewObjectWithGivenProto(s.cx, &kSvgNumberClass, s.svgNumber.prototype));
  if (!out) return false;
  JS::SetReservedSlot(out, 0, JS::DoubleValue(float(value))); return true;
}
bool InstallSvgNumber(State& s) {
  JS::RootedObject plain(s.cx, JS::GetRealmObjectPrototype(s.cx));
  return s.kit.Define("SVGNumber", &kSvgNumberClass, plain, WebKit::Reject, &s.svgNumber) &&
      JS_DeleteProperty(s.cx, s.svgNumber.prototype, "constructor") &&
      s.kit.Accessor(s.svgNumber, "SVGNumber", "value", Get, Set, 0) &&
      JS_DefineProperty(s.cx, s.svgNumber.prototype, "constructor", s.svgNumber.constructor, 0);
}
}
