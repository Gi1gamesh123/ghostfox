// MediaStream members over the track set (stream_tracks.cpp). Firefox 158
// texts (artifacts/media-track/probe.js); addTrack/removeTrack fire no event.
#include "web_media/stream.h"
#include "web_media/track.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#include <string>
namespace ruyijs::web_media {
namespace {
constexpr const char* kMethodNames[] = {"addTrack", "clone", "getAudioTracks", "getTrackById",
                                        "getTracks", "getVideoTracks", "removeTrack"};
bool Missing(JSContext* cx, int op) {
  return WebKit::Fail(cx, (std::string("MediaStream.") + kMethodNames[op] +
                           ": At least 1 argument required, but only 0 passed").c_str());
}
// MediaStream.clone(): a plain MediaStream of the same realm with every track cloned.
bool Clone(JSContext* cx, JS::HandleObject self, JS::MutableHandleObject out) {
  auto& owner = *static_cast<State*>(JS::GetReservedSlot(self, StreamOwner).toPrivate());
  JS::RootedObject tracks(cx), result(cx), item(cx), copy(cx);
  if (!StreamTrackArray(cx, self, 0, &tracks)) return false;
  uint32_t length = 0;
  JS::RootedValue value(cx);
  if (!JS::GetArrayLength(cx, tracks, &length)) return false;
  {
    JSAutoRealm realm(cx, owner.kit.page);
    result = NewStream(owner, owner.stream.prototype);
    if (!result) return false;
  }
  for (uint32_t i = 0; i < length; ++i) {
    if (!JS_GetElement(cx, tracks, i, &value)) return false;
    item = js::CheckedUnwrapStatic(&value.toObject());
    auto& source = *static_cast<State*>(JS::GetReservedSlot(item, MSTrackOwner).toPrivate());
    {
      JSAutoRealm realm(cx, source.kit.page);
      copy = CloneTrack(source, item);
      if (!copy) return false;
    }
    if (!AddStreamTrack(cx, result, copy)) return false;
  }
  out.set(result);
  return JS_WrapObject(cx, out);
}
}
bool StreamGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!StreamReceiver(cx, args, &self)) return false;
  int slot = WebKit::Slot(args);
  if (slot < 0) {  // active: some track of the set is not ended
    bool active = false;
    if (!StreamActive(cx, self, &active)) return false;
    args.rval().setBoolean(active);
    return true;
  }
  JS::RootedValue result(cx, JS::GetReservedSlot(self, slot));
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}
bool StreamMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int op = WebKit::Slot(args);
  JS::RootedObject self(cx), result(cx);
  if (!StreamReceiver(cx, args, &self)) return false;
  if (op == TrackById) {
    if (!argc) return Missing(cx, op);
    JS::RootedString key(cx, JS::ToString(cx, args[0]));
    return key && StreamTrackById(cx, self, key, args.rval());
  }
  if (op == AddTrack || op == RemoveTrack) {
    if (!argc) return Missing(cx, op);
    JS::RootedObject track(cx);
    const std::string where = std::string("MediaStream.") + kMethodNames[op] + ": Argument 1";
    if (!TrackReceiver(cx, args[0], &track, where.c_str())) return false;
    JS::RootedObject value(cx, &args[0].toObject());
    if (!(op == AddTrack ? AddStreamTrack(cx, self, value) : RemoveStreamTrack(cx, self, value)))
      return false;
    args.rval().setUndefined();
    return true;
  }
  if (op == CloneStream) {
    if (!Clone(cx, self, &result)) return false;
  } else if (!StreamTrackArray(cx, self, op == AudioTracks ? 1 : op == VideoTracks ? 2 : 0, &result)) {
    return false;
  }
  args.rval().setObject(*result); return true;
}
}
