// The original incremental codec feeds the original native Transform controller.
#include "web_platform/text_decoder_stream.h"
#include "web_platform/text_codec.h"
#include "web_platform/text_decoder_input.h"
#include "js/ErrorReport.h"
#include "js/String.h"

namespace ruyijs::web_platform {
namespace {
bool DecodeAndEnqueue(JSContext* cx,JS::HandleObject decoder,JS::HandleObject controller,
    JS::HandleValue chunk,bool flush) {
  std::string bytes;JS::RootedObject input(cx);
  if (!flush && (!DecoderBuffer(cx,chunk,&input,true) || !DecoderBytes(cx,input,&bytes))) return false;
  std::u16string units;
  const bool fatal=JS::GetReservedSlot(decoder,DSFatal).toBoolean(),ignore=JS::GetReservedSlot(decoder,DSIgnoreBom).toBoolean();
  const auto status=DecodeText(JS::GetReservedSlot(decoder,DSHandle).toPrivate(),bytes,!flush,fatal,ignore,&units);
  if (status==DecodeStatus::OutOfMemory) {JS_ReportOutOfMemory(cx);return false;}
  if (status==DecodeStatus::Limit) return WebKit::FailKind(cx,"TextDecoderStream output exceeds the bounded codec limit",JSEXN_RANGEERR);
  if (status==DecodeStatus::Invalid) return WebKit::Fail(cx,"Decoding failed.");
  if (units.empty()) return true;
  JS::RootedString text(cx,JS_NewUCStringCopyN(cx,units.data(),units.size()));
  if (!text) return false;
  JS::RootedValue value(cx,JS::StringValue(text));
  return resource_network::NativeTransformEnqueue(cx,controller,value);
}
bool Transform(JSContext* cx,JS::HandleObject decoder,JS::HandleObject controller,JS::HandleValue chunk) {
  return DecodeAndEnqueue(cx,decoder,controller,chunk,false);
}
bool Flush(JSContext* cx,JS::HandleObject decoder,JS::HandleObject controller,JS::HandleValue chunk) {
  return DecodeAndEnqueue(cx,decoder,controller,chunk,true);
}
}
const resource_network::NativeTransformSteps kDecoderStreamSteps={Transform,Flush};
}
