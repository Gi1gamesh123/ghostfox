#include "web_crypto/subtle.h"
#include "js/Object.h"
namespace ruyijs::web_crypto {
bool ConfigureExposure(JSContext* cx, JS::HandleObject page) {
  JSAutoRealm realm(cx, page);
  JS::RootedValue secure(cx);
  JS::RootedObject crypto(cx), prototype(cx);
  if (!OriginalCrypto(cx, page, &crypto) || !JS_GetProperty(cx, page, "isSecureContext", &secure) ||
      !JS_GetPrototype(cx, crypto, &prototype)) return false;
  JS::RootedValue constructor(cx, JS::GetReservedSlot(crypto, CryptoConstructor));
  if (!JS_DeleteProperty(cx, prototype, "constructor")) return false;
  if (!secure.isTrue())
    return JS_DeleteProperty(cx, page, "SubtleCrypto") && JS_DeleteProperty(cx, page, "CryptoKey") &&
           JS_DeleteProperty(cx, prototype, "randomUUID") && JS_DeleteProperty(cx, prototype, "subtle") &&
           JS_DefineProperty(cx, prototype, "constructor", constructor, 0);
  JS::RootedValue subtle(cx, JS::GetReservedSlot(crypto, SubtleConstructor));
  JS::RootedValue key(cx, JS::GetReservedSlot(crypto, KeyConstructor));
  JS::RootedObject getter(cx, &JS::GetReservedSlot(crypto, SubtleGetter).toObject());
  JS::RootedObject setter(cx);
  JS::RootedValue uuid(cx, JS::GetReservedSlot(crypto, RandomUuidMember));
  return JS_DefineProperty(cx, page, "SubtleCrypto", subtle, 0) &&
         JS_DefineProperty(cx, page, "CryptoKey", key, 0) &&
         JS_DefineProperty(cx, prototype, "randomUUID", uuid, JSPROP_ENUMERATE) &&
         JS_DefineProperty(cx, prototype, "subtle", getter, setter, JSPROP_ENUMERATE) &&
         JS_DefineProperty(cx, prototype, "constructor", constructor, 0);
}
}
