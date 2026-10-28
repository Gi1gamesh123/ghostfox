// The "media" environment-profile group's configure and describe ports. The
// profile carries the MediaDevices list and the per-content-type
// MediaCapabilities decoding answers; the effective values live in this
// owner's State (state.h) and the domain table stays the fallback for every
// content type the profile does not pin.
#include "web_media/state.h"
#include "web_media/realm.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "runtime/runtime.h"

namespace ruyijs::web_media {
namespace {
using domain::web_media::Capability;
bool String(JSContext* cx, JS::HandleValue value, std::string* out) {
  if (!value.isString()) return WebKit::Fail(cx, "Media device entry must be a string");
  JS::RootedString text(cx, value.toString());
  return text && ToUTF8(cx, text, out);
}
// "kind|label|deviceId|groupId", the same field order MediaDeviceInfo declares.
bool Device(JSContext* cx, JS::HandleValue value, MediaDevice* out) {
  std::string text;
  if (!String(cx, value, &text)) return false;
  std::vector<std::string> parts(1);
  for (char c : text) {
    if (c == '|') parts.resize(parts.size() + 1);
    else parts.back().push_back(c);
  }
  if (parts.size() != 4)
    return WebKit::Fail(cx, "Media device must be kind|label|deviceId|groupId");
  const std::string& kind = parts[0];
  if (kind != "audioinput" && kind != "audiooutput" && kind != "videoinput")
    return WebKit::Fail(cx, "Media device kind must be audioinput, audiooutput or videoinput");
  *out = {kind, parts[1], parts[2], parts[3]};
  return true;
}
bool Devices(JSContext* cx, JS::HandleValue value, std::vector<MediaDevice>* out) {
  bool array = false;
  if (!value.isObject() || !JS::IsArrayObject(cx, value, &array) || !array)
    return WebKit::Fail(cx, "Media devices must be an array");
  JS::RootedObject list(cx, &value.toObject());
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, list, &length)) return false;
  out->clear();
  for (uint32_t index = 0; index < length; ++index) {
    JS::RootedValue item(cx);
    MediaDevice device;
    if (!JS_GetElement(cx, list, index, &item) || !Device(cx, item, &device)) return false;
    out->push_back(std::move(device));
  }
  return true;
}
bool Decoding(JSContext* cx, JS::HandleValue value,
              std::map<std::string, Capability>* out) {
  bool array = false;
  if (!value.isObject() || !JS::IsArrayObject(cx, value, &array) || array)
    return WebKit::Fail(cx, "Media decoding must be an object");
  JS::RootedObject table(cx, &value.toObject());
  JS::Rooted<JS::IdVector> keys(cx, JS::IdVector(cx));
  if (!JS_Enumerate(cx, table, &keys)) return false;
  out->clear();
  for (size_t key = 0; key < keys.length(); ++key) {
    JS::RootedValue name(cx), entry(cx), field(cx);
    JS::RootedString text(cx);
    std::string content_type;
    Capability capability;
    if (!JS_IdToValue(cx, keys[key], &name) || !(text = JS::ToString(cx, name)) ||
        !ToUTF8(cx, text, &content_type) || !JS_GetPropertyById(cx, table, keys[key], &entry) ||
        !entry.isObject()) return false;
    JS::RootedObject record(cx, &entry.toObject());
    bool* fields[] = {&capability.supported, &capability.smooth, &capability.power_efficient};
    const char* names[] = {"supported", "smooth", "powerEfficient"};
    for (size_t index = 0; index < 3; ++index) {
      if (!JS_GetProperty(cx, record, names[index], &field) || !field.isBoolean())
        return WebKit::Fail(cx, "Media decoding answer must hold three booleans");
      *fields[index] = field.toBoolean();
    }
    out->emplace(std::move(content_type), capability);
  }
  return true;
}
}  // namespace

// The decoding answer for one page contentType: the configured entry, else the
// imported domain table (configuration.h Evaluate) through the caller.
bool ConfiguredDecoding(State& state, const std::string& content_type, Capability* out) {
  const auto entry = state.decoding.find(content_type);
  if (entry == state.decoding.end()) return false;
  *out = entry->second;
  return true;
}

bool Realm::Configure(JS::HandleObject group) {
  State& s = *state_;
  JSContext* cx = s.cx;
  JS::RootedValue devices(cx), decoding(cx);
  if (!JS_GetProperty(cx, group, "devices", &devices) ||
      !JS_GetProperty(cx, group, "decoding", &decoding)) return false;
  if (!devices.isUndefined() && !Devices(cx, devices, &s.devices)) return false;
  if (!decoding.isUndefined() && !Decoding(cx, decoding, &s.decoding)) return false;
  return true;
}

bool Realm::Describe(JS::MutableHandleValue result) {
  State& s = *state_;
  JSContext* cx = s.cx;
  JS::RootedObject group(cx, JS_NewPlainObject(cx));
  JS::RootedObject list(cx, JS::NewArrayObject(cx, s.devices.size()));
  if (!group || !list) return false;
  for (size_t index = 0; index < s.devices.size(); ++index) {
    const auto& device = s.devices[index];
    const std::string text = device.kind + "|" + device.label + "|" +
                             device.device_id + "|" + device.group_id;
    JS::RootedString entry(cx, JS_NewStringCopyUTF8N(cx,
        JS::UTF8Chars(text.data(), text.size())));
    if (!entry || !JS_DefineElement(cx, list, index, entry, JSPROP_ENUMERATE))
      return false;
  }
  JS::RootedObject table(cx, JS_NewPlainObject(cx));
  if (!table) return false;
  for (const auto& [content_type, capability] : s.decoding) {
    JS::RootedObject record(cx, JS_NewPlainObject(cx));
    JS::RootedValue supported(cx, JS::BooleanValue(capability.supported));
    JS::RootedValue smooth(cx, JS::BooleanValue(capability.smooth));
    JS::RootedValue power(cx, JS::BooleanValue(capability.power_efficient));
    if (!record ||
        !JS_DefineProperty(cx, record, "supported", supported, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, record, "smooth", smooth, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, record, "powerEfficient", power, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, table, content_type.c_str(), record, JSPROP_ENUMERATE))
      return false;
  }
  if (!JS_DefineProperty(cx, group, "devices", list, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, group, "decoding", table, JSPROP_ENUMERATE)) return false;
  result.setObject(*group);
  return true;
}
}  // namespace ruyijs::web_media
