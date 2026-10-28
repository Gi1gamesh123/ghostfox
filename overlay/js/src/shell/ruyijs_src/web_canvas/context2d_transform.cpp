// The 2D transform members. These are the real matrix math the specification
// defines; there is no need for a rasterizer, so they are implemented in full
// and asserted strictly (A15.4).
#include "web_canvas/internal.h"

#include <cmath>

namespace ruyijs::web_canvas {
namespace {
enum TransformKind : uint8_t { kTranslate, kScale, kRotate, kTransform, kSetTransform,
                               kResetTransform, kGetTransform };

struct MethodSpec {
  const char* name;
  TransformKind kind;
  unsigned length;
};

const MethodSpec kMethods[] = {
    {"translate", kTranslate, 2},       {"scale", kScale, 2},
    {"rotate", kRotate, 1},             {"transform", kTransform, 6},
    {"setTransform", kSetTransform, 0}, {"resetTransform", kResetTransform, 0},
    {"getTransform", kGetTransform, 0},
};

// out = current * next, with each factor in the canvas [a b c d e f] order.
void Multiply(const double current[6], const double next[6], double out[6]) {
  out[0] = current[0] * next[0] + current[2] * next[1];
  out[1] = current[1] * next[0] + current[3] * next[1];
  out[2] = current[0] * next[2] + current[2] * next[3];
  out[3] = current[1] * next[2] + current[3] * next[3];
  out[4] = current[0] * next[4] + current[2] * next[5] + current[4];
  out[5] = current[1] * next[4] + current[3] * next[5] + current[5];
}
}  // namespace

bool Context2DTransformMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int32_t index = detail::ContextSlot(args);
  JS::RootedObject self(cx);
  if (!detail::ContextReceiver(cx, args, &self)) return false;
  const MethodSpec& spec = kMethods[index];
  State& state = *detail::Self(args);
  args.rval().setUndefined();
  if (spec.kind == kGetTransform) {
    double current[6];
    if (!detail::GetMatrix(cx, self, current)) return false;
    JS::RootedObject matrix(cx);
    if (!state.matrixFactory) return WebKit::Fail(cx, "Canvas matrix factory is not installed");
    if (!state.matrixFactory(state.matrixFactoryContext, current, &matrix)) return false;
    args.rval().setObject(*matrix);
    return true;
  }
  double current[6];
  if (!detail::GetMatrix(cx, self, current)) return false;
  if (spec.kind == kResetTransform) {
    static const double kIdentity[6] = {1.0, 0.0, 0.0, 1.0, 0.0, 0.0};
    return detail::SetMatrix(cx, self, kIdentity);
  }
  double next[6] = {1.0, 0.0, 0.0, 1.0, 0.0, 0.0};
  if (spec.kind == kSetTransform && argc <= 1) {
    if (!args.get(0).isNullOrUndefined()) {
      if (!args[0].isObject()) return WebKit::Fail(cx,"setTransform requires a dictionary or six numbers");
      JS::RootedObject init(cx,&args[0].toObject());
      const char* names[]={"a","b","c","d","e","f"};
      const char* aliases[]={"m11","m12","m21","m22","m41","m42"};
      for (int i=0;i<6;++i) {
        JS::RootedValue value(cx),alias(cx);
        if (!JS_GetProperty(cx,init,names[i],&value) || !JS_GetProperty(cx,init,aliases[i],&alias)) return false;
        if (!value.isUndefined() && !JS::ToNumber(cx,value,&next[i])) return false;
        if (!alias.isUndefined()) {
          double number;
          if (!JS::ToNumber(cx,alias,&number)) return false;
          if (!value.isUndefined() && next[i]!=number && !(std::isnan(next[i]) && std::isnan(number)))
            return WebKit::Fail(cx,"Conflicting DOMMatrix2DInit aliases");
          next[i]=number;
        }
      }
      for (double number:next) if (!std::isfinite(number)) return true;
    }
  } else if (spec.kind == kTranslate || spec.kind == kScale) {
    double pair[2];
    bool usable = false;
    if (!detail::ReadNumbers(cx, args, 0, 2, pair, &usable) || !usable) return !usable;
    if (spec.kind == kTranslate) {
      next[4] = pair[0];
      next[5] = pair[1];
    } else {
      next[0] = pair[0];
      next[3] = pair[1];
    }
  } else if (spec.kind == kRotate) {
    double angle = 0.0;
    bool usable = false;
    if (!detail::ReadNumbers(cx, args, 0, 1, &angle, &usable) || !usable) return !usable;
    next[0] = std::cos(angle);
    next[1] = std::sin(angle);
    next[2] = -std::sin(angle);
    next[3] = std::cos(angle);
  } else {
    bool usable = false;
    if (!detail::ReadNumbers(cx, args, 0, 6, next, &usable) || !usable) return !usable;
  }
  double composed[6];
  if (spec.kind == kSetTransform) {
    for (int i = 0; i < 6; ++i) composed[i] = next[i];
  } else {
    Multiply(current, next, composed);
  }
  return detail::SetMatrix(cx, self, composed);
}

bool InstallContext2DTransform(State& state, WebFace& face) {
  const int32_t count = int32_t(sizeof(kMethods) / sizeof(kMethods[0]));
  for (int32_t index = 0; index < count; ++index)
    if (!detail::Method2D(state, face, kMethods[index].name,
                          Context2DTransformMethod, kMethods[index].length, index))
      return false;
  return true;
}
}  // namespace ruyijs::web_canvas
