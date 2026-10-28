// TextEncoder. The interface is stateless: a brand class with the constant
// "utf-8" label, one encoder for a whole string and one that fills a caller
// Uint8Array. Both funnel into the domain codec, so the byte limit and the
// unpaired-surrogate rule have exactly one implementation.
#include "web_platform/state.h"
#include "web_platform/text_codec.h"

#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "runtime/web_kit.h"
#include "jsapi.h"
#include "js/Conversions.h"
#include "js/ErrorReport.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/String.h"
#include "js/experimental/TypedData.h"
#include <cstring>

namespace ruyijs::web_platform {
const JSClass kTextEncoderClass = {"TextEncoder", 0};

namespace {
// Copies the UTF-16 code units of a JS value so the codec sees one flat string.
bool Units(JSContext* cx, JS::HandleValue value, std::u16string* units) {
  JS::RootedString text(cx, JS::ToString(cx, value));
  if (!text) return false;
  const size_t length = JS_GetStringLength(text);
  units->assign(length, u'\0');
  return !length || JS::CopyStringChars(cx, units->data(), text, length);
}
}  // namespace

bool TextEncoderConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  NativeConstructorTrace trace(cx, args);
  JS::RootedObject prototype(cx);
  if (!state.kit.ConstructProto(args, state.textEncoder.prototype, &prototype))
    return false;
  JS::RootedObject object(cx, state.kit.New(&kTextEncoderClass, prototype));
  if (!object) return false;
  args.rval().setObject(*object);
  return trace.Finish(true);
}

bool Encode(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!WebKit::Check(cx, args.thisv(), &kTextEncoderClass, &self,
                     "TextEncoder member called on an incompatible receiver"))
    return false;
  std::u16string units;
  if (!args.get(0).isUndefined() && !Units(cx, args.get(0), &units)) return false;
  std::string bytes;
  if (!AppendUtf8(units, &bytes))
    return WebKit::FailKind(cx, "TextEncoder.encode exceeds the 8 MiB text limit",
                            JSEXN_RANGEERR);
  JS::RootedObject array(cx, JS_NewUint8Array(cx, bytes.size()));
  if (!array) return false;
  if (!bytes.empty()) {
    uint8_t* data = nullptr;
    {
      JS::AutoCheckCannotGC nogc;
      bool shared = false;
      data = static_cast<uint8_t*>(JS_GetArrayBufferViewData(array, &shared, nogc));
      if (data) memcpy(data, bytes.data(), bytes.size());
    }
    if (!data) return WebKit::Fail(cx, "TextEncoder.encode produced a detached buffer");
  }
  args.rval().setObject(*array);
  return true;
}

bool EncodeInto(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!WebKit::Check(cx, args.thisv(), &kTextEncoderClass, &self,
                     "TextEncoder member called on an incompatible receiver"))
    return false;
  if (args.length() < 2)
    return WebKit::Fail(cx, "TextEncoder.encodeInto requires 2 arguments");
  std::u16string units;
  if (!Units(cx, args.get(0), &units)) return false;
  if (!args.get(1).isObject() || !JS_IsUint8Array(&args.get(1).toObject()))
    return WebKit::Fail(cx, "TextEncoder.encodeInto requires a Uint8Array destination");
  JS::RootedObject view(cx, &args.get(1).toObject());
  size_t read = 0, written = 0;
  {
    JS::AutoCheckCannotGC nogc;
    bool shared = false;
    uint8_t* data = static_cast<uint8_t*>(JS_GetArrayBufferViewData(view, &shared, nogc));
    const size_t capacity = JS_GetTypedArrayByteLength(view);
    if (data || !capacity) EncodeUtf8Into(units, data, capacity, &read, &written);
  }
  JS::RootedObject result(cx, JS_NewPlainObject(cx));
  JS::RootedValue read_value(cx, JS::NumberValue(double(read)));
  JS::RootedValue written_value(cx, JS::NumberValue(double(written)));
  if (!result ||
      !JS_DefineProperty(cx, result, "read", read_value, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, result, "written", written_value, JSPROP_ENUMERATE))
    return false;
  args.rval().setObject(*result);
  return true;
}

bool TextEncoderGetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!WebKit::Check(cx, args.thisv(), &kTextEncoderClass, &self,
                     "TextEncoder member called on an incompatible receiver"))
    return false;
  JS::RootedString value(cx, JS_NewStringCopyZ(cx, "utf-8"));
  if (!value) return false;
  args.rval().setString(value);
  return true;
}

bool InstallTextEncoder(State& state, PageHelpers&) {
  JSContext* cx = state.cx;
  WebKit& kit = state.kit;
  JS::RootedObject objectProto(cx, JS::GetRealmObjectPrototype(cx));
  return objectProto &&
         kit.Define("TextEncoder", &kTextEncoderClass, objectProto,
                    TextEncoderConstructor, &state.textEncoder) &&
         kit.Method(state.textEncoder, "TextEncoder", "encode", Encode, 0, 0) &&
         kit.Method(state.textEncoder, "TextEncoder", "encodeInto", EncodeInto, 2, 0) &&
         kit.Accessor(state.textEncoder, "TextEncoder", "encoding", TextEncoderGetter,
                      nullptr, 0);
}
}  // namespace ruyijs::web_platform
