// SVG viewBox views use the original node's independent weak identity entries.
#include "web_geometry/svg.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
namespace ruyijs::web_geometry {
namespace {
const JSClass kSvgAnimatedRectClass = {"SVGAnimatedRect", JSCLASS_HAS_RESERVED_SLOTS(1)};
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const unsigned role = WebKit::Slot(args);
  JS::RootedObject object(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (!object || JS::GetClass(object) != &kSvgAnimatedRectClass) {
    return WebKit::Fail(cx, role == 1
        ? "'get baseVal' called on an object that does not implement interface SVGAnimatedRect."
        : "'get animVal' called on an object that does not implement interface SVGAnimatedRect.");
  }
  JS::RootedValue node(cx, JS::GetReservedSlot(object, 0));
  return JS_WrapValue(cx, &node) && SvgViewBoxResult(Self(args), node, role, args.rval());
}
bool ViewBox(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Self(args);
  if (!s.svgPorts.viewBoxCheck) return WebKit::Fail(cx, "SVG viewBox source port is not installed");
  return s.svgPorts.viewBoxCheck(s.svgPorts.context, args.thisv(), WebKit::Slot(args)) &&
      SvgViewBoxResult(s, args.thisv(), 0, args.rval());
}
}
bool InstallSvgAnimatedRect(State& s) {
  JS::RootedObject plain(s.cx, JS::GetRealmObjectPrototype(s.cx));
  return s.kit.Define("SVGAnimatedRect", &kSvgAnimatedRectClass, plain, WebKit::Reject, &s.svgAnimatedRect) &&
      JS_DeleteProperty(s.cx, s.svgAnimatedRect.prototype, "constructor") &&
      s.kit.Accessor(s.svgAnimatedRect, "SVGAnimatedRect", "baseVal", Get, nullptr, 1) &&
      s.kit.Accessor(s.svgAnimatedRect, "SVGAnimatedRect", "animVal", Get, nullptr, 2) &&
      JS_DefineProperty(s.cx, s.svgAnimatedRect.prototype, "constructor", s.svgAnimatedRect.constructor, 0);
}
bool SvgViewBoxResult(State& s, JS::HandleValue node, unsigned role, JS::MutableHandleValue result) {
  bool supported = false; document_model::SvgViewBoxValue value; void* factory = nullptr;
  if (!s.svgPorts.viewBoxRead || !s.svgPorts.viewBoxRead(s.svgPorts.context, node, &supported, &value, &factory))
    return false;
  if (!supported || !factory) return WebKit::Fail(s.cx, "SVG viewBox original factory is not installed");
  return static_cast<Realm*>(factory)->SvgViewBoxView(node, role, result);
}
bool Realm::SvgViewBoxView(JS::HandleValue node, unsigned role, JS::MutableHandleValue result) {
  State& s = *state_;
  {
    JSAutoRealm realm(s.cx, s.kit.page);
    JS::RootedValue source(s.cx, node);
    JS::RootedObject object(s.cx), empty(s.cx);
    if (!JS_WrapValue(s.cx, &source) || !s.svgPorts.viewBoxView ||
        !s.svgPorts.viewBoxView(s.svgPorts.context, source, role, empty, &object)) return false;
    if (!object) {
      JS::RootedObject parent(s.cx, &source.toObject());
      if (role) {
        if (!NewSvgRectObject(s, nullptr, parent, role, &object)) return false;
      } else {
        object = JS_NewObjectWithGivenProto(s.cx, &kSvgAnimatedRectClass, s.svgAnimatedRect.prototype);
        if (!object) return false;
        JS::SetReservedSlot(object, 0, source);
      }
      if (!s.svgPorts.viewBoxView(s.svgPorts.context, source, role, object, &object)) return false;
    }
    result.setObject(*object);
  }
  return JS_WrapValue(s.cx, result);
}
bool Realm::InstallSvgViewBoxProjection(JS::HandleObject prototype, unsigned index) {
  static const char* names[] = {"SVGSVGElement", "SVGSymbolElement", "SVGMarkerElement",
                                 "SVGPatternElement", "SVGViewElement"};
  State& s = *state_;
  JSAutoRealm realm(s.cx, s.kit.page);
  WebFace face(s.cx); face.prototype = prototype;
  return index < 5 && s.kit.Accessor(face, names[index], "viewBox", ViewBox, nullptr, index);
}
}
