#ifndef RUYIJS_WEB_GPU_INTERNAL_H
#define RUYIJS_WEB_GPU_INTERNAL_H
#include <cmath>
#include <cstdint>
#include <string>
#include "js/CallArgs.h"
#include "js/CharacterEncoding.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "runtime/runtime.h"
#include "web_gpu/state.h"

namespace ruyijs::web_gpu {

// Reads a DOMString member; a missing or null member leaves the output alone.
inline bool ReadString(JSContext* cx, JS::HandleObject source, const char* name,
                       std::string* out) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, source, name, &value)) return false;
  if (value.isUndefined() || value.isNull()) return true;
  JS::RootedString text(cx, JS::ToString(cx, value));
  return text && ToUTF8(cx, text, out);
}

// Reads a member that the specification declares as a string; a present member
// must convert, so an object with a throwing toString still fails the call.
inline bool ReadLabel(JSContext* cx, JS::HandleObject source, JS::MutableHandleString out) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, source, "label", &value)) return false;
  if (value.isUndefined() || value.isNull()) {
    JS::RootedString empty(cx, JS_NewStringCopyZ(cx, ""));
    out.set(empty.get());
    return true;
  }
  JS::RootedString text(cx, JS::ToString(cx, value));
  out.set(text.get());
  return !!text;
}

// WebIDL unsigned long conversion for usage flags and counts.
inline bool ReadUint(JSContext* cx, JS::HandleValue value, uint32_t* out) {
  return JS::ToUint32(cx, value, out);
}

// WebIDL double conversion for sizes and offsets.
inline bool ReadDouble(JSContext* cx, JS::HandleValue value, double* out) {
  return JS::ToNumber(cx, value, out) && !std::isnan(*out);
}

// Settles a resolved promise over a raw value; WebKit::Resolve takes a rooted
// handle, and this domain's promise-returning operations pass literals.
inline bool Resolve(State& state, const JS::Value& value, JS::MutableHandleValue result) {
  JS::RootedValue rooted(state.cx, value);
  return state.kit.Resolve(rooted, result);
}

// Converts a domain string into a JS string value; the label accessors share it.
inline bool StringValue(JSContext* cx, const std::string& text,
                        JS::MutableHandleValue out) {
  JS::RootedString value(
      cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(text.data(), text.size())));
  if (!value) return false;
  out.setString(value);
  return true;
}

// Stores an ASCII string in a reserved slot and reads it back.
bool SetString(JSContext* cx, JS::HandleObject target, int32_t slot, const char* ascii);
bool SlotString(JSContext* cx, JS::HandleObject target, int32_t slot, std::string* out);

// The brand checks shared by every member of one interface.
bool CheckGpu(JSContext* cx, JS::HandleValue self, JS::MutableHandleObject out);
bool CheckAdapter(JSContext* cx, JS::HandleValue self, JS::MutableHandleObject out);
bool CheckDevice(JSContext* cx, JS::HandleValue self, JS::MutableHandleObject out);
bool CheckBuffer(JSContext* cx, JS::HandleValue self, JS::MutableHandleObject out);
bool CheckTexture(JSContext* cx, JS::HandleValue self, JS::MutableHandleObject out);
bool CheckTextureView(JSContext* cx, JS::HandleValue self, JS::MutableHandleObject out);
bool CheckFeatures(JSContext* cx, JS::HandleValue self, JS::MutableHandleObject out);
bool CheckQueue(JSContext* cx, JS::HandleValue self, JS::MutableHandleObject out);
bool CheckEncoder(JSContext* cx, JS::HandleValue self, JS::MutableHandleObject out);
bool CheckPass(JSContext* cx, JS::HandleValue self, JS::MutableHandleObject out);
bool CheckCommandBuffer(JSContext* cx, JS::HandleValue self, JS::MutableHandleObject out);


}  // namespace ruyijs::web_gpu
#endif
