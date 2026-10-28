#include "web_geometry/svg_dom_types.h"
#include "js/Realm.h"
#include <string>

namespace ruyijs::web_geometry {
const JSClass kSvgPreserveAspectRatioClass = {"SVGPreserveAspectRatio", JSCLASS_HAS_RESERVED_SLOTS(2)};
namespace {
const char* kFields[] = {"align", "meetOrSlice"};
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const unsigned field = WebKit::Slot(args);
  JS::RootedObject object(cx);
  if (!SvgBrandReceiver(cx, args.thisv(), &kSvgPreserveAspectRatioClass,
                        (std::string("get ") + kFields[field]).c_str(), &object)) return false;
  args.rval().setInt32(JS::GetReservedSlot(object, field).toInt32()); return true;
}
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const unsigned field = WebKit::Slot(args);
  JS::RootedObject object(cx);
  if (!SvgBrandReceiver(cx, args.thisv(), &kSvgPreserveAspectRatioClass,
                        (std::string("set ") + kFields[field]).c_str(), &object)) return false;
  uint32_t value;
  if (!JS::ToUint32(cx, args.get(0), &value)) return false;
  JS::SetReservedSlot(object, field, JS::Int32Value(value & 0xffff));
  args.rval().setUndefined(); return true;
}
}
bool InstallSvgPreserveAspectRatio(State& s) {
  JS::RootedObject plain(s.cx, JS::GetRealmObjectPrototype(s.cx));
  if (!s.kit.Define("SVGPreserveAspectRatio", &kSvgPreserveAspectRatioClass, plain, WebKit::Reject,
                    &s.svgPreserveAspectRatio) ||
      !JS_DeleteProperty(s.cx, s.svgPreserveAspectRatio.prototype, "constructor")) return false;
  for (unsigned i = 0; i < 2; ++i)
    if (!s.kit.Accessor(s.svgPreserveAspectRatio, "SVGPreserveAspectRatio", kFields[i], Get, Set, i))
      return false;
  const char* aligns[] = {"UNKNOWN", "NONE", "XMINYMIN", "XMIDYMIN", "XMAXYMIN", "XMINYMID",
      "XMIDYMID", "XMAXYMID", "XMINYMAX", "XMIDYMAX", "XMAXYMAX"};
  const char* meets[] = {"UNKNOWN", "MEET", "SLICE"};
  const unsigned flags = JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT;
  for (unsigned i = 0; i < 11; ++i) {
    const auto name = std::string("SVG_PRESERVEASPECTRATIO_") + aligns[i];
    if (!JS_DefineProperty(s.cx, s.svgPreserveAspectRatio.prototype, name.c_str(), i, flags) ||
        !JS_DefineProperty(s.cx, s.svgPreserveAspectRatio.constructor, name.c_str(), i, flags))
      return false;
  }
  for (unsigned i = 0; i < 3; ++i) {
    const auto name = std::string("SVG_MEETORSLICE_") + meets[i];
    if (!JS_DefineProperty(s.cx, s.svgPreserveAspectRatio.prototype, name.c_str(), i, flags) ||
        !JS_DefineProperty(s.cx, s.svgPreserveAspectRatio.constructor, name.c_str(), i, flags))
      return false;
  }
  return JS_DefineProperty(s.cx, s.svgPreserveAspectRatio.prototype, "constructor",
                           s.svgPreserveAspectRatio.constructor, 0);
}
}
