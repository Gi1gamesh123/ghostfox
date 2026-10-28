#include "web_graphics/internal.h"
#include "web_graphics/public_calls.h"
namespace ruyijs::web_graphics {
namespace {
struct Constant { int version; const char* name; double value; };
const Constant kConstants[] = {
#include "web_graphics/public_constants_0.inc"
#include "web_graphics/public_constants_1.inc"
#include "web_graphics/public_constants_2.inc"
#include "web_graphics/public_constants_3.inc"
#include "web_graphics/public_constants_4.inc"
#include "web_graphics/public_constants_5.inc"
};
struct ConstructorConstant { int version; const char* name; double value; };
const ConstructorConstant kConstructorConstants[] = {
{2, "TIMEOUT_IGNORED", -1},
};
struct Method { int version; const char* name; JSNative native; unsigned length; int op; };
const Method kMethods[] = {
#include "web_graphics/public_methods_0.inc"
#include "web_graphics/public_methods_1.inc"
};
}
bool InstallPublicSurface(State& s) {
  for (const auto& c : kConstants) {
    WebFace& f = c.version == 1 ? s.context : s.context2;
    JS::RootedValue v(s.cx, JS::NumberValue(c.value));
    const unsigned flags = JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT;
    // The prototype copy is made permanent by the binding order pass, after it is placed last.
    if (!JS_DefineProperty(s.cx, f.prototype, c.name, v, flags & ~JSPROP_PERMANENT) ||
        !JS_DefineProperty(s.cx, f.constructor, c.name, v, flags)) return false;
  }
  for (const auto& c : kConstructorConstants) {
    WebFace& f = c.version == 1 ? s.context : s.context2;
    JS::RootedValue v(s.cx, JS::NumberValue(c.value));
    const unsigned flags = JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT;
    if (!JS_DefineProperty(s.cx, f.constructor, c.name, v, flags)) return false;
  }
  for (const auto& m : kMethods) {
    WebFace& f = m.version == 1 ? s.context : s.context2;
    if (!s.kit.Method(f, m.version == 1 ? "WebGLRenderingContext" : "WebGL2RenderingContext",
                      m.name, m.native, m.length, m.op | (m.version << 16))) return false;
  }
  const char* attrs[] = {"canvas","drawingBufferWidth","drawingBufferHeight","drawingBufferColorSpace","unpackColorSpace"};
  for (int version = 1; version <= 2; ++version) {
    WebFace& f = version == 1 ? s.context : s.context2;
    for (int i = 0; i < 5; ++i)
      if (!s.kit.Accessor(f, version == 1 ? "WebGLRenderingContext" : "WebGL2RenderingContext",
          attrs[i], ContextAttribute, i >= 3 ? SetContextColorSpace : nullptr, i | (version << 16))) return false;
  }
  return true;
}
}
