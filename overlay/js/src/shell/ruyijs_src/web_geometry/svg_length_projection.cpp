#include "web_geometry/svg_length.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
namespace ruyijs::web_geometry {
namespace {
const JSClass kSvgAnimatedLengthClass = {"SVGAnimatedLength", JSCLASS_HAS_RESERVED_SLOTS(2)};
bool Result(State& s, JS::HandleValue node, unsigned field, unsigned role, JS::MutableHandleValue result) {
  SvgLengthValue value;
  void* factory = nullptr;
  if (!s.svgPorts.lengthRead || !s.svgPorts.lengthRead(s.svgPorts.context, node, field, &value, &factory)) return false;
  if (!factory) return WebKit::Fail(s.cx, "SVG length original factory is not installed");
  return static_cast<Realm*>(factory)->SvgLengthView(node, field, role, result);
}
bool AnimatedGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Self(args);
  const unsigned role = WebKit::Slot(args);
  JS::RootedObject object(cx, args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (!object || JS::GetClass(object) != &kSvgAnimatedLengthClass)
    return WebKit::Fail(cx, role == 1
        ? "'get baseVal' called on an object that does not implement interface SVGAnimatedLength."
        : "'get animVal' called on an object that does not implement interface SVGAnimatedLength.");
  JS::RootedValue node(cx, JS::GetReservedSlot(object, 0));
  const unsigned field = JS::GetReservedSlot(object, 1).toInt32();
  return JS_WrapValue(cx, &node) && Result(s, node, field, role, args.rval());
}
bool AttributeGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Self(args);
  const unsigned field = WebKit::Slot(args);
  return s.svgPorts.lengthCheck && s.svgPorts.lengthCheck(s.svgPorts.context, args.thisv(), field) &&
      Result(s, args.thisv(), field, 0, args.rval());
}
bool Create(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Self(args);
  if (!s.svgPorts.elementCheck || !s.svgPorts.elementCheck(s.svgPorts.context, args.thisv(),
      "SVGSVGElement", "createSVGLength")) return false;
  JS::RootedObject object(cx), empty(cx);
  if (!NewSvgLengthObject(s, empty, 0, 0, &object)) return false;
  args.rval().setObject(*object); return true;
}
}
bool InstallSvgAnimatedLength(State& s) {
  JS::RootedObject plain(s.cx, JS::GetRealmObjectPrototype(s.cx));
  return s.kit.Define("SVGAnimatedLength", &kSvgAnimatedLengthClass, plain, WebKit::Reject, &s.svgAnimatedLength) &&
      JS_DeleteProperty(s.cx, s.svgAnimatedLength.prototype, "constructor") &&
      s.kit.Accessor(s.svgAnimatedLength, "SVGAnimatedLength", "baseVal", AnimatedGet, nullptr, 1) &&
      s.kit.Accessor(s.svgAnimatedLength, "SVGAnimatedLength", "animVal", AnimatedGet, nullptr, 2) &&
      JS_DefineProperty(s.cx, s.svgAnimatedLength.prototype, "constructor", s.svgAnimatedLength.constructor, 0);
}
bool Realm::SvgLengthView(JS::HandleValue node, unsigned field, unsigned role, JS::MutableHandleValue result) {
  State& s = *state_;
  {
    JSAutoRealm realm(s.cx, s.kit.page);
    JS::RootedValue source(s.cx, node);
    JS::RootedObject object(s.cx), empty(s.cx);
    if (!JS_WrapValue(s.cx, &source) || !s.svgPorts.lengthView ||
        !s.svgPorts.lengthView(s.svgPorts.context, source, field, role, empty, &object)) return false;
    if (!object) {
      JS::RootedObject parent(s.cx, &source.toObject());
      if (role) {
        if (!NewSvgLengthObject(s, parent, field, role, &object)) return false;
      } else {
        object = JS_NewObjectWithGivenProto(s.cx, &kSvgAnimatedLengthClass, s.svgAnimatedLength.prototype);
        if (!object) return false;
        JS::SetReservedSlot(object, 0, source);
        JS::SetReservedSlot(object, 1, JS::Int32Value(field));
      }
      if (!s.svgPorts.lengthView(s.svgPorts.context, source, field, role, object, &object)) return false;
    }
    result.setObject(*object);
  }
  return JS_WrapValue(s.cx, result);
}
bool Realm::InstallSvgLengthProjection(JS::HandleObject prototype, unsigned field) {
  State& s = *state_;
  JSAutoRealm realm(s.cx, s.kit.page);
  WebFace face(s.cx); face.prototype = prototype;
  if (field >= std::size(domain::document_model::kSvgLengthFields))
    return s.kit.Method(face, "SVGSVGElement", "createSVGLength", Create, 0, 0);
  const auto& info = domain::document_model::kSvgLengthFields[field];
  return s.kit.Accessor(face, info.interfaceName, info.member, AttributeGet, nullptr, field);
}
}
