#include "web_crypto/subtle.h"
#include "web_crypto/rsa.h"
#include "web_crypto/ec.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"
#include "js/Exception.h"
#include "js/Object.h"
#include "js/Promise.h"
#include "js/Wrapper.h"
namespace ruyijs::web_crypto {
namespace {
const JSErrorFormatString* ErrorFormat(void*, unsigned) {
  static const JSErrorFormatString format{"RUYIJS_CRYPTO", "{0}", 1, JSEXN_TYPEERR};
  return &format;
}
bool Brand(JSContext* cx, JS::HandleValue value, const JSClass* shape, JS::MutableHandleObject object) {
  object.set(value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  return (object && JS::GetClass(object) == shape) ||
         CryptoTypeError(cx, "WebCrypto member called on an incompatible receiver");
}
bool RejectPending(JSContext* cx, JS::HandleObject promise) {
  if (JS_IsThrowingOutOfMemory(cx) || !JS_IsExceptionPending(cx)) return false;
  JS::RootedValue error(cx);
  if (!JS_GetPendingException(cx, &error)) return false;
  JS_ClearPendingException(cx);
  return JS::RejectPromise(cx, promise, error);
}
}
bool CryptoTypeError(JSContext* cx, const char* message) {
  JS_ReportErrorNumberASCII(cx, ErrorFormat, nullptr, 0, message);
  return false;
}
bool SubtleError(JSContext* cx, JS::HandleObject self, const char* name) {
  JS::RootedValue constructor(cx, JS::GetReservedSlot(self, 0));
  JS::RootedString text(cx, JS_NewStringCopyZ(cx, name));
  JS::RootedValueArray<2> args(cx);
  JS::RootedObject error(cx);
  if (!text || !JS_WrapValue(cx, &constructor)) return false;
  args[0].setString(text); args[1].setString(text);
  if (!JS::Construct(cx, constructor, args, &error)) return false;
  JS::RootedValue result(cx, JS::ObjectValue(*error));
  JS_SetPendingException(cx, result);
  return false;
}
bool SubtleGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Brand(cx, args.thisv(), &kCryptoClassDefinition, &self)) return false;
  args.rval().set(JS::GetReservedSlot(self, 0));
  return JS_WrapValue(cx, args.rval());
}
bool KeyGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Brand(cx, args.thisv(), &kCryptoKeyClass, &self)) return false;
  unsigned slot = unsigned(js::GetFunctionNativeReserved(&args.callee(), 0).toInt32());
  args.rval().set(JS::GetReservedSlot(self, slot));
  return JS_WrapValue(cx, args.rval());
}
namespace {
// Key algorithms are delegated to their owner; every name this realm does not
// implement keeps the previous NotSupportedError rejection.
bool Routed(JSContext* cx, JS::HandleObject self, unsigned op, const JS::CallArgs& args,
            JS::MutableHandleValue value) {
  // EC and AES first: RSA's spki export would claim any key of that format.
  for (auto owner : {&EcOperation, &AesOperation, &HmacOperation, &RsaOperation}) {
    switch (owner(cx, self, op, args, value)) {
      case SubtleStatus::kHandled: return true;
      case SubtleStatus::kFailed: return false;
      default: break;
    }
  }
  return SubtleError(cx, self, "NotSupportedError");
}
}
bool SubtleMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Brand(cx, args.thisv(), &kSubtleClass, &self)) {
    JS::RootedValue error(cx);
    if (!JS_GetPendingException(cx, &error)) return false;
    JS_ClearPendingException(cx);
    JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
    if (!promise || !JS::RejectPromise(cx, promise, error)) return false;
    args.rval().setObject(*promise); return true;
  }
  unsigned op = unsigned(js::GetFunctionNativeReserved(&args.callee(), 0).toInt32());
  JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx, self);
    JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
    JS::RootedValue algorithm(cx, args.get(0)), data(cx, args.get(1)), value(cx);
    if (!promise || !JS_WrapValue(cx, &algorithm) || !JS_WrapValue(cx, &data)) return false;
    bool ok = argc < kSubtleOperations[op].length ?
        CryptoTypeError(cx, "Insufficient arguments for WebCrypto operation") :
        op == 4 ? Digest(cx, self, algorithm, data, &value) : Routed(cx, self, op, args, &value);
    if (!(ok ? JS::ResolvePromise(cx, promise, value) : RejectPending(cx, promise))) return false;
    result.setObject(*promise);
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}
}
