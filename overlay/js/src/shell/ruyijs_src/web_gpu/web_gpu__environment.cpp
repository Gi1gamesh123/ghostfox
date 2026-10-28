// The "webgpu" environment-profile group's configure and describe ports. The
// group carries the stub-adapter opt-in plus the page-visible adapter values:
// adapter info strings, adapter/device feature sets, limit overrides and the
// fallback flag. Features and limits are applied to the imported
// WebGpuCapabilitiesAggregate through Model; defaults stay with that owner.
#include "web_gpu/internal.h"

#include "js/Array.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::web_gpu {
namespace {

constexpr size_t kMaxText = 256;
constexpr uint32_t kMaxFeatures = 64;
constexpr double kMaxLimit = 18446744073709551615.0;

bool ReadString(State& s, JS::HandleObject group, const char* key, std::string* out) {
  JSContext* cx = s.cx;
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, group, key, &value)) return false;
  if (value.isUndefined()) return true;
  JS::RootedString text(cx, JS::ToString(cx, value));
  std::string result;
  if (!text || !ToUTF8(cx, text, &result)) return false;
  if (result.size() > kMaxText) return WebKit::Fail(cx, "webgpu text field is too long");
  *out = result;
  return true;
}

// Reads an optional array of strings; *given tells whether the key was present.
bool ReadStrings(State& s, JS::HandleObject group, const char* key,
                 std::vector<std::string>* out, bool* given) {
  JSContext* cx = s.cx;
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, group, key, &value)) return false;
  *given = !value.isUndefined();
  if (!*given) return true;
  bool isArray = false;
  if (!JS::IsArrayObject(cx, value, &isArray)) return false;
  if (!isArray) return WebKit::Fail(cx, "webgpu feature list must be an array");
  JS::RootedObject array(cx, &value.toObject());
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, array, &length)) return false;
  if (length > kMaxFeatures) return WebKit::Fail(cx, "webgpu feature list is too long");
  out->clear();
  for (uint32_t i = 0; i < length; ++i) {
    JS::RootedValue item(cx);
    if (!JS_GetElement(cx, array, i, &item)) return false;
    JS::RootedString text(cx, JS::ToString(cx, item));
    std::string feature;
    if (!text || !ToUTF8(cx, text, &feature)) return false;
    if (feature.empty() || feature.size() > kMaxText)
      return WebKit::Fail(cx, "webgpu feature name is invalid");
    out->push_back(feature);
  }
  return true;
}

bool ReadLimits(State& s, JS::HandleObject group, std::vector<LimitSpec>* out) {
  JSContext* cx = s.cx;
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, group, "limits", &value)) return false;
  if (value.isUndefined()) return true;
  if (!value.isObject()) return WebKit::Fail(cx, "webgpu limits must be an object");
  JS::RootedObject map(cx, &value.toObject());
  JS::Rooted<JS::IdVector> keys(cx, JS::IdVector(cx));
  if (!JS_Enumerate(cx, map, &keys)) return false;
  for (size_t i = 0; i < keys.length(); ++i) {
    JS::RootedId id(cx, keys[i]);
    JS::RootedValue key(cx), item(cx);
    if (!JS_IdToValue(cx, id, &key) || !JS_GetPropertyById(cx, map, id, &item)) return false;
    JS::RootedString text(cx, JS::ToString(cx, key));
    std::string name;
    if (!text || !ToUTF8(cx, text, &name)) return false;
    double number = 0.0;
    if (!JS::ToNumber(cx, item, &number)) return false;
    if (Limit(s, name.c_str()) <= 0.0)
      return WebKit::Fail(cx, "webgpu limits names an unknown limit");
    if (!std::isfinite(number) || number < 1.0 || number > kMaxLimit ||
        number != std::floor(number))
      return WebKit::Fail(cx, "webgpu limit must be a positive integer");
    out->push_back(LimitSpec{name, number});
  }
  return true;
}

bool StringArray(State& s, const std::vector<std::string>& list, JS::MutableHandleValue out) {
  JSContext* cx = s.cx;
  JS::RootedObject array(cx, JS::NewArrayObject(cx, 0));
  if (!array) return false;
  for (uint32_t i = 0; i < list.size(); ++i) {
    JS::RootedValue item(cx);
    if (!StringValue(cx, list[i], &item) || !JS_SetElement(cx, array, i, item)) return false;
  }
  out.setObject(*array);
  return true;
}

bool Define(State& s, JS::HandleObject target, const char* key, JS::HandleValue value) {
  return JS_DefineProperty(s.cx, target, key, value, JSPROP_ENUMERATE);
}

}  // namespace

bool GpuPort(void* context, JS::HandleObject group) {
  State& state = *static_cast<State*>(context);
  JSContext* cx = state.cx;
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, group, "enabled", &value)) return false;
  const bool enabled = value.isUndefined() ? state.enabled : JS::ToBoolean(value);
  if (!JS_GetProperty(cx, group, "fallback", &value)) return false;
  const bool fallback = value.isUndefined() ? state.fallback : JS::ToBoolean(value);

  std::string vendor = state.info_vendor, architecture = state.info_architecture,
              device = state.info_device, description = state.info_description;
  if (!ReadString(state, group, "vendor", &vendor) ||
      !ReadString(state, group, "architecture", &architecture) ||
      !ReadString(state, group, "device", &device) ||
      !ReadString(state, group, "description", &description)) return false;

  std::vector<std::string> adapter = state.model.adapter_features(),
                           device_features = state.model.device_features();
  bool adapter_given = false, device_given = false;
  if (!ReadStrings(state, group, "features", &adapter, &adapter_given) ||
      !ReadStrings(state, group, "deviceFeatures", &device_features, &device_given))
    return false;
  std::vector<LimitSpec> limits;
  if (!ReadLimits(state, group, &limits)) return false;

  // Everything validated; apply. Configure rejects a device set that is not a
  // subset of the adapter set and leaves the previous sets in place.
  if ((adapter_given || device_given) && !state.model.Configure(adapter, device_features))
    return WebKit::Fail(cx, "webgpu deviceFeatures must be a subset of features");
  for (const auto& limit : limits) state.model.SetLimit(limit.name, limit.value);
  state.enabled = enabled;
  state.fallback = fallback;
  state.info_vendor = vendor;
  state.info_architecture = architecture;
  state.info_device = device;
  state.info_description = description;
  return true;
}

bool GpuDescribe(void* context, JS::MutableHandleValue out) {
  State& state = *static_cast<State*>(context);
  JSContext* cx = state.cx;
  JS::RootedObject group(cx, JS_NewPlainObject(cx));
  if (!group) return false;
  JS::RootedValue value(cx, JS::BooleanValue(state.enabled));
  if (!Define(state, group, "enabled", value)) return false;
  const std::pair<const char*, const std::string*> texts[] = {
      {"vendor", &state.info_vendor}, {"architecture", &state.info_architecture},
      {"device", &state.info_device}, {"description", &state.info_description}};
  for (const auto& text : texts)
    if (!StringValue(cx, *text.second, &value) || !Define(state, group, text.first, value))
      return false;
  if (!StringArray(state, state.model.adapter_features(), &value) ||
      !Define(state, group, "features", value) ||
      !StringArray(state, state.model.device_features(), &value) ||
      !Define(state, group, "deviceFeatures", value)) return false;
  JS::RootedObject limits(cx, JS_NewPlainObject(cx));
  if (!limits) return false;
  for (const auto& limit : state.model.limits()) {
    value.setDouble(limit.value);
    if (!Define(state, limits, limit.name.c_str(), value)) return false;
  }
  value.setObject(*limits);
  if (!Define(state, group, "limits", value)) return false;
  value.setBoolean(state.fallback);
  if (!Define(state, group, "fallback", value)) return false;
  out.setObject(*group);
  return true;
}

}  // namespace ruyijs::web_gpu
