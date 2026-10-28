// The Window.external interface. [LegacyNoInterfaceObject] means the prototype
// exists without an interface object, and both operations are empty in the
// donor, so every call returns undefined and no other state is kept.
#include "browser_environment/external.h"
#include "browser_environment/schema.h"
#include "browser_environment/state.h"
#include "runtime/page_helpers.h"
#include "runtime/runtime.h"
#include "jsfriendapi.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include <iterator>

namespace ruyijs::browser_environment {
namespace {
const JSClass kExternalClass = {"External", JSCLASS_HAS_RESERVED_SLOTS(1)};
const char* const kMethods[] = {"AddSearchProvider", "IsSearchProviderInstalled"};
State* Instance(JSContext* cx, const JS::CallArgs& args, const char* name) {
  auto* state = static_cast<State*>(
      js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  State* receiver = state ? ReceiverState(args, &kExternalClass) : nullptr;
  if (!receiver) {
    Error(cx, "External member called on incompatible receiver");
    return nullptr;
  }
  state = receiver;
  state->Observe("External", name, int(ApiAccess::Call));
  return state;
}
bool Method(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  const int32_t slot = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  if (slot < 0 || slot >= int32_t(std::size(kMethods)))
    return Error(cx, "External member is not installed");
  if (!Instance(cx, args, kMethods[slot])) return false;
  args.rval().setUndefined();
  return true;
}
}  // namespace

bool InstallExternalSurface(State& state) {
  JSContext* cx = state.cx;
  JS::RootedObject prototype(cx, JS_NewPlainObject(cx));
  if (!prototype) return false;
  JS::RootedString tag(cx, JS_NewStringCopyZ(cx, kExternalClass.name));
  JS::RootedId tagId(cx, JS::PropertyKey::Symbol(
      JS::GetWellKnownSymbol(cx, JS::SymbolCode::toStringTag)));
  if (!tag || !JS_DefinePropertyById(cx, prototype, tagId, tag, JSPROP_READONLY))
    return false;
  for (size_t index = 0; index < std::size(kMethods); ++index) {
    JS::RootedObject method(cx,
        state.Function(Method, kMethods[index], 0, int32_t(index)));
    if (!method || !JS_DefineProperty(cx, prototype, kMethods[index], method,
                                      JSPROP_ENUMERATE) ||
        !state.helpers->Member(method, "External", kMethods[index], "call"))
      return false;
  }
  JS::RootedObject instance(cx, JS_NewObjectWithGivenProto(cx, &kExternalClass,
                                                           prototype));
  if (!instance) return false;
  JS::SetReservedSlot(instance, 0, JS::PrivateValue(&state));
  if (!state.helpers->Interface("External", prototype) ||
      !state.helpers->Object("External", prototype)) return false;
  state.external = instance;
  return true;
}
}  // namespace ruyijs::browser_environment
