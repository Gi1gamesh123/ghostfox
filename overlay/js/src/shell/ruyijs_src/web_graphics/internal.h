#ifndef RUYIJS_WEB_GRAPHICS_INTERNAL_H
#define RUYIJS_WEB_GRAPHICS_INTERNAL_H
#include <cstdint>
#include <string>
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Value.h"
#include "jsapi.h"
#include "web_graphics/constants.h"
#include "web_graphics/state.h"

namespace ruyijs::web_graphics::detail {
// Reads the install-time state from the native function's reserved slot. Never
// call this after writing args.rval(): Owner() reads the callee.
inline State* Self(const JS::CallArgs& args) {
  return static_cast<State*>(WebKit::Owner(args));
}

// The receiver check shared by every WebGLRenderingContext member.
inline bool ContextReceiver(JSContext* cx, const JS::CallArgs& args, JS::MutableHandleObject out) {
  bool two = (WebKit::Slot(args) >> 16) == 2;
  return WebKit::CheckBrand(cx, args, two ? &kContext2Class : &kContextClass, out,
                            two ? "WebGL2RenderingContext" : "WebGLRenderingContext");
}

// The Context* stored on a context wrapper's reserved slot 0.
inline Context* ContextOf(JS::HandleObject wrapper) {
  // Every object of kContextClass is created by the canvas context port, which
  // writes this slot before the wrapper is ever observable, so the private
  // value is always present here.
  return static_cast<Context*>(JS::GetReservedSlot(wrapper, 0).toPrivate());
}

// Records the first WebGL error; the specification keeps only the first until
// getError() clears it, and never throws for it.
inline void SetError(Context& context, int32_t error) {
  if (context.error == kNoError) context.error = error;
}

// WebIDL unsigned long conversion for the GLenum arguments.
inline bool ToEnum(JSContext* cx, JS::HandleValue value, int32_t* out) {
  uint32_t number = 0;
  if (!JS::ToUint32(cx, value, &number)) return false;
  *out = int32_t(number);
  return true;
}

// The reserved slots of one created WebGL object: its owner context, its kind
// and whether delete* has invalidated it. Slots 3..5 hold the per-kind payload
// (a shader's source/type/compiled flag, a program's two attached shaders and
// its linked flag, a buffer's bytes, a uniform location's name).
inline constexpr int32_t kObjectContextSlot = 0;
inline constexpr int32_t kObjectKindSlot = 1;
inline constexpr int32_t kObjectDeletedSlot = 2;
inline constexpr int32_t kObjectDataSlot = 3;
inline constexpr int32_t kObjectExtraSlot = 4;
inline constexpr int32_t kObjectFlagSlot = 5;
inline constexpr int32_t kObjectSlotCount = 6;

inline Context* ObjectContext(JSObject* object) {
  return static_cast<Context*>(JS::GetReservedSlot(object, kObjectContextSlot).toPrivate());
}
inline int32_t ObjectKindOf(JSObject* object) {
  return JS::GetReservedSlot(object, kObjectKindSlot).toInt32();
}
inline bool ObjectDeleted(JSObject* object) {
  return JS::GetReservedSlot(object, kObjectDeletedSlot).toBoolean();
}

// ASCII comparison with a literal, used by the canvas context port and the
// extension name lookup.
bool AsciiEquals(JSContext* cx, JS::HandleString text, const char* ascii, bool* match);
}  // namespace ruyijs::web_graphics::detail
#endif
