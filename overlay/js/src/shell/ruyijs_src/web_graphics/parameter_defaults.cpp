#include "web_graphics/internal.h"
#include "js/experimental/TypedData.h"
namespace ruyijs::web_graphics {
bool DefaultParameter(State& s, Context& context, int32_t name,
                      JS::MutableHandleValue out, bool* found) {
  *found = true;
  if (name == 0x8CA6 || (name == 0x8CAA && context.version == 2)) {
    out.setObjectOrNull(name == 0x8CA6 ? context.drawFramebuffer.get() : context.readFramebuffer.get());
    return true;
  }
  if (name == 0x8894 || name == 0x8895) {
    out.setObjectOrNull(name == 0x8894 ? context.arrayBinding.get() : context.elementBinding.get());
    return true;
  }
  if (name == 0x86A3) {  // COMPRESSED_TEXTURE_FORMATS: Firefox reports an empty list.
    JS::RootedObject empty(s.cx, JS_NewUint32Array(s.cx, 0));
    if (!empty) return false;
    out.setObject(*empty);
    return true;
  }
  const auto* def = PublicParameterDefault(name);
  if (!def && context.version == 2) def = Webgl2ParameterDefault(name);
  if (!def) { *found = false; return true; }
  if (def->extension && !(def->core2 && context.version == 2)) {
    bool enabled = false;
    if (!ExtensionEnabled(s, context, def->extension, &enabled)) return false;
    if (!enabled) { *found = false; return true; }
  }
  if (def->kind == ParameterKind::Null) { out.setNull(); return true; }
  if (def->kind == ParameterKind::Number) { out.setNumber(def->values[0]); return true; }
  if (def->kind == ParameterKind::Boolean) { out.setBoolean(def->values[0] != 0); return true; }
  JSContext* cx = s.cx;
  JS::RootedObject array(cx, def->kind == ParameterKind::FloatArray ? JS_NewFloat32Array(cx, def->count) :
    def->kind == ParameterKind::IntArray ? JS_NewInt32Array(cx, def->count) : JS::NewArrayObject(cx, def->count));
  if (!array) return false;
  for (int i = 0; i < def->count; ++i) {
    JS::RootedValue value(cx, JS::NumberValue(def->values[i]));
    if (def->kind == ParameterKind::BoolArray) value.setBoolean(def->values[i] != 0);
    if ((name == 0x0BA2 || name == 0x0C10) && i >= 2) {
      if (!JS_GetProperty(cx, context.canvas, i == 2 ? "width" : "height", &value)) return false;
    }
    if (!JS_SetElement(cx, array, i, value)) return false;
  }
  out.setObject(*array); return true;
}
}
