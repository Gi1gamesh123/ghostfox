// WebIDL conversion for the existing RequestInput boundary. No persistent state.
#include "resource_network/request.h"
#include "resource_network/state.h"
#include "resource_network/abort.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "js/StableStringChars.h"
#include <initializer_list>

namespace ruyijs::resource_network {
bool RequestIntegrity(JSContext* cx, JS::HandleValue input, std::u16string* out) {
  JS::RootedString value(cx, JS::ToString(cx, input));
  JS::AutoStableStringChars chars(cx);
  if (!value || !chars.initTwoByte(cx, value)) return false;
  out->assign(chars.twoByteChars(), chars.length());
  return true;
}
namespace {
bool Enum(JSContext* cx, JS::HandleValue value, std::string* out,
          std::initializer_list<const char*> allowed) {
  if (!EncodeBodyText(cx, value, out)) return false;
  for (const auto* name : allowed) if (*out == name) return true;
  return Error(cx, "Invalid RequestInit enum value");
}
}
bool RequestInitInput(State& state, JS::HandleValue input, RequestInitFields* init,
                      JS::MutableHandleValue signal) {
  signal.setNull();
  if (input.isNullOrUndefined()) return true;
  JSContext* cx = state.cx;
  if (!input.isObject()) return Error(cx, "Request init must be an object");
  JS::RootedObject object(cx, &input.toObject());
  JS::RootedValue value(cx);
  auto get = [&](const char* name, uint32_t bit) {
    if (!JS_GetProperty(cx, object, name, &value)) return false;
    if (!value.isUndefined()) init->present |= bit;
    return true;
  };
  auto& d = init->values;
  // The order is observable through dictionary getters (Firefox RequestInit).
  if (!get("body", InitBody)) return false;
  BodyRecord body;
  if (!BodyInput(state, value, &body)) return false;
  d.body=std::move(body.bytes);d.kind=body.kind;d.nullBody=body.nullBody;
  if (!get("cache", InitCache) || (!value.isUndefined() && !Enum(cx,value,&d.cache,
      {"default","no-store","reload","no-cache","force-cache","only-if-cached"}))) return false;
  if (!get("credentials", InitCredentials) || (!value.isUndefined() && !Enum(cx,value,&d.credentials,
      {"omit","same-origin","include"}))) return false;
  if (!get("headers", InitHeaders) || !HeaderPairsArgument(cx,value,&d.headers)) return false;
  if (!get("integrity", InitIntegrity) || (!value.isUndefined() && !RequestIntegrity(cx,value,&d.integrity))) return false;
  if (!get("keepalive", InitKeepalive)) return false;
  if (!value.isUndefined()) d.keepalive=JS::ToBoolean(value);
  if (!get("method", InitMethod) || (!value.isUndefined() && !Convert(cx,value,&d.method))) return false;
  if (!get("mode", InitMode) || (!value.isUndefined() && !Enum(cx,value,&d.mode,
      {"cors","no-cors","same-origin","navigate"}))) return false;
  std::string priority;
  if (!get("priority", InitPriority) || (!value.isUndefined() && !Enum(cx,value,&priority,{"auto","high","low"}))) return false;
  if (!get("redirect", InitRedirect) || (!value.isUndefined() && !Enum(cx,value,&d.redirect,
      {"follow","error","manual"}))) return false;
  if (!get("referrer", InitReferrer) || (!value.isUndefined() && !EncodeBodyText(cx,value,&d.referrer))) return false;
  if (!get("referrerPolicy", InitReferrerPolicy) || (!value.isUndefined() && !Enum(cx,value,&d.referrer_policy,
      {"","no-referrer","no-referrer-when-downgrade","same-origin","origin","strict-origin",
       "origin-when-cross-origin","strict-origin-when-cross-origin","unsafe-url"}))) return false;
  if (!get("signal", InitSignal)) return false;
  if (!value.isNullOrUndefined()) {
    if (!IsAbortSignal(value)) return Error(cx,"signal must be an AbortSignal");
    signal.set(value);
  }
  return true;
}
}
