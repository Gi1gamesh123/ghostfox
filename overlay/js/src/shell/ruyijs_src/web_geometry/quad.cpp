#include "web_geometry/internal.h"
#include "domain/geometry/values.h"

namespace ruyijs::web_geometry {
namespace {
constexpr const char* kPoints[] = {"p1", "p2", "p3", "p4"};
bool PointSnapshot(State& s, JS::HandleObject object, unsigned index,
                    JS::MutableHandleValue result) {
  double point[] = {JS::GetReservedSlot(object, index * 2).toNumber(),
                    JS::GetReservedSlot(object, index * 2 + 1).toNumber(), 0, 1};
  return PlainValues(s, kPointFields, point, 4, result);
}
}
bool ConstructQuad(State& s, const JS::CallArgs& args) {
  double values[8] = {};
  for (unsigned i = 0; i < 4; ++i)
    if (!ReadDictionary(s, args.get(i), kPointFields, 2, values + i * 2, false)) return false;
  return NewValue(s, Quad, values, &args, args.rval());
}
bool QuadGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Self(args);
  unsigned index = WebKit::Slot(args);
  JS::RootedObject object(cx);
  return Receiver(cx, args.thisv(), Quad, &object) && PointSnapshot(s, object, index, args.rval());
}
bool QuadJSON(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Self(args);
  JS::RootedObject object(cx);
  if (!Receiver(cx, args.thisv(), Quad, &object)) return false;
  JS::RootedObject result(cx, JS_NewPlainObject(cx));
  if (!result) return false;
  for (unsigned i = 0; i < 4; ++i) {
    JS::RootedValue point(cx);
    if (!PointSnapshot(s, object, i, &point) ||
        !JS_DefineProperty(cx, result, kPoints[i], point, JSPROP_ENUMERATE)) return false;
  }
  args.rval().setObject(*result);
  return true;
}
bool QuadBounds(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Self(args);
  JS::RootedObject object(cx);
  if (!Receiver(cx, args.thisv(), Quad, &object)) return false;
  double values[8], rect[8];
  for (unsigned i = 0; i < 8; ++i) values[i] = JS::GetReservedSlot(object, i).toNumber();
  domain::geometry::QuadBounds(values, rect);
  return PlainValues(s, kRectFields, rect, 8, args.rval());
}
bool QuadFrom(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Self(args);
  bool fromRect = WebKit::Slot(args) != 0;
  double values[8] = {};
  if (fromRect) {
    double rect[4] = {};
    if (!ReadDictionary(s, args.get(0), kRectFields, 4, rect, false)) return false;
    double right = rect[0] + rect[2], bottom = rect[1] + rect[3];
    values[0] = values[6] = rect[0]; values[2] = values[4] = right;
    values[1] = values[3] = rect[1]; values[5] = values[7] = bottom;
  } else if (args.get(0).isObject()) {
    JS::RootedObject input(cx, &args[0].toObject());
    for (unsigned i = 0; i < 4; ++i) {
      JS::RootedValue point(cx);
      if (!JS_GetProperty(cx, input, kPoints[i], &point) ||
          !ReadDictionary(s, point, kPointFields, 2, values + i * 2, false)) return false;
    }
  }
  return NewValue(s, Quad, values, nullptr, args.rval());
}
}
