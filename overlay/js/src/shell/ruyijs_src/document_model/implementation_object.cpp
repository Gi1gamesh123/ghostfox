// The original parsing service factory and the per-document implementation cache.
#include "document_model/parsing.h"
#include "document_model/state.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
const JSClass kServiceClass = {"DOM Platform Service", JSCLASS_HAS_RESERVED_SLOTS(3), nullptr};
namespace {
bool Build(State& state, int service, JS::HandleObject document, JS::MutableHandleValue result,
           const JS::CallArgs* args = nullptr) {
  const char* name = service == 0 ? "DOMParser"
                     : service == 1 ? "XMLSerializer" : "DOMImplementation";
  Interface* iface = state.Find(name);
  if (!iface) return TypeError(state.cx, "Parsing service interface is not installed");
  JS::RootedObject source(state.cx, document);
  if (source && !JS_WrapObject(state.cx, &source)) return false;
  JS::RootedObject prototype(state.cx, iface->prototype);
  if (args) {
    JS::RootedObject target(state.cx, &args->newTarget().toObject());
    JS::RootedValue value(state.cx);
    if (!JS_GetProperty(state.cx, target, "prototype", &value)) return false;
    if (value.isObject()) prototype = &value.toObject();
  }
  JS::RootedObject object(state.cx,
      JS_NewObjectWithGivenProto(state.cx, &kServiceClass, prototype));
  if (!object) return false;
  JS::SetReservedSlot(object, 0, JS::PrivateValue(&state));
  JS::SetReservedSlot(object, 1, JS::Int32Value(service));
  JS::SetReservedSlot(object, 2, JS::ObjectOrNullValue(source));
  result.setObject(*object);
  return true;
}
bool Implementation(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& factory = *FunctionState(args);
  JS::RootedObject source(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (!DocumentReceiver(cx, source)) return false;
  factory.Observe("Document", "implementation", int(ApiAccess::Read));
  JS::RootedValue result(cx, JS::GetReservedSlot(source, 6));
  if (result.isUndefined()) {
    if (!Build(factory, 2, source, &result)) return false;
    JSAutoRealm realm(cx, source);
    if (!JS_WrapValue(cx, &result)) return false;
    JS::SetReservedSlot(source, 6, result);
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result);
  return true;
}
}
bool NewService(State& state, int service, JS::MutableHandleValue result, const JS::CallArgs* args) {
  JS::RootedObject source(state.cx);
  if (service == 2) {
    JS::RootedValue document(state.cx);
    if (!state.Wrap(state.main, &document)) return false;
    if (document.isObject()) source = &document.toObject();
  }
  return Build(state, service, source, result, args);
}
bool InstallImplementationObject(State& state) {
  return state.Accessor("Document", "implementation", Implementation, 0);
}
}
