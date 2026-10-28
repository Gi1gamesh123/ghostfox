#include "web_crypto/subtle.h"
#include "runtime/page_helpers.h"
#include "runtime/native_constructor_trace.h"
#include "jsfriendapi.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Symbol.h"

namespace ruyijs::web_crypto {
const JSClass kCryptoClassDefinition = {"Crypto", JSCLASS_HAS_RESERVED_SLOTS(CryptoSlotCount)};
namespace {
bool Constructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  return CryptoTypeError(cx, "Illegal constructor.");
}
JSObject* Function(JSContext* cx, JSNative native, const char* name, unsigned length,
                   RandomOperation op, JS::HandleObject exception, unsigned flags = 0) {
  JS::RootedFunction fn(cx, js::NewFunctionWithReserved(cx, native, length, flags, name));
  if (!fn) return nullptr;
  JS::RootedObject object(cx, JS_GetFunctionObject(fn));
  // Resolve SpiderMonkey's lazy function keys before adding WebIDL properties.
  JS::RootedValue ignored(cx);
  if (!JS_GetProperty(cx, object, "length", &ignored) ||
      !JS_GetProperty(cx, object, "name", &ignored)) return nullptr;
  js::SetFunctionNativeReserved(object, 0, JS::Int32Value(int32_t(op)));
  js::SetFunctionNativeReserved(object, 1, JS::ObjectValue(*exception));
  return object;
}
bool Tag(JSContext* cx, JS::HandleObject object) {
  JS::RootedId id(cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx, JS::SymbolCode::toStringTag)));
  JS::RootedString value(cx, JS_NewStringCopyZ(cx, "Crypto"));
  return value && JS_DefinePropertyById(cx, object, id, value, JSPROP_READONLY);
}
}
bool Install(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
             JS::HandleObject globalPrototype) {
  JSAutoRealm realm(cx, page);
  JS::RootedValue original(cx), prototypeValue(cx);
  if (!JS_GetProperty(cx, page, "DOMException", &original) || !original.isObject()) return false;
  JS::RootedObject exceptionConstructor(cx, &original.toObject());
  if (!JS_GetProperty(cx, exceptionConstructor, "prototype", &prototypeValue) || !prototypeValue.isObject()) return false;
  JS::RootedObject exception(cx, &prototypeValue.toObject());
  JS::RootedObject prototype(cx, JS_NewPlainObject(cx));
  JS::RootedObject constructor(cx, Function(cx, Constructor, "Crypto", 0,
      RandomOperation::Uuid, exception, JSFUN_CONSTRUCTOR));
  JS::RootedObject values(cx, Function(cx, RandomMember, "getRandomValues", 1, RandomOperation::Values, exception));
  JS::RootedObject uuid(cx, Function(cx, RandomMember, "randomUUID", 0, RandomOperation::Uuid, exception));
  if (!prototype || !constructor || !values || !uuid ||
      !JS_DefineProperty(cx, constructor, "prototype", prototype, JSPROP_READONLY | JSPROP_PERMANENT) ||
      !Tag(cx, prototype) ||
      !JS_DefineProperty(cx, prototype, "getRandomValues", values, JSPROP_ENUMERATE) ||
      !helpers.Member(values, "Crypto", "getRandomValues", "call") ||
      !helpers.Member(uuid, "Crypto", "randomUUID", "call") ||
      !helpers.Constructor("Crypto", constructor) ||
      !JS_DefineProperty(cx, page, "Crypto", constructor, 0)) return false;
  JS::RootedObject instance(cx, JS_NewObjectWithGivenProto(cx, &kCryptoClassDefinition, prototype));
  if (!instance) return false;
  JS::SetReservedSlot(instance, RandomUuidMember, JS::ObjectValue(*uuid));
  JS::SetReservedSlot(instance, CryptoConstructor, JS::ObjectValue(*constructor));
  JS::RootedObject target(cx, globalPrototype ? globalPrototype.get() : page.get());
  return InstallSubtle(cx, page, helpers, instance) &&
         InstallGlobal(cx, page, target, instance, helpers) &&
         helpers.Object("Crypto", instance) && ConfigureExposure(cx, page);
}
}
