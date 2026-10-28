// Screen orientation. The values come from the existing screen profile: the
// orientation type is derived from the same width/height the Screen accessors
// report (donor dom/base/ScreenOrientation.cpp ViewportSizeToOrientationType)
// and a primary orientation always reports angle 0. This runtime has no
// fullscreen content, so lock() follows the donor's non-fullscreen path
// (GetLockOrientationPermission -> LOCK_DENIED) and rejects the promise with a
// SecurityError DOMException. The deprecated Screen members live in
// screen_members.cpp. See docs/DEVIATIONS.md D47.
#include "browser_environment/screen_orientation.h"
#include "browser_environment/schema.h"
#include "browser_environment/state.h"
#include "browser_environment/window_members.h"
#include "interaction/realm.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "runtime/runtime.h"
#include "jsfriendapi.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include <string>

namespace ruyijs::browser_environment {
namespace {
const JSClass kOrientationClass = {"ScreenOrientation", JSCLASS_HAS_RESERVED_SLOTS(2)};
enum Member : int32_t { kType = 500, kAngle, kOnChange, kLock, kUnlock };
bool IllegalOrientationConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  return Error(cx, "Illegal constructor.");
}
State* OrientationOwner(JSContext* cx, const JS::CallArgs& args) {
  auto* state = static_cast<State*>(
      js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  State* receiver = state ? ReceiverState(args, &kOrientationClass) : nullptr;
  if (!receiver) {
    Error(cx, "ScreenOrientation member called on incompatible receiver");
    return nullptr;
  }
  state = receiver;
  return state;
}
// Takes the pending DOMException the Document Model's sole thrower created, the
// only public way to obtain a DOMException value with its name set.
bool RejectSecurity(State& state, JS::HandleObject promise) {
  JSContext* cx = state.cx;
  // ThrowDOM signals "threw" through its return value (false, as in every other
  // caller); the pending DOMException is the rejection reason, so the result is
  // deliberately discarded here instead of aborting the promise path.
  (void)ThrowDOM(state, "SecurityError", "The operation is insecure.");
  JS::RootedValue error(cx);
  if (!JS_IsExceptionPending(cx) || !JS_GetPendingException(cx, &error)) return false;
  JS_ClearPendingException(cx);
  return JS::RejectPromise(cx, promise, error);
}
bool Lock(State& state, const JS::CallArgs& args) {
  JSContext* cx = state.cx;
  if (args.length() < 1) return Error(cx, "lock requires at least 1 argument");
  // WebIDL enum conversion: ToString, then membership in OrientationLockType.
  JS::RootedString raw(cx, JS::ToString(cx, args.get(0)));
  std::string text;
  if (!raw || !ToUTF8(cx, raw, &text)) return false;
  static const char* const kTypes[] = {
    "any", "natural", "landscape", "portrait", "portrait-primary",
    "portrait-secondary", "landscape-primary", "landscape-secondary"
  };
  bool known = false;
  for (const char* type : kTypes) known = known || text == type;
  if (!known)
    return Error(cx, "is not a valid value for enumeration OrientationLockType.");
  JS::RootedObject promise(cx, JS::NewPromiseObject(cx, nullptr));
  if (!promise || !RejectSecurity(state, promise)) return false;
  args.rval().setObject(*promise);
  return true;
}
bool OrientationGetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  const int32_t slot = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  State* state = OrientationOwner(cx, args);
  if (!state) return false;
  const char* name = slot == kType ? "type" : slot == kAngle ? "angle" : "onchange";
  state->Observe("ScreenOrientation", name, int(ApiAccess::Read));
  if (slot == kType) return OrientationTypeText(*state, args.rval());
  if (slot == kAngle) { args.rval().setInt32(0); return true; }
  args.rval().set(JS::GetReservedSlot(&args.thisv().toObject(), 1));
  return true;
}
bool OrientationSetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  State* state = OrientationOwner(cx, args);
  if (!state) return false;
  state->Observe("ScreenOrientation", "onchange", int(ApiAccess::Write));
  // WebIDL EventHandler: a non-callable value is stored as null.
  JS::RootedValue handler(cx);
  if (args.get(0).isObject() && JS::IsCallable(&args.get(0).toObject()))
    handler.set(args.get(0));
  else handler.setNull();
  JS::SetReservedSlot(&args.thisv().toObject(), 1, handler);
  args.rval().setUndefined();
  return true;
}
bool OrientationMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  const int32_t slot = js::GetFunctionNativeReserved(&args.callee(), 1).toInt32();
  State* state = OrientationOwner(cx, args);
  if (!state) return false;
  state->Observe("ScreenOrientation", slot == kLock ? "lock" : "unlock",
                 int(ApiAccess::Call));
  if (slot == kLock) return Lock(*state, args);
  args.rval().setUndefined();
  return true;
}
}  // namespace

bool OrientationTypeText(State& state, JS::MutableHandleValue result) {
  JS::RootedValue width(state.cx), height(state.cx);
  if (!state.Read(Kind::Screen, 0, ValueType::Integer, &width) ||
      !state.Read(Kind::Screen, 1, ValueType::Integer, &height)) return false;
  const char* text = width.toInt32() >= height.toInt32() ? "landscape-primary"
                                                        : "portrait-primary";
  JS::RootedString string(state.cx, JS_NewStringCopyZ(state.cx, text));
  if (!string) return false;
  result.setString(string);
  return true;
}

bool InstallScreenOrientation(State& state, interaction::Realm& events) {
  JSContext* cx = state.cx;
  JS::RootedObject eventPrototype(cx, events.prototype());
  JS::RootedObject prototype(cx,
      JS_NewObjectWithGivenProto(cx, &kOrientationClass, eventPrototype));
  JS::RootedObject constructor(cx,
      state.Function(IllegalOrientationConstructor, "ScreenOrientation", 0, 0, JSFUN_CONSTRUCTOR));
  JS::RootedObject instance(cx,
      JS_NewObjectWithGivenProto(cx, &kOrientationClass, prototype));
  JS::RootedString tag(cx, JS_NewStringCopyZ(cx, kOrientationClass.name));
  JS::RootedId tagId(cx, JS::PropertyKey::Symbol(
      JS::GetWellKnownSymbol(cx, JS::SymbolCode::toStringTag)));
  if (!eventPrototype || !prototype || !constructor || !instance || !tag ||
      !JS_DefineProperty(cx, constructor, "prototype", prototype,
                         JSPROP_READONLY | JSPROP_PERMANENT) ||
      !JS_DefineProperty(cx, prototype, "constructor", constructor, 0) ||
      !JS_DefinePropertyById(cx, prototype, tagId, tag, JSPROP_READONLY) ||
      !state.Accessor(prototype, "type", kType, JSPROP_ENUMERATE, OrientationGetter) ||
      !state.helpers->Accessor("ScreenOrientation", prototype, "type") ||
      !state.Accessor(prototype, "angle", kAngle, JSPROP_ENUMERATE, OrientationGetter) ||
      !state.helpers->Accessor("ScreenOrientation", prototype, "angle") ||
      !state.Accessor(prototype, "onchange", kOnChange, JSPROP_ENUMERATE, OrientationGetter,
                      OrientationSetter) ||
      !state.helpers->Accessor("ScreenOrientation", prototype, "onchange")) return false;
  const struct { const char* name; unsigned length; int32_t slot; } kMethods[] = {
    {"lock", 1, kLock}, {"unlock", 0, kUnlock}
  };
  for (const auto& spec : kMethods) {
    JS::RootedObject method(cx,
        state.Function(OrientationMethod, spec.name, spec.length, spec.slot));
    if (!method ||
        !JS_DefineProperty(cx, prototype, spec.name, method, JSPROP_ENUMERATE) ||
        !state.helpers->Member(method, "ScreenOrientation", spec.name, "call"))
      return false;
  }
  JS::SetReservedSlot(instance, 0, JS::PrivateValue(&state));
  JS::SetReservedSlot(instance, 1, JS::NullValue());
  if (!events.Attach(instance)) return false;
  state.screenOrientation = instance;
  return JS_DefineProperty(cx, state.page, "ScreenOrientation", constructor, 0) &&
         state.helpers->Interface("ScreenOrientation", prototype) &&
         state.helpers->Object("ScreenOrientation", prototype) &&
         state.helpers->Constructor("ScreenOrientation", constructor);
}
}  // namespace ruyijs::browser_environment
