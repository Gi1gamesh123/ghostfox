// The draw-path entry point: one native for every WebGL call that carries real
// shader, program, buffer, attribute, uniform, draw or readback state. The op
// code lives in the same native-function slot the state calls use, and the work
// itself is split between draw_program.cpp and draw_transfer.cpp.
#include "web_graphics/draw_calls.h"

#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "web_graphics/internal.h"
#include "web_graphics/public_calls.h"

namespace ruyijs::web_graphics {
namespace draw_call_detail {
bool TextOf(JSContext* cx, JS::HandleValue value, std::string* out) {
  JS::RootedString text(cx, JS::ToString(cx, value));
  if (!text) return false;
  JS::UniqueChars bytes(JS_EncodeStringToUTF8(cx, text));
  if (!bytes) return false;
  out->assign(bytes.get());
  return true;
}

JSObject* Owned(State& state, Context& context, JS::HandleValue value, int32_t kind) {
  if (!value.isObject()) return nullptr;
  JSObject* object = &value.toObject();
  if (JS::GetClass(object) != state.Face(kind).clasp) return nullptr;
  if (detail::ObjectContext(object) != &context) return nullptr;
  if (JS::GetReservedSlot(object, detail::kObjectDeletedSlot).toBoolean()) return nullptr;
  return object;
}

JSObject* StageOf(JSObject* program, int32_t stage) {
  JS::Value shader = JS::GetReservedSlot(
      program, stage == 0 ? detail::kObjectDataSlot : detail::kObjectExtraSlot);
  return shader.isObject() ? &shader.toObject() : nullptr;
}

bool ShaderSource(JSContext* cx, JSObject* shader, std::string* out) {
  JS::Value source = JS::GetReservedSlot(shader, detail::kObjectDataSlot);
  if (!source.isString()) return false;
  JS::RootedString text(cx, source.toString());
  JS::UniqueChars bytes(JS_EncodeStringToUTF8(cx, text));
  if (!bytes) return false;
  out->assign(bytes.get());
  return true;
}

void CanvasSize(JSContext* cx, Context& context, int32_t* width, int32_t* height) {
  *width = 300;
  *height = 150;
  if (!context.canvas) return;
  JS::RootedObject canvas(cx, context.canvas);
  JS::RootedValue property(cx);
  uint32_t value = 0;
  if (JS_GetProperty(cx, canvas, "width", &property) && JS::ToUint32(cx, property, &value))
    *width = int32_t(value);
  if (JS_GetProperty(cx, canvas, "height", &property) && JS::ToUint32(cx, property, &value))
    *height = int32_t(value);
}

bool StageDeclarations(JSContext* cx, JSObject* program, int32_t stage, const char* kind,
                       std::vector<GlslDeclaration>* out) {
  out->clear();
  JSObject* shader = StageOf(program, stage);
  if (!shader) return false;
  std::string source;
  if (!ShaderSource(cx, shader, &source)) return false;
  GlslStage parsed;
  if (!GlslParse(source, &parsed)) return false;
  *out = GlslDeclarations(parsed, kind);
  return true;
}
}  // namespace draw_call_detail

bool DrawCall(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *detail::Self(args);
  const int op = WebKit::Slot(args) & 0xffff;
  JS::RootedObject self(cx);
  if (!detail::ContextReceiver(cx, args, &self)) return false;
  Context& context = *detail::ContextOf(self);
  args.rval().setUndefined();
  switch (op) {
    case kBufferData: case kEnableVertexAttrib: case kDisableVertexAttrib:
    case kVertexAttribPointer: case kUniformFloat: case kDrawArrays: case kReadPixels:
      return TransferOperation(cx, args, state, context, op);
    default:
      return ProgramOperation(cx, args, state, context, op);
  }
}
}  // namespace ruyijs::web_graphics
