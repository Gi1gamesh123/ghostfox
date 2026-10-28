// AbortController: one AbortSignal in its reserved slot and the abort() entry
// that flips it. The signal object and its abort transition are owned by
// abort_signal.cpp; this file adds no second abort state.
#include "resource_network/state.h"
#include "resource_network/abort.h"
#include "runtime/page_helpers.h"
#include "runtime/native_constructor_trace.h"
#include "jsfriendapi.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"

namespace ruyijs::resource_network {
namespace {
bool SignalOf(State& state, const JS::CallArgs& args, JS::MutableHandleObject signal) {
  JS::RootedObject self(state.cx,args.thisv().isObject()?js::CheckedUnwrapStatic(&args.thisv().toObject()):nullptr);
  if (!self || JS::GetClass(self) != &kAbortControllerClass)
    return Error(state.cx, "AbortController member requires an AbortController");
  signal.set(&JS::GetReservedSlot(self, 0).toObject());
  return true;
}
bool SignalGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionOwner(args);
  JS::RootedObject signal(cx);
  if (!SignalOf(state, args, &signal)) return false;
  state.Observe("AbortController", "signal", int(ApiAccess::Read));
  args.rval().setObject(*signal);
  return JS_WrapValue(cx,args.rval());
}
bool AbortCall(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionOwner(args);
  JS::RootedObject signal(cx);
  if (!SignalOf(state, args, &signal)) return false;
  state.Observe("AbortController", "abort", int(ApiAccess::Call));
  JS::RootedValue reason(cx, args.get(0));
  if (reason.isUndefined() && !AbortDefaultReason(state, &reason)) return false;
  if (!AbortSignalAbort(state, signal, reason)) return false;
  args.rval().setUndefined();
  return true;
}
bool ControllerCreate(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  State& state = *FunctionOwner(args);
  if (!args.isConstructing()) return Error(cx, "AbortController requires new");
  state.Observe("AbortController", "constructor", int(ApiAccess::Construct));
  JS::RootedObject target(cx, &args.newTarget().toObject());
  JS::RootedObject prototype(cx);
  JS::RootedValue desired(cx);
  if (!JS_GetProperty(cx, target, "prototype", &desired)) return false;
  if (desired.isObject()) prototype = &desired.toObject();
  JS::RootedObject signal(cx);
  if (!NewAbortSignal(state, &signal)) return false;
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, &kAbortControllerClass, prototype));
  if (!object) return false;
  JS::SetReservedSlot(object, 0, JS::ObjectValue(*signal));
  args.rval().setObject(*object);
  return trace.Finish(true);
}
}  // namespace

const JSClass kAbortControllerClass = {"AbortController", JSCLASS_HAS_RESERVED_SLOTS(1), nullptr};

bool InstallAbort(State& state, PageHelpers& helpers, interaction::Realm& events) {
  JSContext* cx = state.cx;
  JS::RootedObject signal_constructor(cx);
  if (!InstallAbortSignal(state, helpers, events, &signal_constructor)) return false;
  JS::RootedObject prototype(cx, JS_NewPlainObject(cx));
  JS::RootedObject constructor(
      cx, NativeFunction(state, ControllerCreate, "AbortController", 0, JSFUN_CONSTRUCTOR));
  JS::RootedObject signal(cx, NativeFunction(state, SignalGet, "get signal", 0));
  JS::RootedObject abort(cx, NativeFunction(state, AbortCall, "abort", 0));
  JS::RootedObject setter(cx);
  if (!prototype || !constructor || !signal || !abort ||
      !JS_DefineProperty(cx, constructor, "prototype", prototype,
                         JSPROP_READONLY | JSPROP_PERMANENT) ||
      !JS_DefineProperty(cx, prototype, "abort", abort, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, prototype, "signal", signal, setter, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, prototype, "constructor", constructor, 0) ||
      !Tag(cx, prototype, "AbortController")) return false;
  return helpers.Member(signal, "AbortController", "signal", "get") &&
         helpers.Member(abort, "AbortController", "abort", "call") &&
         helpers.Constructor("AbortController", constructor) &&
         JS_DefineProperty(cx, state.page, "AbortSignal", signal_constructor, 0) &&
         JS_DefineProperty(cx, state.page, "AbortController", constructor, 0);
}
}  // namespace ruyijs::resource_network
