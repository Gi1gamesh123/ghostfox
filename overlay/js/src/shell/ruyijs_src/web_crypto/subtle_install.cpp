#include "web_crypto/subtle.h"
#include "runtime/page_helpers.h"
#include "runtime/native_constructor_trace.h"
#include "jsfriendapi.h"
#include "js/Object.h"
#include "js/Symbol.h"
#include <iterator>
#include <string>
namespace ruyijs::web_crypto {
const JSClass kSubtleClass = {"SubtleCrypto", JSCLASS_HAS_RESERVED_SLOTS(SubtleSlotCount)};
const JSClass kCryptoKeyClass = {"CryptoKey", JSCLASS_HAS_RESERVED_SLOTS(CryptoKeySlotCount)};
namespace {
bool Illegal(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  return CryptoTypeError(cx, "Illegal constructor");
}
JSObject* Function(JSContext* cx, JSNative native, const char* name, unsigned length,
                     int32_t slot, unsigned flags = 0) {
  JS::RootedFunction function(cx, js::NewFunctionWithReserved(cx, native, length, flags, name));
  if (!function) return nullptr;
  JS::RootedObject result(cx, JS_GetFunctionObject(function));
  js::SetFunctionNativeReserved(result, 0, JS::Int32Value(slot));
  return result;
}
bool Accessor(JSContext* cx, PageHelpers& helpers, JS::HandleObject prototype,
                const char* iface, const char* name, JSNative getter, int32_t slot) {
  std::string getName = std::string("get ") + name;
  JS::RootedObject get(cx, Function(cx, getter, getName.c_str(), 0, slot));
  JS::RootedObject set(cx);
  return get && JS_DefineProperty(cx, prototype, name, get, set, JSPROP_ENUMERATE) &&
         helpers.Accessor(iface, prototype, name);
}
bool Finish(JSContext* cx, PageHelpers& helpers, const char* name,
              JS::HandleObject prototype, JS::HandleObject crypto, unsigned slot) {
  JS::RootedObject constructor(cx, Function(cx, Illegal, name, 0, 0, JSFUN_CONSTRUCTOR));
  JS::RootedString tag(cx, JS_NewStringCopyZ(cx, name));
  JS::RootedId key(cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx, JS::SymbolCode::toStringTag)));
  if (!constructor || !tag) return false;
  JS::SetReservedSlot(crypto, slot, JS::ObjectValue(*constructor));
  return
      JS_DefineProperty(cx, constructor, "prototype", prototype, JSPROP_READONLY | JSPROP_PERMANENT) &&
      JS_DefineProperty(cx, prototype, "constructor", constructor, 0) &&
      JS_DefinePropertyById(cx, prototype, key, tag, JSPROP_READONLY) &&
      helpers.Interface(name, prototype) && helpers.Object(name, prototype) &&
      helpers.Constructor(name, constructor);
}
}
bool InstallSubtle(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
                   JS::HandleObject crypto) {
  JS::RootedObject subtlePrototype(cx, JS_NewPlainObject(cx));
  JS::RootedObject keyPrototype(cx, JS_NewPlainObject(cx));
  if (!subtlePrototype || !keyPrototype) return false;
  for (unsigned i = 0; i < std::size(kSubtleOperations); ++i) {
    const auto& op = kSubtleOperations[i];
    JS::RootedObject method(cx, Function(cx, SubtleMethod, op.name, op.length, int32_t(i)));
    if (!method || !JS_DefineProperty(cx, subtlePrototype, op.name, method, JSPROP_ENUMERATE) ||
        !helpers.Member(method, "SubtleCrypto", op.name, "call")) return false;
  }
  const char* const members[] = {"type", "extractable", "algorithm", "usages"};
  for (unsigned i = 0; i < std::size(members); ++i)
    if (!Accessor(cx, helpers, keyPrototype, "CryptoKey", members[i], KeyGet, int32_t(i))) return false;
  if (!Finish(cx, helpers, "SubtleCrypto", subtlePrototype, crypto, SubtleConstructor) ||
      !Finish(cx, helpers, "CryptoKey", keyPrototype, crypto, KeyConstructor)) return false;
  JS::RootedObject subtle(cx, JS_NewObjectWithGivenProto(cx, &kSubtleClass, subtlePrototype));
  JS::RootedValue exception(cx);
  if (!subtle || !JS_GetProperty(cx, page, "DOMException", &exception)) return false;
  JS::SetReservedSlot(subtle, SubtleException, exception);
  JS::SetReservedSlot(subtle, SubtleKeyConstructor,
                      JS::GetReservedSlot(crypto, KeyConstructor));
  JS::SetReservedSlot(crypto, SubtleInstance, JS::ObjectValue(*subtle));
  JS::RootedObject getter(cx, Function(cx, SubtleGet, "get subtle", 0, 0));
  if (!getter || !helpers.Member(getter, "Crypto", "subtle", "get")) return false;
  JS::SetReservedSlot(crypto, SubtleGetter, JS::ObjectValue(*getter));
  return true;
}
}
