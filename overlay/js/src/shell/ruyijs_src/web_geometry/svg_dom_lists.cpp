#include "web_geometry/svg_dom_types.h"
#include "js/Realm.h"
#include "js/Symbol.h"
#include <string>

namespace ruyijs::web_geometry {
const JSClass kSvgLengthListClass = {"SVGLengthList"};
const JSClass kSvgNumberListClass = {"SVGNumberList"};
const JSClass kSvgStringListClass = {"SVGStringList"};
const JSClass kSvgTransformListClass = {"SVGTransformList"};
namespace {
struct Def { const JSClass* clasp; const char* name; WebFace* face; unsigned index; };
// Operation slot: interface index * 16 + op (0 clear .. 8 consolidate).
const char* kMethods[] = {"clear", "initialize", "getItem", "insertItemBefore", "replaceItem",
    "removeItem", "appendItem", "createSVGTransformFromMatrix", "consolidate"};
bool Exception(State& s, const char* type, const std::string& message) {
  return s.exception ? s.exception(s.exceptionContext, type, message.c_str())
                     : WebKit::Fail(s.cx, message.c_str());
}
bool Length(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const unsigned slot = WebKit::Slot(args), index = slot / 16, which = slot % 16;
  const char* name = which == 9 ? "get length" : "get numberOfItems";
  JS::RootedObject object(cx);
  const JSClass* clasps[] = {&kSvgLengthListClass, &kSvgNumberListClass, &kSvgStringListClass,
                             &kSvgTransformListClass};
  if (!SvgBrandReceiver(cx, args.thisv(), clasps[index], name, &object)) return false;
  args.rval().setNumber(0); return true;
}
bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp); State& s = Self(args);
  const unsigned slot = WebKit::Slot(args), index = slot / 16, op = slot % 16;
  const char* names[] = {"SVGLengthList", "SVGNumberList", "SVGStringList", "SVGTransformList"};
  const JSClass* clasps[] = {&kSvgLengthListClass, &kSvgNumberListClass, &kSvgStringListClass,
                             &kSvgTransformListClass};
  JS::RootedObject object(cx);
  if (!SvgBrandReceiver(cx, args.thisv(), clasps[index], kMethods[op], &object)) return false;
  const auto member = std::string(names[index]) + "." + kMethods[op];
  if (op == 2 || op == 5) return Exception(s, "IndexSizeError", member + ": Index out of range");
  if (op == 8) { args.rval().setNull(); return true; }
  if (op == 7) {
    double components[6] = {1, 0, 0, 1, 0, 0};
    if (args.get(0).isObject()) {
      JS::RootedObject init(cx, &args.get(0).toObject());
      static const char* fields[6] = {"a", "b", "c", "d", "e", "f"};
      for (unsigned i = 0; i < 6; ++i) {
        JS::RootedValue v(cx);
        if (!JS_GetProperty(cx, init, fields[i], &v)) return false;
        if (v.isUndefined()) continue;
        double number;
        if (!JS::ToNumber(cx, v, &number)) return false;
        components[i] = number;
      }
    }
    JS::RootedObject result(cx);
    if (!NewSvgTransformObject(s, 1, components, 0, &result)) return false;
    args.rval().setObject(*result); return true;
  }
  args.rval().setUndefined(); return true;
}
bool RestoreToStringTag(State& s, JSObject* proto, const char* name) {
  JS::RootedObject object(s.cx, proto);
  JS::RootedId tag(s.cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(s.cx, JS::SymbolCode::toStringTag)));
  JS::RootedString text(s.cx, JS_NewStringCopyZ(s.cx, name));
  JS::ObjectOpResult deleted;
  return text && JS_DeletePropertyById(s.cx, object, tag, deleted) &&
         JS_DefinePropertyById(s.cx, object, tag, text, JSPROP_READONLY);
}
bool DefineList(State& s, const Def& def, bool transformList) {
  JS::RootedObject plain(s.cx, JS::GetRealmObjectPrototype(s.cx));
  if (!s.kit.Define(def.name, def.clasp, plain, WebKit::Reject, def.face) ||
      !JS_DeleteProperty(s.cx, def.face->prototype, "constructor")) return false;
  const unsigned count = transformList ? 9 : 7;
  const unsigned lengths[] = {0, 1, 1, 2, 2, 1, 1, 0, 0};
  for (unsigned op = 0; op < count; ++op)
    if (!s.kit.Method(*def.face, def.name, kMethods[op], Call, lengths[op], def.index * 16 + op))
      return false;
  const unsigned index = def.index;
  if (!s.kit.Accessor(*def.face, def.name, "length", Length, nullptr, index * 16 + 9) ||
      !s.kit.Accessor(*def.face, def.name, "numberOfItems", Length, nullptr, index * 16 + 10))
    return false;
  JS::RootedObject array(s.cx, JS::GetRealmArrayPrototype(s.cx)); JS::RootedValue values(s.cx);
  JS::RootedId id(s.cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(s.cx, JS::SymbolCode::iterator)));
  return JS_DefineProperty(s.cx, def.face->prototype, "constructor", def.face->constructor, 0) &&
      array && JS_GetProperty(s.cx, array, "values", &values) &&
      JS_DefinePropertyById(s.cx, def.face->prototype, id, values, 0) &&
      RestoreToStringTag(s, def.face->prototype, def.name);
}
}
bool InstallSvgLists(State& s) {
  Def defs[] = {
    {&kSvgLengthListClass, "SVGLengthList", &s.svgLengthList, 0},
    {&kSvgNumberListClass, "SVGNumberList", &s.svgNumberList, 1},
    {&kSvgStringListClass, "SVGStringList", &s.svgStringList, 2},
    {&kSvgTransformListClass, "SVGTransformList", &s.svgTransformList, 3},
  };
  for (unsigned i = 0; i < 4; ++i)
    if (!DefineList(s, defs[i], i == 3)) return false;
  return true;
}
}
