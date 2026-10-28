#include "web_geometry/svg_dom_types.h"
#include "js/Realm.h"
#include <string>

namespace ruyijs::web_geometry {
const JSClass kSvgAnimatedBooleanClass = {"SVGAnimatedBoolean", JSCLASS_HAS_RESERVED_SLOTS(2)};
const JSClass kSvgAnimatedEnumerationClass = {"SVGAnimatedEnumeration", JSCLASS_HAS_RESERVED_SLOTS(2)};
const JSClass kSvgAnimatedIntegerClass = {"SVGAnimatedInteger", JSCLASS_HAS_RESERVED_SLOTS(2)};
const JSClass kSvgAnimatedNumberClass = {"SVGAnimatedNumber", JSCLASS_HAS_RESERVED_SLOTS(2)};
const JSClass kSvgAnimatedAngleClass = {"SVGAnimatedAngle", JSCLASS_HAS_RESERVED_SLOTS(2)};
const JSClass kSvgAnimatedLengthListClass = {"SVGAnimatedLengthList", JSCLASS_HAS_RESERVED_SLOTS(2)};
const JSClass kSvgAnimatedNumberListClass = {"SVGAnimatedNumberList", JSCLASS_HAS_RESERVED_SLOTS(2)};
const JSClass kSvgAnimatedPreserveAspectRatioClass = {"SVGAnimatedPreserveAspectRatio", JSCLASS_HAS_RESERVED_SLOTS(2)};
const JSClass kSvgAnimatedTransformListClass = {"SVGAnimatedTransformList", JSCLASS_HAS_RESERVED_SLOTS(2)};
namespace {
enum Kind { kBool, kEnum, kInt, kNumber, kObject };
struct Def { const JSClass* clasp; const char* name; Kind kind; };
constexpr Def kDefs[] = {
  {&kSvgAnimatedBooleanClass, "SVGAnimatedBoolean", kBool},
  {&kSvgAnimatedEnumerationClass, "SVGAnimatedEnumeration", kEnum},
  {&kSvgAnimatedIntegerClass, "SVGAnimatedInteger", kInt},
  {&kSvgAnimatedNumberClass, "SVGAnimatedNumber", kNumber},
  {&kSvgAnimatedAngleClass, "SVGAnimatedAngle", kObject},
  {&kSvgAnimatedLengthListClass, "SVGAnimatedLengthList", kObject},
  {&kSvgAnimatedNumberListClass, "SVGAnimatedNumberList", kObject},
  {&kSvgAnimatedPreserveAspectRatioClass, "SVGAnimatedPreserveAspectRatio", kObject},
  {&kSvgAnimatedTransformListClass, "SVGAnimatedTransformList", kObject},
};
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const unsigned slot = WebKit::Slot(args), index = slot / 2, field = slot % 2;
  const Def& def = kDefs[index];
  JS::RootedObject object(cx);
  if (!SvgBrandReceiver(cx, args.thisv(), def.clasp,
                        field ? "get animVal" : "get baseVal", &object)) return false;
  const JS::Value value = JS::GetReservedSlot(object, field);
  if (def.kind == kBool) args.rval().setBoolean(value.toNumber() != 0);
  else if (def.kind == kEnum || def.kind == kInt) args.rval().setInt32(value.toInt32());
  else args.rval().set(value);
  return true;
}
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const unsigned index = WebKit::Slot(args) / 2;
  const Def& def = kDefs[index];
  JS::RootedObject object(cx);
  if (!SvgBrandReceiver(cx, args.thisv(), def.clasp, "set baseVal", &object)) return false;
  if (def.kind == kBool) {
    JS::SetReservedSlot(object, 0, JS::DoubleValue(JS::ToBoolean(args.get(0)) ? 1 : 0));
  } else if (def.kind == kEnum) {
    uint32_t value;
    if (!JS::ToUint32(cx, args.get(0), &value)) return false;
    JS::SetReservedSlot(object, 0, JS::Int32Value(value & 0xffff));
  } else if (def.kind == kInt) {
    int32_t value;
    if (!JS::ToInt32(cx, args.get(0), &value)) return false;
    JS::SetReservedSlot(object, 0, JS::Int32Value(value));
  } else {
    double value;
    if (!JS::ToNumber(cx, args.get(0), &value)) return false;
    JS::SetReservedSlot(object, 0, JS::DoubleValue(value));
  }
  args.rval().setUndefined(); return true;
}
}
bool InstallSvgAnimated(State& s) {
  WebFace* faces[] = {&s.svgAnimatedBoolean, &s.svgAnimatedEnumeration, &s.svgAnimatedInteger,
      &s.svgAnimatedNumber, &s.svgAnimatedAngle, &s.svgAnimatedLengthList, &s.svgAnimatedNumberList,
      &s.svgAnimatedPreserveAspectRatio, &s.svgAnimatedTransformList};
  JS::RootedObject plain(s.cx, JS::GetRealmObjectPrototype(s.cx));
  for (unsigned i = 0; i < 9; ++i) {
    const Def& def = kDefs[i];
    WebFace& face = *faces[i];
    const bool writable = def.kind != kObject;
    if (!s.kit.Define(def.name, def.clasp, plain, WebKit::Reject, &face) ||
        !JS_DeleteProperty(s.cx, face.prototype, "constructor") ||
        !s.kit.Accessor(face, def.name, "baseVal", Get, writable ? Set : nullptr, i * 2) ||
        !s.kit.Accessor(face, def.name, "animVal", Get, nullptr, i * 2 + 1) ||
        !JS_DefineProperty(s.cx, face.prototype, "constructor", face.constructor, 0)) return false;
  }
  return true;
}
}
