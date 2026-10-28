#include "web_crypto/subtle.h"
#include "runtime/page_reference.h"
#include "runtime/page_helpers.h"
#include "jsfriendapi.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"

namespace ruyijs::web_crypto {
bool OriginalCrypto(JSContext* cx, JS::HandleObject page, JS::MutableHandleObject result) {
  JS::RootedValue value(cx);
  if (!ReadPageReference(page, PageReference::Crypto, &value) || !value.isObject() ||
      JS::GetClass(&value.toObject()) != &kCryptoClassDefinition)
    return CryptoTypeError(cx, "Crypto global called on an incompatible receiver");
  result.set(&value.toObject());
  return true;
}
namespace {
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject page(cx, args.thisv().isNullOrUndefined() ?
      &js::GetFunctionNativeReserved(&args.callee(), 0).toObject() :
      args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  JS::RootedObject instance(cx);
  if (!OriginalCrypto(cx, page, &instance)) return false;
  args.rval().setObject(*instance);
  return JS_WrapValue(cx, args.rval());
}
}
bool InstallGlobal(JSContext* cx, JS::HandleObject page, JS::HandleObject target,
                   JS::HandleObject crypto, PageHelpers& helpers) {
  JS::RootedValue value(cx, JS::ObjectValue(*crypto));
  JS::RootedFunction function(cx, js::NewFunctionWithReserved(cx, Get, 0, 0, "get crypto"));
  if (!function || !WritePageReference(page, PageReference::Crypto, value)) return false;
  JS::RootedObject getter(cx, JS_GetFunctionObject(function)), setter(cx);
  js::SetFunctionNativeReserved(getter, 0, JS::ObjectValue(*page));
  return JS_DefineProperty(cx, target, "crypto", getter, setter, JSPROP_ENUMERATE) &&
         helpers.Accessor(target == page ? "Window" : "WorkerGlobalScope", target, "crypto");
}
}
