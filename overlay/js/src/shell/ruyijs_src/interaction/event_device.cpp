// The T3c device sensor events: DeviceMotionEvent and DeviceOrientationEvent
// plus the two [LegacyNoInterfaceObject] helper shapes DeviceMotionEvent
// exposes through acceleration/accelerationIncludingGravity/rotationRate
// (dom/webidl/DeviceMotionEvent.webidl). This TU owns their property and method
// tables, the two legacy init methods and the per-realm helper prototypes; the
// device classes and the helpers share one slot layout and one factory, so they
// stay together while the other T3c payload classes are split across
// event_supplemental.cpp, event_input_extra.cpp and event_rtc.cpp.
#include "interaction/internal.h"
#include "runtime/page_helpers.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include <string>

namespace ruyijs::interaction {
namespace {
JSObject* Self(const JS::CallArgs& args) { return &args.thisv().toObject(); }

// The two [LegacyNoInterfaceObject] shapes: no interface object and no global
// property, one prototype per realm reachable only through the attributes that
// return helper instances.
const JSClass kAccelerationShape = {"DeviceAcceleration",
                                    JSCLASS_HAS_RESERVED_SLOTS(4)};
const JSClass kRotationRateShape = {"DeviceRotationRate",
                                    JSCLASS_HAS_RESERVED_SLOTS(4)};
// x/y/z (or alpha/beta/gamma) plus the owning event. The owner slot keeps the
// event alive like the donor's cycle-collected mOwner link
// (dom/events/DeviceMotionEvent.h) and feeds the monitor report; the values
// themselves are the helper's own, so a helper whose event was collected still
// reads them.
enum HelperSlot : uint32_t { kHelperX, kHelperY, kHelperZ, kHelperOwner };

bool HelperValue(JSContext* cx, const JS::CallArgs& args, const JSClass* shape,
                 const char* iface, const char* member, uint32_t slot) {
  if (!args.thisv().isObject() ||
      JS::GetClass(&args.thisv().toObject()) != shape)
    return Error(cx, "TypeError", "Device sensor value receiver is incompatible");
  JS::RootedObject self(cx, &args.thisv().toObject());
  JS::RootedValue owner(cx, JS::GetReservedSlot(self, kHelperOwner));
  if (owner.isObject()) {
    State* state = Owner(cx, &owner.toObject(), true);
    Event* event = state ? state->event(&owner.toObject()) : nullptr;
    if (event) ReportEventAccess(cx, event, iface, member, int(ApiAccess::Read));
  }
  args.rval().set(JS::GetReservedSlot(self, slot));
  return JS_WrapValue(cx, args.rval());
}
#define HELPER_GETTER(name, shape, iface, member, slot)                \
  bool name(JSContext* cx, unsigned argc, JS::Value* vp) {             \
    return HelperValue(cx, JS::CallArgsFromVp(argc, vp), shape, iface, \
                       member, slot);                                  \
  }
HELPER_GETTER(GetAccelerationX, &kAccelerationShape, "DeviceAcceleration", "x",
              kHelperX)
HELPER_GETTER(GetAccelerationY, &kAccelerationShape, "DeviceAcceleration", "y",
              kHelperY)
HELPER_GETTER(GetAccelerationZ, &kAccelerationShape, "DeviceAcceleration", "z",
              kHelperZ)
HELPER_GETTER(GetRotationRateAlpha, &kRotationRateShape, "DeviceRotationRate",
              "alpha", kHelperX)
HELPER_GETTER(GetRotationRateBeta, &kRotationRateShape, "DeviceRotationRate",
              "beta", kHelperY)
HELPER_GETTER(GetRotationRateGamma, &kRotationRateShape, "DeviceRotationRate",
              "gamma", kHelperZ)
#undef HELPER_GETTER

const JSPropertySpec kAccelerationProperties[] = {
  JS_PSG("x", GetAccelerationX, JSPROP_ENUMERATE),
  JS_PSG("y", GetAccelerationY, JSPROP_ENUMERATE),
  JS_PSG("z", GetAccelerationZ, JSPROP_ENUMERATE),
  JS_PS_END
};
const JSPropertySpec kRotationRateProperties[] = {
  JS_PSG("alpha", GetRotationRateAlpha, JSPROP_ENUMERATE),
  JS_PSG("beta", GetRotationRateBeta, JSPROP_ENUMERATE),
  JS_PSG("gamma", GetRotationRateGamma, JSPROP_ENUMERATE),
  JS_PS_END
};

// Read-only member of one of the two device interfaces: brand-check the
// receiver, report the access of the installed member and re-wrap the traced
// slot the constructor wrote.
bool Slot(JSContext* cx, const JS::CallArgs& args, const char* member, Iface iface,
          uint32_t slot) {
  Event* event = Receiver(cx, args, iface);
  if (!event) return false;
  ReportEventAccess(cx, event, InterfaceName(iface), member, int(ApiAccess::Read));
  args.rval().set(JS::GetReservedSlot(Self(args), slot));
  return JS_WrapValue(cx, args.rval());
}
#define DEVICE_GETTER(name, member, iface, slot)                          \
  bool name(JSContext* cx, unsigned argc, JS::Value* vp) {                \
    return Slot(cx, JS::CallArgsFromVp(argc, vp), member, iface, slot);   \
  }
DEVICE_GETTER(GetMotionAcceleration, "acceleration", Iface::DeviceMotion,
              kEventMotionAcceleration)
DEVICE_GETTER(GetMotionAccelerationIncludingGravity,
              "accelerationIncludingGravity", Iface::DeviceMotion,
              kEventMotionGravity)
DEVICE_GETTER(GetMotionRotationRate, "rotationRate", Iface::DeviceMotion,
              kEventMotionRotation)
DEVICE_GETTER(GetMotionInterval, "interval", Iface::DeviceMotion,
              kEventMotionInterval)
DEVICE_GETTER(GetOrientationAlpha, "alpha", Iface::DeviceOrientation,
              kEventOrientationAlpha)
DEVICE_GETTER(GetOrientationBeta, "beta", Iface::DeviceOrientation,
              kEventOrientationBeta)
DEVICE_GETTER(GetOrientationGamma, "gamma", Iface::DeviceOrientation,
              kEventOrientationGamma)
DEVICE_GETTER(GetOrientationAbsolute, "absolute", Iface::DeviceOrientation,
              kEventOrientationAbsolute)
#undef DEVICE_GETTER

// double? member of the device dictionaries: a missing, undefined or null
// member keeps the null default, anything else is the WebIDL double conversion.
bool ReadNullableNumber(JSContext* cx, JS::HandleObject dict, const char* name,
                        bool* is_null, double* out) {
  if (!dict) return true;
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, dict, name, &value)) return false;
  if (value.isNullOrUndefined()) return true;
  *is_null = false;
  return JS::ToNumber(cx, value, out);
}
// One dictionary-valued member of DeviceMotionEventInit: the member's default
// is the empty dictionary, so a missing, undefined or null member yields the
// shape's null defaults and a primitive is the WebIDL dictionary TypeError
// (obj-jscall-check/dom/bindings/DeviceMotionEventBinding.cpp:325-420).
bool ReadNestedDictionary(JSContext* cx, JS::HandleObject dict, const char* name,
                          JS::MutableHandleObject out) {
  out.set(nullptr);
  if (!dict) return true;
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, dict, name, &value)) return false;
  if (value.isNullOrUndefined()) return true;
  if (!value.isObject()) {
    std::string message = std::string("DeviceMotionEventInit member '") + name +
                          "' requires a dictionary";
    return Error(cx, "TypeError", message.c_str());
  }
  out.set(&value.toObject());
  return true;
}

struct NullableTriple {
  bool is_null[3] = {true, true, true};
  double value[3] = {0.0, 0.0, 0.0};
};
const char* const kAccelerationNames[3] = {"x", "y", "z"};
const char* const kRotationRateNames[3] = {"alpha", "beta", "gamma"};
bool ReadTriple(JSContext* cx, JS::HandleObject dict, const char* const names[3],
                NullableTriple* out) {
  if (!dict) return true;
  for (int index = 0; index < 3; ++index)
    if (!ReadNullableNumber(cx, dict, names[index], &out->is_null[index],
                            &out->value[index])) return false;
  return true;
}

// Materializes one helper instance: the three nullable values plus the owning
// event, created on the shape's realm prototype.
bool MakeHelper(JSContext* cx, const JSClass* shape, JS::HandleObject prototype,
                JS::HandleObject owner, const NullableTriple& triple,
                JS::MutableHandleObject out) {
  JS::RootedObject helper(cx, JS_NewObjectWithGivenProto(cx, shape, prototype));
  if (!helper) return false;
  JS::SetReservedSlot(helper, kHelperX, triple.is_null[0]
      ? JS::NullValue() : JS::NumberValue(triple.value[0]));
  JS::SetReservedSlot(helper, kHelperY, triple.is_null[1]
      ? JS::NullValue() : JS::NumberValue(triple.value[1]));
  JS::SetReservedSlot(helper, kHelperZ, triple.is_null[2]
      ? JS::NullValue() : JS::NumberValue(triple.value[2]));
  JS::SetReservedSlot(helper, kHelperOwner, JS::ObjectValue(*owner));
  out.set(helper);
  return true;
}

bool InitMotionFields(JSContext* cx, JS::HandleObject dict,
                      JS::HandleObject object) {
  State* state = RealmState(cx);
  if (!state || !state->deviceAccelerationPrototype ||
      !state->deviceRotationRatePrototype)
    return Error(cx, "TypeError", "Device sensor helpers are not installed");
  JS::RootedObject acceleration_dict(cx), gravity_dict(cx), rotation_dict(cx);
  if (!ReadNestedDictionary(cx, dict, "acceleration", &acceleration_dict) ||
      !ReadNestedDictionary(cx, dict, "accelerationIncludingGravity",
                            &gravity_dict) ||
      !ReadNestedDictionary(cx, dict, "rotationRate", &rotation_dict)) return false;
  bool interval_null = true;
  double interval = 0.0;
  if (!ReadNullableNumber(cx, dict, "interval", &interval_null, &interval))
    return false;
  NullableTriple acceleration, gravity, rotation;
  if (!ReadTriple(cx, acceleration_dict, kAccelerationNames, &acceleration) ||
      !ReadTriple(cx, gravity_dict, kAccelerationNames, &gravity) ||
      !ReadTriple(cx, rotation_dict, kRotationRateNames, &rotation)) return false;
  JS::RootedObject acceleration_object(cx), gravity_object(cx), rotation_object(cx);
  if (!MakeHelper(cx, &kAccelerationShape, state->deviceAccelerationPrototype,
                  object, acceleration, &acceleration_object) ||
      !MakeHelper(cx, &kAccelerationShape, state->deviceAccelerationPrototype,
                  object, gravity, &gravity_object) ||
      !MakeHelper(cx, &kRotationRateShape, state->deviceRotationRatePrototype,
                  object, rotation, &rotation_object)) return false;
  JS::SetReservedSlot(object, kEventMotionAcceleration,
                      JS::ObjectValue(*acceleration_object));
  JS::SetReservedSlot(object, kEventMotionGravity, JS::ObjectValue(*gravity_object));
  JS::SetReservedSlot(object, kEventMotionRotation,
                      JS::ObjectValue(*rotation_object));
  JS::SetReservedSlot(object, kEventMotionInterval,
                      interval_null ? JS::NullValue() : JS::NumberValue(interval));
  return true;
}

bool InitOrientationFields(JSContext* cx, JS::HandleObject dict,
                           JS::HandleObject object) {
  bool alpha_null = true, beta_null = true, gamma_null = true;
  double alpha = 0.0, beta = 0.0, gamma = 0.0;
  bool absolute = false;
  if (dict && (!ReadNullableNumber(cx, dict, "alpha", &alpha_null, &alpha) ||
               !ReadNullableNumber(cx, dict, "beta", &beta_null, &beta) ||
               !ReadNullableNumber(cx, dict, "gamma", &gamma_null, &gamma) ||
               !ReadInitBool(cx, dict, "absolute", &absolute))) return false;
  JS::SetReservedSlot(object, kEventOrientationAlpha,
                      alpha_null ? JS::NullValue() : JS::NumberValue(alpha));
  JS::SetReservedSlot(object, kEventOrientationBeta,
                      beta_null ? JS::NullValue() : JS::NumberValue(beta));
  JS::SetReservedSlot(object, kEventOrientationGamma,
                      gamma_null ? JS::NullValue() : JS::NumberValue(gamma));
  SetSlotBool(object, kEventOrientationAbsolute, absolute);
  return true;
}

// initDeviceMotionEvent(7) (dom/webidl/DeviceMotionEvent.webidl). The donor's
// method is dispatching-guarded as a whole (dom/events/DeviceMotionEvent.cpp
// InitDeviceMotionEvent), so a mid-dispatch call is a no-op.
bool InitDeviceMotionEvent(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Receiver(cx, args, Iface::DeviceMotion);
  if (!event) return false;
  ReportEventAccess(cx, event, "DeviceMotionEvent", "initDeviceMotionEvent",
                    int(ApiAccess::Call));
  if (args.length() < 1)
    return Error(cx, "TypeError",
                 "DeviceMotionEvent.initDeviceMotionEvent requires a type");
  args.rval().setUndefined();
  if (event->value.dispatching()) return true;
  JS::RootedObject object(cx, Self(args));
  if (!LegacyBase(cx, event, args)) return false;
  State* state = RealmState(cx);
  if (!state || !state->deviceAccelerationPrototype ||
      !state->deviceRotationRatePrototype)
    return Error(cx, "TypeError", "Device sensor helpers are not installed");
  JS::RootedObject acceleration_dict(cx), gravity_dict(cx), rotation_dict(cx);
  if (!LegacyDictionary(cx, args, 3, &acceleration_dict) ||
      !LegacyDictionary(cx, args, 4, &gravity_dict) ||
      !LegacyDictionary(cx, args, 5, &rotation_dict)) return false;
  bool interval_null = true;
  double interval = 0.0;
  if (!LegacyNullableDouble(cx, args, 6, &interval_null, &interval)) return false;
  NullableTriple acceleration, gravity, rotation;
  if (!ReadTriple(cx, acceleration_dict, kAccelerationNames, &acceleration) ||
      !ReadTriple(cx, gravity_dict, kAccelerationNames, &gravity) ||
      !ReadTriple(cx, rotation_dict, kRotationRateNames, &rotation)) return false;
  JS::RootedObject acceleration_object(cx), gravity_object(cx), rotation_object(cx);
  if (!MakeHelper(cx, &kAccelerationShape, state->deviceAccelerationPrototype,
                  object, acceleration, &acceleration_object) ||
      !MakeHelper(cx, &kAccelerationShape, state->deviceAccelerationPrototype,
                  object, gravity, &gravity_object) ||
      !MakeHelper(cx, &kRotationRateShape, state->deviceRotationRatePrototype,
                  object, rotation, &rotation_object)) return false;
  JS::SetReservedSlot(object, kEventMotionAcceleration,
                      JS::ObjectValue(*acceleration_object));
  JS::SetReservedSlot(object, kEventMotionGravity, JS::ObjectValue(*gravity_object));
  JS::SetReservedSlot(object, kEventMotionRotation,
                      JS::ObjectValue(*rotation_object));
  JS::SetReservedSlot(object, kEventMotionInterval,
                      interval_null ? JS::NullValue() : JS::NumberValue(interval));
  return true;
}

// initDeviceOrientationEvent(7) (dom/webidl/DeviceOrientationEvent.webidl).
// The donor's generated LegacyEventInit method calls Event::InitEvent - itself
// dispatching-guarded - and then writes the payload unconditionally
// (obj-jscall-check/dom/bindings/DeviceOrientationEvent.cpp:95-104), so a
// mid-dispatch call still updates the sensor values but never the type.
bool InitDeviceOrientationEvent(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Event* event = Receiver(cx, args, Iface::DeviceOrientation);
  if (!event) return false;
  ReportEventAccess(cx, event, "DeviceOrientationEvent",
                    "initDeviceOrientationEvent", int(ApiAccess::Call));
  if (args.length() < 1)
    return Error(cx, "TypeError",
                 "DeviceOrientationEvent.initDeviceOrientationEvent requires a type");
  args.rval().setUndefined();
  JS::RootedObject object(cx, Self(args));
  bool alpha_null = true, beta_null = true, gamma_null = true;
  double alpha = 0.0, beta = 0.0, gamma = 0.0;
  if (!LegacyNullableDouble(cx, args, 3, &alpha_null, &alpha) ||
      !LegacyNullableDouble(cx, args, 4, &beta_null, &beta) ||
      !LegacyNullableDouble(cx, args, 5, &gamma_null, &gamma)) return false;
  bool absolute = false;
  LegacyBool(args, 6, &absolute);
  if (!event->value.dispatching() && !LegacyBase(cx, event, args)) return false;
  JS::SetReservedSlot(object, kEventOrientationAlpha,
                      alpha_null ? JS::NullValue() : JS::NumberValue(alpha));
  JS::SetReservedSlot(object, kEventOrientationBeta,
                      beta_null ? JS::NullValue() : JS::NumberValue(beta));
  JS::SetReservedSlot(object, kEventOrientationGamma,
                      gamma_null ? JS::NullValue() : JS::NumberValue(gamma));
  SetSlotBool(object, kEventOrientationAbsolute, absolute);
  return true;
}
}  // namespace

// One optional dictionary argument of a legacy initializer. Undefined and null
// both mean the argument's default (the empty dictionary); a non-object
// primitive is the WebIDL dictionary TypeError and an object is read by the
// caller.
bool LegacyDictionary(JSContext* cx, const JS::CallArgs& args, unsigned index,
                      JS::MutableHandleObject out) {
  out.set(nullptr);
  if (!args.hasDefined(index) || args[index].isNullOrUndefined()) return true;
  if (!args[index].isObject())
    return Error(cx, "TypeError", "Event initializer requires a dictionary");
  out.set(&args[index].toObject());
  return true;
}
// One optional double? argument of a legacy initializer; undefined and null
// keep the null default.
bool LegacyNullableDouble(JSContext* cx, const JS::CallArgs& args, unsigned index,
                          bool* is_null, double* out) {
  if (!args.hasDefined(index) || args[index].isNullOrUndefined()) {
    *is_null = true;
    return true;
  }
  *is_null = false;
  return JS::ToNumber(cx, args[index], out);
}

extern const JSPropertySpec kDeviceMotionProperties[] = {
  JS_PSG("acceleration", GetMotionAcceleration, JSPROP_ENUMERATE),
  JS_PSG("accelerationIncludingGravity", GetMotionAccelerationIncludingGravity,
         JSPROP_ENUMERATE),
  JS_PSG("rotationRate", GetMotionRotationRate, JSPROP_ENUMERATE),
  JS_PSG("interval", GetMotionInterval, JSPROP_ENUMERATE),
  JS_PS_END
};
extern const JSPropertySpec kDeviceOrientationProperties[] = {
  JS_PSG("alpha", GetOrientationAlpha, JSPROP_ENUMERATE),
  JS_PSG("beta", GetOrientationBeta, JSPROP_ENUMERATE),
  JS_PSG("gamma", GetOrientationGamma, JSPROP_ENUMERATE),
  JS_PSG("absolute", GetOrientationAbsolute, JSPROP_ENUMERATE),
  JS_PS_END
};
extern const JSFunctionSpec kDeviceMotionMethods[] = {
  JS_FN("initDeviceMotionEvent", InitDeviceMotionEvent, 1, JSPROP_ENUMERATE),
  JS_FS_END
};
extern const JSFunctionSpec kDeviceOrientationMethods[] = {
  JS_FN("initDeviceOrientationEvent", InitDeviceOrientationEvent, 1,
        JSPROP_ENUMERATE),
  JS_FS_END
};

bool InitDeviceFields(JSContext* cx, Iface iface, JS::HandleObject dict,
                      JS::HandleObject object) {
  switch (iface) {
    case Iface::DeviceMotion:
      return InitMotionFields(cx, dict, object);
    case Iface::DeviceOrientation:
      return InitOrientationFields(cx, dict, object);
    default:
      return true;
  }
}

// The two [LegacyNoInterfaceObject] helper prototypes: ordinary per-realm
// prototype objects carrying the read-only accessors, with no interface object
// and no global property (the donor creates them through the same
// CreateInterfaceObjects path with a null interface cache, BindingUtils.cpp
// CreateInterfaceObjects). Instances attach them in InitMotionFields.
bool InstallSupplementalEventObjects(State& state) {
  JSContext* cx = state.cx;
  JS::RootedObject plain(cx, JS::GetRealmObjectPrototype(cx));
  if (!plain) return false;
  JS::RootedObject acceleration(cx, JS_NewObjectWithGivenProto(cx, nullptr, plain));
  JS::RootedObject rotation(cx, JS_NewObjectWithGivenProto(cx, nullptr, plain));
  if (!acceleration || !rotation) return false;
  if (!JS_DefineProperties(cx, acceleration, kAccelerationProperties) ||
      !JS_DefineProperties(cx, rotation, kRotationRateProperties)) return false;
  if (!Tag(cx, acceleration, "DeviceAcceleration") ||
      !Tag(cx, rotation, "DeviceRotationRate")) return false;
  state.deviceAccelerationPrototype = acceleration;
  state.deviceRotationRatePrototype = rotation;
  if (!state.helpers->Interface("DeviceAcceleration", acceleration) ||
      !state.helpers->Interface("DeviceRotationRate", rotation)) return false;
  for (const JSPropertySpec* property = kAccelerationProperties;
       property->name.string(); ++property)
    if (!state.helpers->Accessor("DeviceAcceleration", acceleration,
                                 property->name.string())) return false;
  for (const JSPropertySpec* property = kRotationRateProperties;
       property->name.string(); ++property)
    if (!state.helpers->Accessor("DeviceRotationRate", rotation,
                                 property->name.string())) return false;
  return true;
}
}  // namespace ruyijs::interaction
