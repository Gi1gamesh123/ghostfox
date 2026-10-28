// The one TextDecoder class owns an opaque Firefox codec handle. Options and
// canonical label retain their existing slots; the codec owns all stream state.
#include "web_platform/state.h"
#include "web_platform/text_codec.h"
#include "web_platform/text_decoder_input.h"
#include "runtime/web_kit.h"
#include "jsapi.h"
#include "js/Conversions.h"
#include "js/ErrorReport.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"

namespace ruyijs::web_platform {
namespace {
void FinalizeDecoder(JS::GCContext*, JSObject* object) {
  const auto& slot = JS::GetReservedSlot(object, kDecoderHandleSlot);
  if (!slot.isUndefined()) CloseTextDecoder(slot.toPrivate());
}
const JSClassOps kDecoderOps = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, FinalizeDecoder};
}
const JSClass kTextDecoderClass = {
    "TextDecoder", JSCLASS_HAS_RESERVED_SLOTS(kDecoderSlotCount), &kDecoderOps};

bool Decode(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx), input(cx), options(cx);
  if (!WebKit::Check(cx, args.thisv(), &kTextDecoderClass, &self,
                     "TextDecoder member called on an incompatible receiver")) return false;
  if (!DecoderBuffer(cx, args.get(0), &input) || !DecoderOptions(cx, args.get(1), &options)) return false;
  bool stream = false;
  if (options) {
    JS::RootedValue value(cx);
    if (!JS_GetProperty(cx, options, "stream", &value)) return false;
    stream = JS::ToBoolean(value);
  }
  std::string bytes;
  if (!DecoderBytes(cx, input, &bytes)) return false;
  const bool fatal = JS::GetReservedSlot(self, kDecoderFatalSlot).toBoolean();
  const bool ignore_bom = JS::GetReservedSlot(self, kDecoderIgnoreBomSlot).toBoolean();
  void* decoder = JS::GetReservedSlot(self, kDecoderHandleSlot).toPrivate();
  std::u16string units;
  switch (DecodeText(decoder, bytes, stream, fatal, ignore_bom, &units)) {
    case DecodeStatus::OutOfMemory: JS_ReportOutOfMemory(cx); return false;
    case DecodeStatus::Limit:
      return WebKit::FailKind(cx, "TextDecoder output exceeds the bounded codec limit", JSEXN_RANGEERR);
    case DecodeStatus::Invalid:
      return WebKit::Fail(cx, "TextDecoder.decode hit invalid input in fatal mode");
    case DecodeStatus::Ok: break;
  }
  JS::RootedString text(cx, JS_NewUCStringCopyN(cx, units.data(), units.size()));
  if (!text) return false;
  args.rval().setString(text);
  return true;
}

bool TextDecoderGetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!WebKit::Check(cx, args.thisv(), &kTextDecoderClass, &self,
                     "TextDecoder member called on an incompatible receiver"))
    return false;
  switch (DecoderField(WebKit::Slot(args))) {
    case kDecoderEncodingField:
      args.rval().set(JS::GetReservedSlot(self, kDecoderEncodingSlot));
      return true;
    case kDecoderFatalField:
      args.rval().setBoolean(JS::GetReservedSlot(self, kDecoderFatalSlot).toBoolean());
      return true;
    case kDecoderIgnoreBomField:
      args.rval().setBoolean(
          JS::GetReservedSlot(self, kDecoderIgnoreBomSlot).toBoolean());
      return true;
  }
  return WebKit::Fail(cx, "Unknown TextDecoder member");
}
}  // namespace ruyijs::web_platform
