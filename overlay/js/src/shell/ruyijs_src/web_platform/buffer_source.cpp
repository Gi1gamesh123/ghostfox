// Shared original WebIDL BufferSource union; caller policy preserves TextDecoder.
#include "web_platform/buffer_source.h"
#include "runtime/web_kit.h"
#include "jsapi.h"
#include "js/ArrayBuffer.h"
#include "js/SharedArrayBuffer.h"
#include "js/experimental/TypedData.h"
#include "js/Wrapper.h"

namespace ruyijs::web_platform {
bool PlatformBufferSource(JSContext* cx, JS::HandleValue value, JS::MutableHandleObject result,bool streamCaller) {
  result.set(nullptr);
  if (value.isUndefined() && !streamCaller) return true;
  if (!value.isObject()) return WebKit::Fail(cx,streamCaller?"Value could not be converted to any of: ArrayBufferView, ArrayBuffer.":"TextDecoder.decode requires a BufferSource");
  result.set(streamCaller?js::CheckedUnwrapStatic(&value.toObject()):&value.toObject());
  if (!result) return WebKit::Fail(cx,"Value could not be converted to any of: ArrayBufferView, ArrayBuffer.");
  if (streamCaller && JS::IsSharedArrayBufferObject(result))
    return WebKit::Fail(cx,"ArrayBuffer branch of (ArrayBufferView or ArrayBuffer) can't be a SharedArrayBuffer or an ArrayBufferView backed by a SharedArrayBuffer");
  if (!JS::IsArrayBufferObject(result) && !JS_IsArrayBufferViewObject(result))
    return WebKit::Fail(cx,streamCaller?"Value could not be converted to any of: ArrayBufferView, ArrayBuffer.":"TextDecoder.decode requires an ArrayBuffer or view");
  if (streamCaller && JS_IsArrayBufferViewObject(result) && JS::IsArrayBufferViewShared(result))
    return WebKit::Fail(cx,"ArrayBufferView branch of (ArrayBufferView or ArrayBuffer) can't be a SharedArrayBuffer or an ArrayBufferView backed by a SharedArrayBuffer");
  const auto data = JS::ArrayBufferOrView::unwrap(result);
  if (streamCaller && data.isImmutable())
    return WebKit::Fail(cx,JS::IsArrayBufferObject(result)?
        "ArrayBuffer branch of (ArrayBufferView or ArrayBuffer) can't be an immutable ArrayBuffer or ArrayBufferView":
        "ArrayBufferView branch of (ArrayBufferView or ArrayBuffer) can't be an immutable ArrayBuffer or ArrayBufferView");
  if (data.isResizable() || data.isImmutable())
    return WebKit::Fail(cx,streamCaller?(JS::IsArrayBufferObject(result)?
        "ArrayBuffer branch of (ArrayBufferView or ArrayBuffer) can't be a resizable ArrayBuffer or ArrayBufferView":
        "ArrayBufferView branch of (ArrayBufferView or ArrayBuffer) can't be a resizable ArrayBuffer or ArrayBufferView"):
        "TextDecoder requires a fixed mutable BufferSource");
  if (JS_IsArrayBufferViewObject(result)) {
    bool shared = false;
    JS::RootedObject backing(cx, JS_GetArrayBufferViewBuffer(cx, result, &shared));
    if (!backing) return false;
    if (shared) return WebKit::Fail(cx,streamCaller?
        "ArrayBufferView branch of (ArrayBufferView or ArrayBuffer) can't be a SharedArrayBuffer or an ArrayBufferView backed by a SharedArrayBuffer":
        "TextDecoder BufferSource must not be shared");
  }
  return true;
}
}
