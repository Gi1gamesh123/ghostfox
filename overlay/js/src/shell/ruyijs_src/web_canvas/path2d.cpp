// Path2D shares the context's bounded command format and geometry projection.
#include <cmath>
#include "web_canvas/internal.h"
#include "js/Realm.h"
#include "runtime/native_constructor_trace.h"

namespace ruyijs::web_canvas {
const JSClass kPath2DClass = {"Path2D", JSCLASS_HAS_RESERVED_SLOTS(kContextSlotCount)};
namespace {
bool CopyPath(JSContext* cx, JS::HandleObject target, JS::HandleObject source,
              const double matrix[6]) {
  JS::RootedObject from(cx, &JS::GetReservedSlot(source, kContextPathSlot).toObject());
  JS::RootedObject to(cx, &JS::GetReservedSlot(target, kContextPathSlot).toObject());
  uint32_t count = 0, size = 0;
  if (!JS::GetArrayLength(cx, from, &count) || !JS::GetArrayLength(cx, to, &size)) return false;
  if (count + size > 8192) return true;
  // Snapshot first: addPath(self) must not chase newly appended entries.
  std::vector<double> commands;
  for (uint32_t i = 0; i < count; ++i) {
    JS::RootedValue value(cx);
    if (!JS_GetElement(cx, from, i, &value)) return false;
    commands.push_back(value.toNumber());
  }
  static const unsigned sizes[] = {2,2,0,4,6,8,4,6,6};
  for (size_t i = 0; i < commands.size();) {
    int op = int(commands[i++]);
    if (op < 0 || op > 8 || i + sizes[op] > commands.size()) return false;
    if (op == 8) {
      const double a=commands[i], b=commands[i+1], c=commands[i+2], d=commands[i+3],
                   e=commands[i+4], f=commands[i+5];
      commands[i]=matrix[0]*a+matrix[2]*b; commands[i+1]=matrix[1]*a+matrix[3]*b;
      commands[i+2]=matrix[0]*c+matrix[2]*d; commands[i+3]=matrix[1]*c+matrix[3]*d;
      commands[i+4]=matrix[0]*e+matrix[2]*f+matrix[4];
      commands[i+5]=matrix[1]*e+matrix[3]*f+matrix[5];
    }
    i += sizes[op];
  }
  for (double n : commands) {
    JS::RootedValue value(cx, JS::NumberValue(n));
    if (!JS_SetElement(cx, to, size++, value)) return false;
  }
  return true;
}
bool PathCtor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *detail::Self(args);
  NativeConstructorTrace trace(cx, args);
  JS::RootedObject proto(cx);
  if (!s.kit.ConstructProto(args, s.path2d.prototype, &proto)) return false;
  JS::RootedObject self(cx, s.kit.New(&kPath2DClass, proto));
  JS::RootedObject path(cx, JS::NewArrayObject(cx, 0));
  if (!self || !path) return false;
  JS::SetReservedSlot(self, kContextPathSlot, JS::ObjectValue(*path));
  if (args.get(0).isObject() && JS::GetClass(&args[0].toObject()) == &kPath2DClass) {
    JS::RootedObject source(cx, &args[0].toObject());
    const double identity[6] = {1,0,0,1,0,0};
    if (!CopyPath(cx, self, source, identity)) return false;
  } else if (!args.get(0).isUndefined() && !JS::ToString(cx, args[0])) return false;
  // SVG text is the public wheel's empty-path transition, not an SVG parser.
  args.rval().setObject(*self);
  return trace.Finish(true);
}
bool PathExtra(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *detail::Self(args);
  int op = WebKit::Slot(args);
  JS::RootedObject self(cx);
  if (!WebKit::CheckBrand(cx, args, &kPath2DClass, &self, "Path2D")) return false;
  args.rval().setUndefined();
  if (op == 1) {
    if (argc < 5) return WebKit::Fail(cx, "arcTo requires five arguments");
    double values[5]; bool usable;
    if (!detail::ReadNumbers(cx, args, 0, 5, values, &usable)) return false;
    return !usable || values[4] >= 0 || ThrowDOMException(s, "IndexSizeError", "Negative radius");
  }
  JS::RootedObject source(cx);
  if (!WebKit::Check(cx, args.get(0), &kPath2DClass, &source, "addPath requires a Path2D")) return false;
  double m[6] = {1,0,0,1,0,0};
  if (args.get(1).isObject()) {
    JS::RootedObject init(cx, &args[1].toObject());
    const char* names[] = {"a","b","c","d","e","f"};
    for (int i = 0; i < 6; ++i) {
      JS::RootedValue value(cx);
      if (!JS_GetProperty(cx, init, names[i], &value)) return false;
      if (!value.isUndefined() && !JS::ToNumber(cx, value, &m[i])) return false;
      if (!std::isfinite(m[i])) return true;
    }
  }
  return CopyPath(cx, self, source, m);
}
}
bool InstallPath2D(State& s) {
  JS::RootedObject parent(s.cx, JS::GetRealmObjectPrototype(s.cx));
  if (!s.kit.Define("Path2D", &kPath2DClass, parent, PathCtor, &s.path2d)) return false;
  const char* names[] = {"closePath","moveTo","lineTo","arc","ellipse","rect",
    "quadraticCurveTo","bezierCurveTo"};
  const unsigned lengths[] = {0,2,2,5,7,4,4,6};
  for (int i = 0; i < 8; ++i)
    if (!s.kit.Method(s.path2d, "Path2D", names[i], Context2DPathMethod, lengths[i], 101+i)) return false;
  return s.kit.Method(s.path2d, "Path2D", "addPath", PathExtra, 1, 0) &&
    s.kit.Method(s.path2d, "Path2D", "arcTo", PathExtra, 5, 1) &&
    s.kit.Method(s.path2d, "Path2D", "roundRect", RoundRect, 4, 100);
}
}
