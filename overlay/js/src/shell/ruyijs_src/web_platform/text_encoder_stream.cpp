// TextEncoderStream (docs/owners/WEB_PLATFORM.md, 2026-10-21): the Encoding
// standard's UTF-8 encode-and-enqueue steps over the original native Transform,
// which owns both streams. A high surrogate ending one chunk is held until the
// next chunk or flush, as in Firefox.
#include "web_platform/state.h"
#include "resource_network/native_transform.h"
#include "runtime/native_constructor_trace.h"
#include "jsapi.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/String.h"
#include "js/Conversions.h"
#include "jsfriendapi.h"
#include "js/StableStringChars.h"
#include "js/Symbol.h"
#include "js/Wrapper.h"
#include "js/experimental/TypedData.h"
#include <string>

namespace ruyijs::web_platform {
namespace {
enum Slot { ESPending, ESTransform, ESReadable, ESWritable, ESSlots };
const JSClass kTextEncoderStreamClass = {"TextEncoderStream", JSCLASS_HAS_RESERVED_SLOTS(ESSlots)};
void Append(std::string* out, uint32_t c) {
  if (c < 0x80) { out->push_back(char(c)); return; }
  if (c < 0x800) { out->push_back(char(0xC0 | (c >> 6))); }
  else if (c < 0x10000) { out->push_back(char(0xE0 | (c >> 12))); out->push_back(char(0x80 | ((c >> 6) & 63))); }
  else {
    out->push_back(char(0xF0 | (c >> 18)));
    out->push_back(char(0x80 | ((c >> 12) & 63)));
    out->push_back(char(0x80 | ((c >> 6) & 63)));
  }
  out->push_back(char(0x80 | (c & 63)));
}
bool Enqueue(JSContext* cx, JS::HandleObject controller, const std::string& bytes) {
  if (bytes.empty()) return true;
  JS::RootedObject array(cx, JS_NewUint8Array(cx, bytes.size()));
  if (!array) return false;
  {
    JS::AutoCheckCannotGC nogc;
    bool shared = false;
    uint8_t* data = JS_GetUint8ArrayData(array, &shared, nogc);
    std::copy(bytes.begin(), bytes.end(), data);
  }
  JS::RootedValue value(cx, JS::ObjectValue(*array));
  return resource_network::NativeTransformEnqueue(cx, controller, value);
}
bool Transform(JSContext* cx, JS::HandleObject self, JS::HandleObject controller, JS::HandleValue chunk) {
  JS::RootedString text(cx, JS::ToString(cx, chunk));  // DOMString conversion of the chunk
  if (!text) return false;
  JS::AutoStableStringChars chars(cx);
  if (!chars.initTwoByte(cx, text)) return false;
  const auto units = chars.twoByteRange();
  uint32_t pending = uint32_t(JS::GetReservedSlot(self, ESPending).toInt32());
  std::string out;
  for (size_t i = 0; i < units.length(); ++i) {
    const uint32_t unit = units[i];
    if (pending) {
      const uint32_t high = pending;
      pending = 0;
      if (unit >= 0xDC00 && unit <= 0xDFFF) { Append(&out, 0x10000 + ((high - 0xD800) << 10) + (unit - 0xDC00)); continue; }
      Append(&out, 0xFFFD);
    }
    if (unit >= 0xD800 && unit <= 0xDBFF) { pending = unit; continue; }
    Append(&out, unit >= 0xDC00 && unit <= 0xDFFF ? 0xFFFD : unit);
  }
  JS::SetReservedSlot(self, ESPending, JS::Int32Value(int32_t(pending)));
  return Enqueue(cx, controller, out);
}
bool Flush(JSContext* cx, JS::HandleObject self, JS::HandleObject controller, JS::HandleValue) {
  if (!JS::GetReservedSlot(self, ESPending).toInt32()) return true;
  JS::SetReservedSlot(self, ESPending, JS::Int32Value(0));
  return Enqueue(cx, controller, std::string("\xEF\xBF\xBD"));
}
const resource_network::NativeTransformSteps kSteps = {Transform, Flush};
bool Construct(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  if (!args.isConstructing()) return WebKit::Fail(cx, "TextEncoderStream constructor: 'new' is required");
  State& state = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject prototype(cx), factory(cx), object(cx), transform(cx), readable(cx), writable(cx);
  JS::RootedObject fallback(cx, state.textEncoderStream.prototype);
  if (!state.kit.ConstructProto(args, fallback, &prototype) ||
      !resource_network::NativeTransformFactory(cx, state.kit.page, &factory)) return false;
  object = JS_NewObjectWithGivenProto(cx, &kTextEncoderStreamClass, prototype);
  if (!object) return false;
  JS::SetReservedSlot(object, ESPending, JS::Int32Value(0));
  if (!resource_network::NewNativeTransform(cx, factory, object, kSteps, &transform, &readable, &writable)) return false;
  JS::SetReservedSlot(object, ESTransform, JS::ObjectValue(*transform));
  JS::SetReservedSlot(object, ESReadable, JS::ObjectValue(*readable));
  JS::SetReservedSlot(object, ESWritable, JS::ObjectValue(*writable));
  args.rval().setObject(*object);
  return trace.Finish(true);
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int slot = WebKit::Slot(args);
  JS::RootedObject object(cx, args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (!object || JS::GetClass(object) != &kTextEncoderStreamClass) {
    const char* name = slot == ESReadable ? "readable" : slot == ESWritable ? "writable" : "encoding";
    return WebKit::Fail(cx, (std::string("'get ") + name +
                             "' called on an object that does not implement interface TextEncoderStream.").c_str());
  }
  if (slot == ESSlots) {
    JSString* utf8 = JS_NewStringCopyZ(cx, "utf-8");
    if (!utf8) return false;
    args.rval().setString(utf8);
    return true;
  }
  args.rval().set(JS::GetReservedSlot(object, uint32_t(slot)));
  return JS_WrapValue(cx, args.rval());
}
}  // namespace

bool InstallTextEncoderStream(State& state) {
  JSContext* cx = state.cx;
  WebKit& kit = state.kit;
  WebFace& face = state.textEncoderStream;
  JS::RootedObject parent(cx, JS::GetRealmObjectPrototype(cx));
  if (!parent || !kit.Define("TextEncoderStream", &kTextEncoderStreamClass, parent, Construct, &face)) return false;
  // GenericTransformStream's readable/writable, then TextEncoderCommon's encoding, then constructor/tag.
  JS::RootedId tag(cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx, JS::SymbolCode::toStringTag)));
  if (!JS_DeleteProperty(cx, face.prototype, "constructor") || !JS_DeletePropertyById(cx, face.prototype, tag) ||
      !kit.Accessor(face, "TextEncoderStream", "readable", Get, nullptr, ESReadable) ||
      !kit.Accessor(face, "TextEncoderStream", "writable", Get, nullptr, ESWritable) ||
      !kit.Accessor(face, "TextEncoderStream", "encoding", Get, nullptr, ESSlots)) return false;
  JS::RootedString name(cx, JS_NewStringCopyZ(cx, "TextEncoderStream"));
  return name && JS_DefineProperty(cx, face.prototype, "constructor", face.constructor, 0) &&
         JS_DefinePropertyById(cx, face.prototype, tag, name, JSPROP_READONLY);
}
}  // namespace ruyijs::web_platform
