// TextDecoder construction and installation. The constructor resolves the label,
// reads the fatal/ignoreBOM options and seeds the slots the behaviour file reads;
// the installer defines the one realm-local constructor, prototype and accessors.
#include "web_platform/state.h"
#include "web_platform/text_codec.h"
#include "web_platform/text_decoder_input.h"

#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/ErrorReport.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"

namespace ruyijs::web_platform {
bool TextDecoderConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *static_cast<State*>(WebKit::Owner(args));
  NativeConstructorTrace trace(cx, args);
  std::string label = "utf-8";
  if (argc >= 1 && !args.get(0).isUndefined() && !Convert(cx, args.get(0), &label))
    return false;
  bool fatal = false, ignore_bom = false;
  JS::RootedObject options(cx);
  if (!DecoderOptions(cx, args.get(1), &options)) return false;
  if (options) {
    JS::RootedValue value(cx);
    if (!JS_GetProperty(cx, options, "fatal", &value)) return false;
    fatal = JS::ToBoolean(value);
    if (!JS_GetProperty(cx, options, "ignoreBOM", &value)) return false;
    ignore_bom = JS::ToBoolean(value);
  }
  std::string encoding;
  if (!EncodingFromLabel(label, &encoding))
    return WebKit::FailKind(cx, "TextDecoder does not support this encoding label",
                            JSEXN_RANGEERR);
  JS::RootedObject prototype(cx);
  if (!state.kit.ConstructProto(args, state.textDecoder.prototype, &prototype))
    return false;
  JS::RootedObject object(cx, state.kit.New(&kTextDecoderClass, prototype));
  JS::RootedValue encoding_value(cx);
  if (!object || !Value(cx, encoding, &encoding_value)) return false;
  JS::SetReservedSlot(object, kDecoderEncodingSlot, encoding_value);
  JS::SetReservedSlot(object, kDecoderFatalSlot, JS::BooleanValue(fatal));
  JS::SetReservedSlot(object, kDecoderIgnoreBomSlot, JS::BooleanValue(ignore_bom));
  void* decoder = OpenTextDecoder(encoding, ignore_bom);
  if (!decoder) { JS_ReportOutOfMemory(cx); return false; }
  JS::SetReservedSlot(object, kDecoderHandleSlot, JS::PrivateValue(decoder));
  args.rval().setObject(*object);
  return trace.Finish(true);
}

bool InstallTextDecoder(State& state, PageHelpers&) {
  JSContext* cx = state.cx;
  WebKit& kit = state.kit;
  JS::RootedObject objectProto(cx, JS::GetRealmObjectPrototype(cx));
  return objectProto &&
         kit.Define("TextDecoder", &kTextDecoderClass, objectProto,
                    TextDecoderConstructor, &state.textDecoder) &&
         kit.Method(state.textDecoder, "TextDecoder", "decode", Decode, 0, 0) &&
         kit.Accessor(state.textDecoder, "TextDecoder", "encoding", TextDecoderGetter,
                      nullptr, kDecoderEncodingField) &&
         kit.Accessor(state.textDecoder, "TextDecoder", "fatal", TextDecoderGetter,
                      nullptr, kDecoderFatalField) &&
         kit.Accessor(state.textDecoder, "TextDecoder", "ignoreBOM", TextDecoderGetter,
                      nullptr, kDecoderIgnoreBomField);
}
}  // namespace ruyijs::web_platform
