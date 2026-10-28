#include "web_platform/text_decoder_input.h"
#include "web_platform/buffer_source.h"
#include "web_platform/text_codec.h"
#include "runtime/web_kit.h"
#include "jsapi.h"
#include "js/ArrayBuffer.h"
#include "js/SharedArrayBuffer.h"
#include "js/ErrorReport.h"
#include "js/experimental/TypedData.h"
#include "js/Wrapper.h"

namespace ruyijs::web_platform {
bool DecoderOptions(JSContext* cx, JS::HandleValue value, JS::MutableHandleObject result,bool streamCaller) {
  result.set(nullptr);
  if (value.isNullOrUndefined()) return true;
  if (!value.isObject()) return WebKit::Fail(cx,streamCaller?"TextDecoderStream constructor: Argument 2 can't be converted to a dictionary.":"TextDecoder options must be a dictionary");
  result.set(&value.toObject());
  return true;
}
bool DecoderBuffer(JSContext* cx,JS::HandleValue value,JS::MutableHandleObject result,bool streamCaller) {
  return PlatformBufferSource(cx,value,result,streamCaller);
}
bool DecoderBytes(JSContext* cx, JS::HandleObject object, std::string* bytes) {
  if (!object) return true;
  const bool buffer = JS::IsArrayBufferObject(object);
  bool shared = false;
  JS::RootedObject backing(cx, buffer ? object.get() : JS_GetArrayBufferViewBuffer(cx, object, &shared));
  if (!backing) return false;
  if (shared) return WebKit::Fail(cx, "TextDecoder BufferSource must not be shared");
  // Gecko treats a detached ArrayBuffer/view as an empty input.
  if (JS::IsDetachedArrayBufferObject(backing)) return true;
  const size_t length = buffer ? JS::GetArrayBufferByteLength(object) : JS_GetArrayBufferViewByteLength(object);
  if (length > kTextByteLimit)
    return WebKit::FailKind(cx, "TextDecoder input exceeds the 8 MiB text limit", JSEXN_RANGEERR);
  DecodeStatus status;
  {
    JS::AutoCheckCannotGC nogc;
    const auto* data = static_cast<const char*>(buffer ? JS::GetArrayBufferData(object, &shared, nogc)
        : JS_GetArrayBufferViewData(object, &shared, nogc));
    if (!data || !length) return true;
    status=CopyDecoderBytes(data,length,bytes);
  }
  if (status==DecodeStatus::OutOfMemory) {JS_ReportOutOfMemory(cx);return false;}
  if (status!=DecodeStatus::Ok) return WebKit::FailKind(cx,"TextDecoder input exceeds the 8 MiB text limit",JSEXN_RANGEERR);
  return true;
}
}
