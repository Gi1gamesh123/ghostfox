// The deprecated Screen orientation members and the [SameObject] accessor that
// exposes the ScreenOrientation instance screen_orientation.cpp created. One
// installer owns all five members; Screen keeps inheriting EventTarget from the
// environment factory, and the onmozorientationchange handler lives in the
// Screen instance's second reserved slot (never dispatched in this runtime).
// donor dom/base/nsScreen.h:77-81: mozLockOrientation always returns false and
// mozUnlockOrientation is empty. See docs/DEVIATIONS.md D47.
#include "browser_environment/screen_orientation.h"
#include "browser_environment/schema.h"
#include "browser_environment/state.h"
#include "runtime/page_helpers.h"
#include "jsfriendapi.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"

namespace ruyijs::browser_environment {
namespace {
// Slot 1 of the Screen instance holds Screen.onmozorientationchange.
constexpr size_t kScreenHandlerSlot = 1;
enum Member : int32_t {
  kMozOrientation = 520, kMozOrientationChange, kOrientation,
  kMozLock = 530, kMozUnlock
};
State* Owner(JSContext* cx, const JS::CallArgs& args) {
  auto* state = static_cast<State*>(
      js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  State* receiver = state ? ReceiverState(args, ClassFor(Kind::Screen)) : nullptr;
  if (!receiver) {
    Error(cx, "Screen member called on incompatible receiver");
    return nullptr;
  }
  state = receiver;
  return state;
}
bool Getter(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  const int32_t slot = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  State* state = Owner(cx, args);
  if (!state) return false;
  if (slot == kMozOrientation) {
    state->Observe("Screen", "mozOrientation", int(ApiAccess::Read));
    return OrientationTypeText(*state, args.rval());
  }
  if (slot == kMozOrientationChange) {
    state->Observe("Screen", "onmozorientationchange", int(ApiAccess::Read));
    args.rval().set(JS::GetReservedSlot(&args.thisv().toObject(), kScreenHandlerSlot));
    return true;
  }
  state->Observe("Screen", "orientation", int(ApiAccess::Read));
  args.rval().setObject(*state->screenOrientation);
  return JS_WrapValue(cx, args.rval());
}
bool Setter(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  State* state = Owner(cx, args);
  if (!state) return false;
  state->Observe("Screen", "onmozorientationchange", int(ApiAccess::Write));
  // WebIDL EventHandler: a non-callable value is stored as null.
  JS::RootedValue handler(cx);
  if (args.get(0).isObject() && JS::IsCallable(&args.get(0).toObject()))
    handler.set(args.get(0));
  else handler.setNull();
  JS::SetReservedSlot(&args.thisv().toObject(), kScreenHandlerSlot, handler);
  args.rval().setUndefined();
  return true;
}
bool Method(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  const int32_t slot = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  State* state = Owner(cx, args);
  if (!state) return false;
  const bool lock = slot == kMozLock;
  state->Observe("Screen", lock ? "mozLockOrientation" : "mozUnlockOrientation",
                 int(ApiAccess::Call));
  if (lock) {
    // The DOMString overload still converts its argument before returning false.
    JS::RootedString ignored(cx, JS::ToString(cx, args.get(0)));
    if (!ignored) return false;
    args.rval().setBoolean(false);
    return true;
  }
  args.rval().setUndefined();
  return true;
}
bool Accessor(State& state, JS::HandleObject target, const char* name, int32_t slot,
              JSNative setter) {
  return state.Accessor(target, name, slot, JSPROP_ENUMERATE, Getter, setter) &&
         state.helpers->Accessor("Screen", target, name);
}
bool Operation(State& state, JS::HandleObject target, const char* name, unsigned length,
               int32_t slot) {
  JS::RootedObject method(state.cx, state.Function(Method, name, length, slot));
  return method && JS_DefineProperty(state.cx, target, name, method, JSPROP_ENUMERATE) &&
         state.helpers->Member(method, "Screen", name, "call");
}
}  // namespace

bool InstallScreenMembers(State& state) {
  JSContext* cx = state.cx;
  JS::RootedObject screen(cx, state.instances.get()[int32_t(Kind::Screen)]);
  JS::RootedObject prototype(cx);
  if (!screen || !JS_GetPrototype(cx, screen, &prototype)) return false;
  JS::SetReservedSlot(screen, kScreenHandlerSlot, JS::NullValue());
  return Accessor(state, prototype, "mozOrientation", kMozOrientation, nullptr) &&
         Accessor(state, prototype, "onmozorientationchange", kMozOrientationChange, Setter) &&
         Accessor(state, prototype, "orientation", kOrientation, nullptr) &&
         Operation(state, prototype, "mozLockOrientation", 1, kMozLock) &&
         Operation(state, prototype, "mozUnlockOrientation", 0, kMozUnlock);
}
}  // namespace ruyijs::browser_environment
