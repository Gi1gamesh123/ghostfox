#include "web_gpu/internal.h"
#include "web_gpu/value_objects.h"

#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"

namespace ruyijs::web_gpu {
namespace {

// The specification's requiredFeatures must be a subset of the adapter's, and
// requiredLimits may only ask for at most the adapter's value.
bool ReadRequiredFeatures(State& state, JS::HandleObject descriptor) {
  JSContext* cx = state.cx;
  JS::RootedValue required(cx);
  if (!JS_GetProperty(cx, descriptor, "requiredFeatures", &required)) return false;
  if (required.isUndefined() || required.isNull()) return true;
  bool isArray = false;
  if (!JS::IsArrayObject(cx, required, &isArray))
    return false;
  if (!isArray)
    return ThrowError(state, "TypeError", nullptr,
                      "requiredFeatures must be a sequence of feature names");
  uint32_t length = 0;
  JS::RootedObject array(cx, &required.toObject());
  if (!JS::GetArrayLength(cx, array, &length)) return false;
  for (uint32_t index = 0; index < length; ++index) {
    JS::RootedValue element(cx);
    if (!JS_GetElement(cx, array, index, &element)) return false;
    JS::RootedString text(cx, JS::ToString(cx, element));
    std::string feature;
    if (!text || !ToUTF8(cx, text, &feature)) return false;
    if (!state.model.AdapterHas(feature))
      return ThrowOperation(state, "the adapter does not support a required feature");
  }
  return true;
}

bool ReadRequiredLimits(State& state, JS::HandleObject descriptor) {
  JSContext* cx = state.cx;
  JS::RootedValue required(cx);
  if (!JS_GetProperty(cx, descriptor, "requiredLimits", &required)) return false;
  if (required.isUndefined() || required.isNull()) return true;
  if (!required.isObject())
    return ThrowError(state, "TypeError", nullptr,
                      "requiredLimits must be a dictionary of limit values");
  JS::RootedObject limits(cx, &required.toObject());
  JS::Rooted<JS::IdVector> keys(cx, JS::IdVector(cx));
  if (!JS_Enumerate(cx, limits, &keys)) return false;
  for (size_t index = 0; index < keys.length(); ++index) {
    JS::RootedId id(cx, keys[index]);
    JS::RootedValue key(cx), value(cx);
    if (!JS_IdToValue(cx, id, &key)) return false;
    JS::RootedString text(cx, JS::ToString(cx, key));
    std::string name;
    if (!text || !ToUTF8(cx, text, &name)) return false;
    const double maximum = Limit(state, name.c_str());
    if (maximum <= 0.0)
      return ThrowOperation(state, "requiredLimits names an unknown limit");
    double wanted = 0.0;
    if (!JS_GetPropertyById(cx, limits, id, &value) ||
        !JS::ToNumber(cx, value, &wanted) || std::isnan(wanted))
      return ThrowError(state, "TypeError", nullptr,
                        "requiredLimits values must be numbers");
    if (wanted > maximum || wanted < 0.0)
      return ThrowOperation(state, "requiredLimits exceeds the adapter limit");
  }
  return true;
}

}  // namespace

JSObject* NewAdapter(State& state) {
  JSContext* cx = state.cx;
  const JSAutoRealm realm(cx, state.kit.page);
  JS::RootedObject adapter(cx, state.kit.New(&kAdapterClass, state.adapter.prototype));
  if (!adapter) return nullptr;
  JS::SetReservedSlot(adapter, kAdapterFeatures, JS::NullValue());
  JS::SetReservedSlot(adapter, kAdapterLimits, JS::NullValue());
  JS::SetReservedSlot(adapter, kAdapterInfo, JS::NullValue());
  return adapter.get();
}

bool GpuMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject self(cx);
  if (!CheckGpu(cx, args.thisv(), &self)) return false;
  switch (WebKit::Slot(args)) {
    case kGpuRequestAdapter: {
      JS::RootedValue options(cx, args.get(0));
      if (!options.isUndefined() && !options.isObject())
        return ThrowError(*state, "TypeError", nullptr,
                          "requestAdapter requires an options dictionary");
      // No GPU: with the default profile the honest answer is null, not a stub
      // adapter that leads the page into paths it cannot finish (deviation D39).
      if (!state->enabled) return Resolve(*state, JS::NullValue(), args.rval());
      JS::RootedObject adapter(cx, NewAdapter(*state));
      if (!adapter) return false;
      return Resolve(*state, JS::ObjectValue(*adapter), args.rval());
    }
    case kGpuPreferredCanvasFormat: {
      JS::RootedString format(cx, JS_NewStringCopyZ(cx, kPreferredCanvasFormat));
      if (!format) return false;
      args.rval().setString(format);
      return true;
    }
    default:
      return WebKit::Fail(cx, "unknown GPU method");
  }
}

bool AdapterProperty(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  State* state = ValueReceiver(cx, args.thisv(), &kAdapterClass, &self);
  if (!state) return false;
  auto read = [&]() -> bool {
  switch (WebKit::Slot(args)) {
    case kAdapterFeatures: {
      JS::RootedValue cached(cx, JS::GetReservedSlot(self, kAdapterFeatures));
      if (cached.isObject()) {
        args.rval().set(cached);
        return true;
      }
      JS::RootedObject features(cx, NewFeatures(*state, 0));
      if (!features) return false;
      JS::SetReservedSlot(self, kAdapterFeatures, JS::ObjectValue(*features));
      args.rval().setObject(*features);
      return true;
    }
    case kAdapterLimits: {
      JS::RootedValue cached(cx, JS::GetReservedSlot(self, kAdapterLimits));
      if (cached.isObject()) {
        args.rval().set(cached);
        return true;
      }
      JS::RootedObject limits(cx);
      if (!BuildLimits(*state, &limits)) return false;
      JS::SetReservedSlot(self, kAdapterLimits, JS::ObjectValue(*limits));
      args.rval().setObject(*limits);
      return true;
    }
    case kAdapterInfo: {
      JS::RootedValue cached(cx, JS::GetReservedSlot(self, kAdapterInfo));
      if (cached.isObject()) {
        args.rval().set(cached);
        return true;
      }
      JS::RootedObject info(cx, NewAdapterInfo(*state));
      if (!info) return false;
      JS::SetReservedSlot(self, kAdapterInfo, JS::ObjectValue(*info));
      args.rval().setObject(*info);
      return true;
    }
    default:
      args.rval().setBoolean(state->fallback);
      return true;
  }
  };
  { JSAutoRealm realm(cx, self); if (!read()) return false; }
  return JS_WrapValue(cx, args.rval());
}

bool AdapterMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject self(cx);
  if (!CheckAdapter(cx, args.thisv(), &self)) return false;
  JS::RootedValue descriptor(cx, args.get(0));
  if (!descriptor.isUndefined() && !descriptor.isObject())
    return ThrowError(*state, "TypeError", nullptr,
                      "requestDevice requires a descriptor dictionary");
  if (descriptor.isObject()) {
    JS::RootedObject dictionary(cx, &descriptor.toObject());
    if (!ReadRequiredFeatures(*state, dictionary) ||
        !ReadRequiredLimits(*state, dictionary))
      return false;
  }
  JS::RootedObject device(cx, NewDevice(*state, state->device.prototype));
  if (!device) return false;
  return Resolve(*state, JS::ObjectValue(*device), args.rval());
}

}  // namespace ruyijs::web_gpu
