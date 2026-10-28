#include "web_gpu/value_objects.h"
#include "runtime/native_constructor_trace.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::web_gpu {
const JSClass kValidationErrorClass = {"GPUValidationError", JSCLASS_HAS_RESERVED_SLOTS(1)};
const JSClass kMemoryErrorClass = {"GPUOutOfMemoryError", JSCLASS_HAS_RESERVED_SLOTS(1)};
const JSClass kInternalErrorClass = {"GPUInternalError", JSCLASS_HAS_RESERVED_SLOTS(1)};
namespace {
WebFace& Face(State& state, int kind) {
  return kind == 1 ? state.memory_error : kind == 2 ? state.internal_error : state.validation_error;
}
bool DesiredPrototype(JSContext* cx, const JS::CallArgs& args, int kind,
                       JS::MutableHandleObject prototype) {
  JS::RootedObject target(cx, &args.newTarget().toObject());
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, target, "prototype", &value)) return false;
  if (value.isObject()) { prototype.set(&value.toObject()); return true; }
  JS::Realm* realm = JS::GetFunctionRealm(cx, target);
  if (!realm) return false;
  State* owner = StateForGlobal(JS::GetRealmGlobalOrNull(realm));
  if (!owner) return WebKit::Fail(cx, "WebGPU target realm is not installed");
  prototype.set(Face(*owner, kind).prototype);
  return JS_WrapObject(cx, prototype);
}
}
bool ConstructGpuError(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  if (!args.isConstructing()) return WebKit::Fail(cx, "WebGPU error constructor requires new");
  State& state = *static_cast<State*>(WebKit::Owner(args));
  const int kind = WebKit::Slot(args);
  JS::RootedObject prototype(cx);
  if (!DesiredPrototype(cx, args, kind, &prototype)) return false;
  if (!argc) return WebKit::Fail(cx, "WebGPU error constructor requires a message");
  JS::RootedString message(cx, JS::ToString(cx, args[0]));
  if (!message) return false;
  JS::RootedObject object(cx, state.kit.New(Face(state, kind).clasp, prototype));
  if (!object) return false;
  JS::SetReservedSlot(object, 0, JS::StringValue(message));
  args.rval().setObject(*object);
  return trace.Finish(true);
}
bool GpuErrorMessage(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject object(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  const JSClass* clasp = object ? JS::GetClass(object) : nullptr;
  if (clasp != &kValidationErrorClass && clasp != &kMemoryErrorClass && clasp != &kInternalErrorClass)
    return WebKit::Fail(cx, "GPUError message called on an incompatible receiver");
  args.rval().set(JS::GetReservedSlot(object, 0));
  return JS_WrapValue(cx, args.rval());
}
}
