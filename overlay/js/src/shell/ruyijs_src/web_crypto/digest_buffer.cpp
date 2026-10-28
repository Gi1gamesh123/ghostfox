#include "web_crypto/subtle.h"
#include "js/ArrayBuffer.h"
#include "js/Wrapper.h"
#include "js/experimental/TypedData.h"
#include <cstring>
namespace ruyijs::web_crypto {
bool CryptoBuffer(JSContext* cx, JS::HandleValue input, JS::MutableHandleObject object) {
  object.set(input.isObject() ? js::CheckedUnwrapStatic(&input.toObject()) : nullptr);
  return (object && (JS::IsArrayBufferObject(object) || JS_IsArrayBufferViewObject(object))) ||
         CryptoTypeError(cx, "WebCrypto data must be an ArrayBuffer or an ArrayBuffer view");
}
bool CryptoSnapshot(JSContext* cx, JS::HandleObject input, CryptoBytes* output) {
  bool view = JS_IsArrayBufferViewObject(input), shared = false;
  JS::RootedObject buffer(cx, view ? JS_GetArrayBufferViewBuffer(cx, input, &shared) : input.get());
  if (!buffer) return false;
  if (shared) return CryptoTypeError(cx, "WebCrypto data buffer is shared");
  // Firefox's digest byte snapshot observes a detached BufferSource as empty.
  if (JS::IsDetachedArrayBufferObject(buffer)) return true;
  output->size = view ? JS_GetArrayBufferViewByteLength(input) : JS::GetArrayBufferByteLength(buffer);
  if (!output->size) return true;
  output->data.reset(static_cast<char*>(JS_malloc(cx, output->size)));
  if (!output->data) return false;
  JS::AutoCheckCannotGC nogc;
  const void* bytes = view ? JS_GetArrayBufferViewData(input, &shared, nogc) :
                            JS::GetArrayBufferData(buffer, &shared, nogc);
  std::memcpy(output->data.get(), bytes, output->size);
  return true;
}
}
