#include "web_media/legacy_values.h"
#include "web_media/media_runtime.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"

namespace ruyijs::web_media {
// The four MediaDeviceInfo strings are the object data: deviceId, kind, label
// and groupId in the accessor slots kMediaDeviceInfoFields registers.
const JSClass kMediaDeviceInfoClass = {
    "MediaDeviceInfo", JSCLASS_HAS_RESERVED_SLOTS(4)};
namespace {
const JSClass kMediaError = {"MediaError", 0};
const JSClass kTimeRanges = {"TimeRanges", 0};
const JSClass kPlaybackQuality = {"VideoPlaybackQuality", 0};

bool Illegal(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  return trace.Finish(WebKit::Fail(cx, "Illegal constructor."));
}

// A MediaDeviceInfo answers from its own four string slots; the other media
// values carry no script-visible backing data.
bool Getter(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int slot = WebKit::Slot(args);
  const JSClass* clasp = slot < 4 ? &kMediaDeviceInfoClass :
      slot < 6 ? &kMediaError : slot == 6 ? &kTimeRanges : &kPlaybackQuality;
  JS::RootedObject object(cx);
  if (!MediaReceiver(cx, args.thisv(), clasp, &object)) return false;
  if (clasp == &kMediaDeviceInfoClass) {
    args.rval().set(JS::GetReservedSlot(object, slot));
    return JS_WrapValue(cx, args.rval());
  }
  return WebKit::Fail(cx, "Media value has no script-visible backing data");
}

bool Method(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int slot = WebKit::Slot(args);
  const JSClass* clasp = slot == 0 ? &kMediaDeviceInfoClass : &kTimeRanges;
  JS::RootedObject object(cx);
  if (!MediaReceiver(cx, args.thisv(), clasp, &object)) return false;
  return WebKit::Fail(cx, "Media value has no script-visible backing data");
}

bool DefineValue(State& s, const char* name, const JSClass* clasp,
                 WebFace& face, const char* const* getters, size_t count,
                 int firstSlot) {
  JS::RootedObject parent(s.cx, JS::GetRealmObjectPrototype(s.cx));
  if (!s.kit.Define(name, clasp, parent, Illegal, &face)) return false;
  for (size_t i = 0; i < count; ++i)
    if (!s.kit.Accessor(face, name, getters[i], Getter, nullptr,
                        firstSlot + static_cast<int32_t>(i))) return false;
  return true;
}
}

bool InstallLegacyValues(State& s) {
  const char* devices[] = {"deviceId", "kind", "label", "groupId"};
  const char* error[] = {"code", "message"};
  const char* ranges[] = {"length"};
  const char* quality[] = {"creationTime", "totalVideoFrames", "droppedVideoFrames"};
  WebFace& device = s.deviceInfo;
  WebFace mediaError(s.cx), timeRanges(s.cx), playback(s.cx);
  if (!DefineValue(s, "MediaDeviceInfo", &kMediaDeviceInfoClass, device, devices, 4, 0) ||
      !s.kit.Method(device, "MediaDeviceInfo", "toJSON", Method, 0, 0) ||
      !MediaConstructorLast(s, device) ||
      !DefineValue(s, "MediaError", &kMediaError, mediaError, error, 2, 4) ||
      !DefineValue(s, "TimeRanges", &kTimeRanges, timeRanges, ranges, 1, 6) ||
      !s.kit.Method(timeRanges, "TimeRanges", "start", Method, 1, 2) ||
      !s.kit.Method(timeRanges, "TimeRanges", "end", Method, 1, 1) ||
      !MediaConstructorLast(s, timeRanges) ||
      !DefineValue(s, "VideoPlaybackQuality", &kPlaybackQuality, playback, quality, 3, 9))
    return false;
  if (!MediaConstructorLast(s, playback)) return false;
  const struct Constant { WebFace* face; const char* name; int value; } constants[] = {
      {&mediaError, "MEDIA_ERR_ABORTED", 1}, {&mediaError, "MEDIA_ERR_NETWORK", 2},
      {&mediaError, "MEDIA_ERR_DECODE", 3}, {&mediaError, "MEDIA_ERR_SRC_NOT_SUPPORTED", 4}};
  for (const auto& item : constants) {
    JS::RootedValue value(s.cx, JS::Int32Value(item.value));
    const unsigned flags = JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT;
    if (!JS_DefineProperty(s.cx, item.face->prototype, item.name, value, flags) ||
        !JS_DefineProperty(s.cx, item.face->constructor, item.name, value, flags)) return false;
  }
  return MediaConstructorLast(s, mediaError);
}

// The one MediaDeviceInfo factory: an enumerated device is a plain value
// object over the four accessor slots installed above.
bool NewMediaDeviceInfo(State& s, const char* deviceId, const char* kind,
                        const char* label, const char* groupId,
                        JS::MutableHandleObject out) {
  JSContext* cx = s.cx;
  const char* values[] = {deviceId, kind, label, groupId};
  JS::RootedObject object(cx, s.kit.New(&kMediaDeviceInfoClass, s.deviceInfo.prototype));
  if (!object) return false;
  for (int32_t slot = 0; slot < 4; ++slot) {
    JSString* text = JS_NewStringCopyZ(cx, values[slot] ? values[slot] : "");
    if (!text) return false;
    JS::SetReservedSlot(object, slot, JS::StringValue(text));
  }
  out.set(object);
  return true;
}
}
