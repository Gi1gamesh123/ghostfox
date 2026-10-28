// Geolocation methods, the delivery task, and the Position/Coordinates/Error
// members (docs/owners/GEOLOCATION.md). Texts and shapes follow Firefox 158.
#include "web_geolocation/internal.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"
#include <string>

namespace ruyijs::web_geolocation {
const JSClass kGeolocationClass = {"Geolocation", JSCLASS_HAS_RESERVED_SLOTS(GeoSlotCount)};
const JSClass kPositionClass = {"GeolocationPosition", JSCLASS_HAS_RESERVED_SLOTS(PositionSlotCount)};
const JSClass kCoordsClass = {"GeolocationCoordinates", JSCLASS_HAS_RESERVED_SLOTS(kCoordsSlots)};
const JSClass kErrorClass = {"GeolocationPositionError", JSCLASS_HAS_RESERVED_SLOTS(ErrorSlotCount)};
// A same-class receiver from any realm: unwrap, check the class, and use the
// receiver's own object (and, for Geolocation, its own owner State).
bool Receiver(JSContext* cx, JS::HandleValue self, const JSClass* clasp, const std::string& member,
              JS::MutableHandleObject out) {
  JSObject* object = self.isObject() ? js::CheckedUnwrapStatic(&self.toObject()) : nullptr;
  if (!object || JS::GetClass(object) != clasp)
    return WebKit::Fail(cx, ("'" + member + "' called on an object that does not implement "
                             "interface " + clasp->name + ".").c_str());
  out.set(object);
  return true;
}
namespace {
const char* const kMethods[] = {"getCurrentPosition", "watchPosition", "clearWatch"};
bool Callback(JSContext* cx, JS::HandleValue value, const char* method, int index, bool optional) {
  if (optional && value.isNullOrUndefined()) return true;
  const std::string prefix = std::string("Geolocation.") + method + ": Argument " +
                             std::to_string(index) + " is not ";
  if (!value.isObject()) return WebKit::Fail(cx, (prefix + "an object.").c_str());
  if (!JS::IsCallable(&value.toObject())) return WebKit::Fail(cx, (prefix + "callable.").c_str());
  return true;
}
// PositionOptions is read for its conversions only: a fixed source answers
// at once, as Firefox does with a fixed provider.
bool Options(JSContext* cx, JS::HandleValue value, const char* method) {
  if (value.isNullOrUndefined()) return true;
  if (!value.isObject())
    return WebKit::Fail(cx, (std::string("Geolocation.") + method +
                             ": Argument 3 can't be converted to a dictionary.").c_str());
  JS::RootedObject options(cx, &value.toObject());
  JS::RootedValue member(cx);
  double number = 0;
  for (const char* name : {"enableHighAccuracy", "maximumAge", "timeout"}) {
    if (!JS_GetProperty(cx, options, name, &member)) return false;
    if (name[0] != 'e' && !member.isUndefined() && !JS::ToNumber(cx, member, &number)) return false;
  }
  return true;
}
Outcome Decide(State& state) {
  JS::RootedValue secure(state.cx);
  if (!JS_GetProperty(state.cx, state.kit.page, "isSecureContext", &secure)) return Outcome::Denied;
  const int permission = state.permissionPort ? state.permissionPort(state.permissionContext) : 2;
  if (!secure.isTrue() || permission == 1) return Outcome::Denied;
  if (permission == 2) return Outcome::Pending;
  return state.config->HasPosition() ? Outcome::Success : Outcome::Unavailable;
}
bool NewPosition(State& state, JS::MutableHandleValue out) {
  JSContext* cx = state.cx;
  const Config& config = *state.config;
  JS::RootedObject coords(cx, state.kit.New(&kCoordsClass, state.coords.prototype));
  JS::RootedObject position(cx, state.kit.New(&kPositionClass, state.position.prototype));
  if (!coords || !position) return false;
  for (unsigned field = 0; field < kCoordsSlots; ++field) {
    const bool absent = !config.set[field] || config.null[field];
    // accuracy is required by the interface; an unset accuracy reads 0.
    JS::SetReservedSlot(coords, field, absent && field != Accuracy
        ? JS::NullValue() : JS::DoubleValue(absent ? 0 : config.values[field]));
  }
  JS::RootedValue timestamp(cx);
  if (config.set[Timestamp] && !config.null[Timestamp]) {
    timestamp.setDouble(config.values[Timestamp]);
  } else {
    JS::RootedValue date(cx, JS::ObjectValue(*state.date));
    JS::RootedValue now(cx, JS::ObjectValue(*state.dateNow));
    if (!JS::Call(cx, date, now, JS::HandleValueArray::empty(), &timestamp)) return false;
  }
  JS::SetReservedSlot(position, PositionCoords, JS::ObjectValue(*coords));
  JS::SetReservedSlot(position, PositionTimestamp, timestamp);
  out.setObject(*position);
  return true;
}
bool NewError(State& state, int32_t code, JS::MutableHandleValue out) {
  JSContext* cx = state.cx;
  static const char* const messages[] = {"", "User denied geolocation prompt",
                                         "Unknown error acquiring position", "Position acquisition timed out"};
  JS::RootedObject error(cx, state.kit.New(&kErrorClass, state.error.prototype));
  JS::RootedString text(cx, JS_NewStringCopyZ(cx, messages[code]));
  if (!error || !text) return false;
  JS::SetReservedSlot(error, ErrorCode, JS::Int32Value(code));
  JS::SetReservedSlot(error, ErrorMessage, JS::StringValue(text));
  out.setObject(*error);
  return true;
}
// One delivery: [success, error, outcome, watch id]. A cleared watch is dropped.
bool Deliver(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  // Read the callee's slots first: rval() shares its Value with callee().
  State& state = *static_cast<State*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
  JS::RootedObject record(cx, &js::GetFunctionNativeReserved(&args.callee(), 1).toObject());
  args.rval().setUndefined();
  JS::RootedValue success(cx), failure(cx), outcome(cx), id(cx), value(cx), ignored(cx);
  if (!JS_GetElement(cx, record, 0, &success) || !JS_GetElement(cx, record, 1, &failure) ||
      !JS_GetElement(cx, record, 2, &outcome) || !JS_GetElement(cx, record, 3, &id)) return false;
  if (id.toInt32() && state.cleared.count(id.toInt32())) return true;
  JSAutoRealm realm(cx, state.kit.page);
  const auto result = Outcome(outcome.toInt32());
  if (result == Outcome::Success) {
    if (!NewPosition(state, &value)) return false;
    JS::RootedValueArray<1> argv(cx);
    argv[0].set(value);
    return JS::Call(cx, JS::UndefinedHandleValue, success, argv, &ignored);
  }
  if (failure.isNullOrUndefined()) return true;
  if (!NewError(state, result == Outcome::Denied ? 1 : 2, &value)) return false;
  JS::RootedValueArray<1> argv(cx);
  argv[0].set(value);
  return JS::Call(cx, JS::UndefinedHandleValue, failure, argv, &ignored);
}
bool Queue(State& state, JS::HandleValue success, JS::HandleValue failure, Outcome outcome, int32_t id) {
  JSContext* cx = state.cx;
  if (outcome == Outcome::Pending || !state.taskPort) return true;
  JS::RootedObject record(cx, JS::NewArrayObject(cx, 4));
  if (!record || !JS_SetElement(cx, record, 0, success) || !JS_SetElement(cx, record, 1, failure) ||
      !JS_SetElement(cx, record, 2, int32_t(outcome)) || !JS_SetElement(cx, record, 3, id)) return false;
  JSFunction* function = js::NewFunctionWithReserved(cx, Deliver, 0, 0, "Geolocation callback");
  if (!function) return false;
  JS::RootedObject task(cx, JS_GetFunctionObject(function));
  js::SetFunctionNativeReserved(task, 0, JS::PrivateValue(&state));
  js::SetFunctionNativeReserved(task, 1, JS::ObjectValue(*record));
  return state.taskPort(state.taskContext, task);
}
}
bool GetCurrentPosition(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int32_t slot = WebKit::Slot(args);
  const char* method = kMethods[slot];
  JS::RootedObject self(cx);
  if (!Receiver(cx, args.thisv(), &kGeolocationClass, method, &self)) return false;
  State& state = *static_cast<State*>(JS::GetReservedSlot(self, GeoState).toPrivate());
  if (!args.length())
    return WebKit::Fail(cx, (std::string("Geolocation.") + method +
                             ": At least 1 argument required, but only 0 passed").c_str());
  if (!Callback(cx, args[0], method, 1, false) || !Callback(cx, args.get(1), method, 2, true) ||
      !Options(cx, args.get(2), method)) return false;
  const int32_t id = slot == 1 ? state.nextWatch++ : 0;
  JS::RootedValue success(cx, args[0]), failure(cx, args.get(1));
  {
    JSAutoRealm realm(cx, state.kit.page);
    if (!JS_WrapValue(cx, &success) || !JS_WrapValue(cx, &failure) ||
        !Queue(state, success, failure, Decide(state), id)) return false;
  }
  if (slot == 1) args.rval().setInt32(id);
  else args.rval().setUndefined();
  return true;
}
bool ClearWatch(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args.thisv(), &kGeolocationClass, "clearWatch", &self)) return false;
  State& state = *static_cast<State*>(JS::GetReservedSlot(self, GeoState).toPrivate());
  if (!args.length())
    return WebKit::Fail(cx, "Geolocation.clearWatch: At least 1 argument required, but only 0 passed");
  int32_t id = 0;
  if (!JS::ToInt32(cx, args[0], &id)) return false;
  if (id > 0 && id < state.nextWatch) state.cleared.insert(id);
  args.rval().setUndefined();
  return true;
}
}  // namespace ruyijs::web_geolocation
