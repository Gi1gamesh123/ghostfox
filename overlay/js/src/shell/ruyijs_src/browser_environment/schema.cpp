#include "browser_environment/schema.h"
#include "runtime/runtime.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Array.h"
#include "js/PropertyAndElement.h"
#include "js/Conversions.h"
#include <cstring>
#include <string>
#include <iterator>

namespace ruyijs::browser_environment {
static const JSErrorFormatString* ErrorFormat(void* data, unsigned) {
  return static_cast<const JSErrorFormatString*>(data);
}
bool Error(JSContext* cx, const char* message, JSExnType type) {
  JSErrorFormatString format{"RUYIJS_ENVIRONMENT", "{0}", 1, int16_t(type)};
  JS_ReportErrorNumberASCII(cx, ErrorFormat, &format, 0, message);
  return false;
}
bool ReceiverError(JSContext* cx, const JS::CallArgs& args, const char* iface) {
  std::string member;
  JS::RootedString name(cx);
  JS::RootedFunction function(cx, JS_GetObjectFunction(&args.callee()));
  if (function && JS_GetFunctionDisplayId(cx, function, &name) && name &&
      !ToUTF8(cx, name, &member))
    return false;
  const std::string message = "'" + member + "' called on an object that does not implement interface " +
                              iface + ".";
  return Error(cx, message.c_str());
}
const OwnerGroupSpec* FindOwnerGroup(const std::string& name) {
  for (const auto& owner : kOwnerGroups)
    if (name == owner.name) return &owner;
  return nullptr;
}
static bool Object(JSContext* cx, JS::HandleValue value) {
  bool array = false;
  if (!value.isObject()) return Error(cx, "Environment group must be an object");
  return JS::IsArrayObject(cx, value, &array) &&
         (!array || Error(cx, "Environment group must not be an array"));
}
static bool Group(JSContext* cx, JS::HandleObject object, Kind kind,
                   const Field* fields, size_t count, bool complete,
                   std::vector<Change>* changes) {
  JS::Rooted<JS::IdVector> keys(cx, JS::IdVector(cx));
  if (!JS_Enumerate(cx, object, &keys)) return false;
  if (complete && keys.length() != count)
    return Error(cx, "Firefox baseline has an incomplete environment group");
  for (size_t key = 0; key < keys.length(); ++key) {
    JS::RootedValue name(cx), value(cx);
    JS::RootedId id(cx, keys[key]);
    if (!JS_IdToValue(cx, id, &name)) return false;
    JS::RootedString text(cx, JS::ToString(cx, name));
    std::string member;
    if (!text || !ToUTF8(cx, text, &member)) return false;
    size_t index = 0;
    while (index < count && member != fields[index].name) ++index;
    if (index == count) return Error(cx, "Unknown environment profile field");
    if (!JS_GetPropertyById(cx, object, id, &value)) return false;
    Change change{kind, uint32_t(index), {}};
    if (!ReadField(cx, value, kind, uint32_t(index), fields[index],
                   &change.value)) return false;
    changes->push_back(std::move(change));
  }
  return true;
}
bool Profile(JSContext* cx, JS::HandleObject object, bool complete,
             std::vector<Change>* changes) {
  JS::RootedValue root(cx, JS::ObjectValue(*object));
  if (!Object(cx, root)) return false;
  JS::Rooted<JS::IdVector> keys(cx, JS::IdVector(cx));
  if (!JS_Enumerate(cx, object, &keys)) return false;
  // The baseline must carry the five groups every realm has (the values its
  // Window, Navigator, Screen and Location read before any page script runs).
  // Any further group is allowed: the baseline completes what the group's owner
  // would otherwise default to. Field completeness is enforced per group by
  // Group(), so a listed group is either fully pinned or rejected.
  uint32_t seen = 0;
  for (size_t key = 0; key < keys.length(); ++key) {
    JS::RootedId id(cx, keys[key]);
    JS::RootedValue name(cx), value(cx);
    if (!JS_IdToValue(cx, id, &name)) return false;
    JS::RootedString text(cx, JS::ToString(cx, name));
    std::string group;
    if (!text || !ToUTF8(cx, text, &group)) return false;
    if (!JS_GetPropertyById(cx, object, id, &value) || !Object(cx, value)) return false;
    JS::RootedObject nested(cx, &value.toObject());
    if (group == "navigator") {
      seen |= 1;
      if (!Group(cx, nested, Kind::Navigator, kNavigatorFields,
                   std::size(kNavigatorFields), complete, changes)) return false;
    } else if (group == "screen") {
      seen |= 2;
      if (!Group(cx, nested, Kind::Screen, kScreenFields,
                   std::size(kScreenFields), complete, changes)) return false;
    } else if (group == "location") {
      seen |= 4;
      if (!Group(cx, nested, Kind::Location, kLocationFields, 1,
                   complete, changes)) return false;
    } else if (group == "locale") {
      seen |= 8;
      if (!Group(cx, nested, Kind::Locale, kLocaleFields, 1,
                   complete, changes)) return false;
    } else if (group == "viewport") {
      // The baseline pins the window's inner size so it differs from the
      // screen work area (Firefox: innerHeight < availHeight by the chrome
      // height) and the device pixel ratio its media queries derive from. A
      // user override uses the same validated group.
      seen |= 16;
      if (!Group(cx, nested, Kind::Viewport, kViewportFields,
                 std::size(kViewportFields), complete, changes)) return false;
    } else if (group == "webgl") {
      // The WebGL owner's data: the baseline may pin the GL strings, the WebGL2
      // version strings, the context attributes, the extension lists and the
      // per-pname limits, and the owner applies them through its port.
      if (!Group(cx, nested, Kind::WebGL, kWebGLFields,
                 std::size(kWebGLFields), complete, changes)) return false;
    } else if (group == "webgpu") {
      // The WebGPU owner's data: the stub adapter is opt-in, so an unlisted
      // group leaves it disabled; a listed one is validated by the owner.
      if (!Group(cx, nested, Kind::WebGpu, kWebGpuFields,
                 std::size(kWebGpuFields), complete, changes)) return false;
    } else if (group == "geolocation") {
      // The Geolocation owner's fixed position: without it a granted request
      // reports POSITION_UNAVAILABLE, which is Firefox's own answer when no
      // position source exists.
      if (!Group(cx, nested, Kind::Geolocation, kGeolocationFields,
                 std::size(kGeolocationFields), complete, changes)) return false;
    } else if (group == "speech") {
      // The Speech Synthesis owner's voice list; without it the owner has no
      // voices, so the baseline pins the user's real SAPI set.
      if (!Group(cx, nested, Kind::Speech, kSpeechFields,
                 std::size(kSpeechFields), complete, changes)) return false;
    } else if (group == "media") {
      // The Web Media owner's data: the enumerateDevices() list and the
      // MediaCapabilities decoding table, applied through its port.
      if (!Group(cx, nested, Kind::Media, kMediaFields,
                 std::size(kMediaFields), complete, changes)) return false;
    } else if (group == "storage") {
      // The StorageManager owner's estimate and persistence answer; the
      // baseline pins the trace machine's quota, usage and persisted flag.
      if (!Group(cx, nested, Kind::Storage, kStorageFields,
                 std::size(kStorageFields), complete, changes)) return false;
    } else if (const OwnerGroupSpec* owner = FindOwnerGroup(group)) {
      // A configure-only owner group (canvas, audio, mediaFeatures, rtc, fonts):
      // absent, the owner keeps the trace machine's values it already reports.
      if (!Group(cx, nested, owner->kind, owner->fields, owner->count, complete, changes))
        return false;
    } else if (group == "permissions") {
      // The Permissions owner's default states.
      if (!Group(cx, nested, Kind::Permissions, kPermissionsFields,
                 std::size(kPermissionsFields), complete, changes)) return false;
    } else return Error(cx, "Unknown environment profile group");
  }
  if (complete && seen != 31)
    return Error(cx, "Firefox baseline needs navigator, screen, viewport, location and locale");
  return true;
}
}
