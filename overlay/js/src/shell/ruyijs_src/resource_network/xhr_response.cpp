#include "resource_network/xhr.h"
#include "js/ArrayBuffer.h"
#include "js/Exception.h"
#include "js/JSON.h"
#include "js/Object.h"
#include "js/Wrapper.h"
#include <cstring>
namespace ruyijs::resource_network {
bool XhrResponse(JSContext* cx, JS::HandleObject self, XhrMember member,
                  JS::MutableHandleValue result) {
  auto& r = XhrData(self);
  bool textType = r.responseType.empty() || r.responseType == "text";
  if ((member == XText && !textType) ||
      (member == XXml && !r.responseType.empty() && r.responseType != "document"))
    return XhrException(*r.owner, "InvalidStateError");
  if (member == XXml) { result.setNull(); return true; }
  if (member == XText || textType) {
    // Firefox exposes both text getters while LOADING; binary/JSON responses
    // below still wait for DONE. Read the original HTTP body in either case.
    bool available = r.state.ready >= 3;
    JS::RootedString text(cx, DecodeBodyText(cx, available ? r.response.body : ""));
    if (!text) return false;
    result.setString(text); return true;
  }
  result.setNull();
  if (r.state.ready != 4) return true;
  JS::RootedValue cached(cx, JS::GetReservedSlot(self, XResponseCache));
  if (!cached.isUndefined()) {
    if (!JS_WrapValue(cx, &cached)) return false;
    result.set(cached); return true;
  }
  {
    if (r.responseType == "arraybuffer") {
      JS::RootedObject buffer(cx, JS::NewArrayBuffer(cx, r.response.body.size()));
      if (!buffer) return false;
      size_t length = 0; bool shared = false; uint8_t* bytes = nullptr;
      JS::GetArrayBufferLengthAndData(buffer, &length, &shared, &bytes);
      if (length) std::memcpy(bytes, r.response.body.data(), length);
      cached.setObject(*buffer);
    } else if (r.responseType == "json") {
      JS::RootedString text(cx, DecodeBodyText(cx, r.response.body));
      if (!text) return false;
      if (!JS_ParseJSON(cx, text, &cached)) {
        if (!JS_IsExceptionPending(cx) || JS_IsThrowingOutOfMemory(cx)) return false;
        JS_ClearPendingException(cx); cached.setNull();
      }
    } else cached.setNull();
    JS::RootedValue stored(cx, cached);
    JSAutoRealm realm(cx, self);
    if (!JS_WrapValue(cx, &stored)) return false;
    JS::SetReservedSlot(self, XResponseCache, stored);
  }
  // Public blob/document response values are null placeholders.
  if (!JS_WrapValue(cx, &cached)) return false;
  result.set(cached);
  return true;
}
}
