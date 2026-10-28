#include "web_geometry/svg_dom_types.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::web_geometry {
bool InstallSvgDomTypes(State& s) {
  return InstallSvgNumber(s) && InstallSvgAngle(s) && InstallSvgMatrix(s) && InstallSvgTransform(s) &&
      InstallSvgPreserveAspectRatio(s) && InstallSvgAnimated(s) && InstallSvgLists(s);
}
namespace {
bool CreateNumber(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp); State& s = Self(args);
  if (!s.svgPorts.elementCheck || !s.svgPorts.elementCheck(s.svgPorts.context, args.thisv(),
      "SVGSVGElement", "createSVGNumber")) return false;
  JS::RootedObject object(cx);
  if (!NewSvgNumberObject(s, 0, &object)) return false;
  args.rval().setObject(*object); return true;
}
bool CreateAngle(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp); State& s = Self(args);
  if (!s.svgPorts.elementCheck || !s.svgPorts.elementCheck(s.svgPorts.context, args.thisv(),
      "SVGSVGElement", "createSVGAngle")) return false;
  JS::RootedObject object(cx);
  if (!NewSvgAngleObject(s, 0, 1, &object)) return false;
  args.rval().setObject(*object); return true;
}
bool CreateMatrix(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp); State& s = Self(args);
  if (!s.svgPorts.elementCheck || !s.svgPorts.elementCheck(s.svgPorts.context, args.thisv(),
      "SVGSVGElement", "createSVGMatrix")) return false;
  JS::RootedObject object(cx);
  const double identity[6] = {1, 0, 0, 1, 0, 0};
  if (!NewSvgMatrixObject(s, identity, &object)) return false;
  args.rval().setObject(*object); return true;
}
bool CreateTransform(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp); State& s = Self(args);
  if (!s.svgPorts.elementCheck || !s.svgPorts.elementCheck(s.svgPorts.context, args.thisv(),
      "SVGSVGElement", "createSVGTransform")) return false;
  JS::RootedObject object(cx);
  const double identity[6] = {1, 0, 0, 1, 0, 0};
  if (!NewSvgTransformObject(s, 1, identity, 0, &object)) return false;
  args.rval().setObject(*object); return true;
}
}
bool Realm::InstallSvgDomTypesProjection(JS::HandleObject prototype) {
  State& s = *state_;
  JSAutoRealm realm(s.cx, s.kit.page);
  WebFace face(s.cx); face.prototype = prototype;
  return s.kit.Method(face, "SVGSVGElement", "createSVGNumber", CreateNumber, 0, 0) &&
      s.kit.Method(face, "SVGSVGElement", "createSVGAngle", CreateAngle, 0, 0) &&
      s.kit.Method(face, "SVGSVGElement", "createSVGMatrix", CreateMatrix, 0, 0) &&
      s.kit.Method(face, "SVGSVGElement", "createSVGTransform", CreateTransform, 0, 0);
}
}
