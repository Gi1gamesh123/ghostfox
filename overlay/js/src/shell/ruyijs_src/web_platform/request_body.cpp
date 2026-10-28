#include "web_platform/request_body.h"
#include "web_platform/state.h"
#include "runtime/runtime.h"
#include "jsapi.h"
#include "js/ArrayBuffer.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/Wrapper.h"
#include "js/experimental/TypedData.h"

namespace ruyijs::web_platform {
bool ReadRequestText(JSContext* cx, JS::HandleValue value, std::string* out) {
  JS::RootedString text(cx, JS::ToString(cx, value));
  return text && ToUTF8(cx, text, out);
}
namespace {
bool Copy(JSContext* cx, JS::HandleObject object, RequestBody* out) {
  const bool view = JS_IsArrayBufferViewObject(object);
  JS::RootedObject buffer(cx, object);
  bool shared = false;
  if (view) buffer = JS_GetArrayBufferViewBuffer(cx, object, &shared);
  if (!buffer) return false;
  if (shared) return WebKit::Fail(cx, "sendBeacon body requires a non-shared buffer");
  // Firefox admits detached BufferSource values as an empty body.
  if (JS::IsDetachedArrayBufferObject(buffer)) return true;
  size_t length = view ? JS_GetArrayBufferViewByteLength(object)
                       : JS::GetArrayBufferByteLength(buffer);
  if (length > kRequestBodyLimit) { out->tooLarge = true; return true; }
  JS::AutoCheckCannotGC nogc;
  const auto* data = static_cast<const char*>(view
      ? JS_GetArrayBufferViewData(object, &shared, nogc)
      : JS::GetArrayBufferData(buffer, &shared, nogc));
  if (length) out->bytes.assign(data, length);
  return true;
}
}
bool ReadRequestBody(JSContext* cx, JS::HandleValue value, RequestBody* out) {
  *out = {};
  if (value.isNullOrUndefined()) return true;
  if (value.isObject()) {
    JS::RootedObject self(cx, js::CheckedUnwrapStatic(&value.toObject()));
    if (self) {
      JSAutoRealm realm(cx, self);
      const JSClass* shape = JS::GetClass(self);
      if (shape == &kBlobClass || shape == &kFileClass) {
        JS::RootedValue type(cx, JS::GetReservedSlot(self, kBlobTypeSlot));
        if (!ReadRequestText(cx, type, &out->contentType)) return false;
        const uint8_t* data = nullptr;
        size_t length = 0;
        if (!BlobBytes(cx, self, &data, &length)) return false;
        if (length > kRequestBodyLimit) { out->tooLarge = true; return true; }
        if (length) out->bytes.assign(reinterpret_cast<const char*>(data), length);
        return true;
      }
      if (shape == &kSearchParamsClass) {
        std::vector<std::pair<std::string, std::string>> entries;
        if (!ParamsEntries(cx, self, &entries)) return false;
        out->bytes = SerializeFormQuery(entries);
        out->tooLarge = out->bytes.size() > kRequestBodyLimit;
        out->contentType = "application/x-www-form-urlencoded;charset=UTF-8";
        return true;
      }
      if (shape == &kFormDataClass) return ReadFormRequestBody(cx, self, out);
      if (JS::IsArrayBufferObject(self) || JS_IsArrayBufferViewObject(self))
        return Copy(cx, self, out);
    }
  }
  if (!ReadRequestText(cx, value, &out->bytes)) return false;
  out->tooLarge = out->bytes.size() > kRequestBodyLimit;
  out->contentType = "text/plain;charset=UTF-8";
  return true;
}
}
