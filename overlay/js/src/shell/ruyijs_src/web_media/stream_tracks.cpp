// A MediaStream's track set: one array per stream in its StreamTracks slot,
// insertion-ordered and without duplicates (Firefox DOMMediaStream::mTracks).
// Readers return fresh arrays, so the set itself is never exposed.
#include "web_media/stream.h"
#include "web_media/track.h"
#include "jsapi.h"
#include "js/Array.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::web_media {
namespace {
JSObject* List(JSObject* stream) { return &JS::GetReservedSlot(stream, StreamTracks).toObject(); }
// Index of `track` (any wrapper of it) in the set, or -1.
bool Find(JSContext* cx, JS::HandleObject list, JS::HandleObject track, int32_t* index) {
  uint32_t length = 0;
  JS::RootedValue item(cx);
  JSObject* target = js::CheckedUnwrapStatic(track);
  *index = -1;
  if (!JS::GetArrayLength(cx, list, &length)) return false;
  for (uint32_t i = 0; i < length; ++i) {
    if (!JS_GetElement(cx, list, i, &item)) return false;
    if (js::CheckedUnwrapStatic(&item.toObject()) == target) { *index = int32_t(i); return true; }
  }
  return true;
}
}
bool AddStreamTrack(JSContext* cx, JS::HandleObject stream, JS::HandleObject track) {
  JS::RootedObject list(cx, List(stream));
  JSAutoRealm realm(cx, list);
  JS::RootedObject value(cx, track);
  int32_t index = -1;
  uint32_t length = 0;
  if (!JS_WrapObject(cx, &value) || !Find(cx, list, value, &index)) return false;
  if (index >= 0) return true;
  return JS::GetArrayLength(cx, list, &length) && JS_DefineElement(cx, list, length, value, JSPROP_ENUMERATE);
}
bool RemoveStreamTrack(JSContext* cx, JS::HandleObject stream, JS::HandleObject track) {
  JS::RootedObject list(cx, List(stream));
  JSAutoRealm realm(cx, list);
  JS::RootedObject value(cx, track);
  int32_t index = -1;
  uint32_t length = 0;
  if (!JS_WrapObject(cx, &value) || !Find(cx, list, value, &index)) return false;
  if (index < 0) return true;
  JS::RootedValue item(cx);
  if (!JS::GetArrayLength(cx, list, &length)) return false;
  for (uint32_t i = uint32_t(index); i + 1 < length; ++i)
    if (!JS_GetElement(cx, list, i + 1, &item) || !JS_SetElement(cx, list, i, item)) return false;
  return JS::SetArrayLength(cx, list, length - 1);
}
bool StreamTrackArray(JSContext* cx, JS::HandleObject stream, int kind, JS::MutableHandleObject out) {
  JS::RootedObject list(cx, List(stream));
  JS::RootedObject result(cx, JS::NewArrayObject(cx, 0));
  if (!result) return false;
  uint32_t length = 0, count = 0;
  JS::RootedValue item(cx);
  {
    JSAutoRealm realm(cx, list);
    if (!JS::GetArrayLength(cx, list, &length)) return false;
  }
  for (uint32_t i = 0; i < length; ++i) {
    {
      JSAutoRealm realm(cx, list);
      if (!JS_GetElement(cx, list, i, &item)) return false;
    }
    if (!JS_WrapValue(cx, &item)) return false;
    const bool video = TrackIsVideo(js::CheckedUnwrapStatic(&item.toObject()));
    if (kind == 1 && video) continue;
    if (kind == 2 && !video) continue;
    if (!JS_DefineElement(cx, result, count++, item, JSPROP_ENUMERATE)) return false;
  }
  out.set(result);
  return true;
}
bool StreamTrackById(JSContext* cx, JS::HandleObject stream, JS::HandleString id, JS::MutableHandleValue out) {
  JS::RootedObject tracks(cx);
  if (!StreamTrackArray(cx, stream, 0, &tracks)) return false;
  uint32_t length = 0;
  JS::RootedValue item(cx);
  out.setNull();
  if (!JS::GetArrayLength(cx, tracks, &length)) return false;
  for (uint32_t i = 0; i < length; ++i) {
    if (!JS_GetElement(cx, tracks, i, &item)) return false;
    // The id string belongs to the track's zone; compare a copy in ours.
    JS::RootedValue own(cx, JS::GetReservedSlot(js::CheckedUnwrapStatic(&item.toObject()), MSTrackId));
    int32_t order = 0;
    if (!JS_WrapValue(cx, &own)) return false;
    JS::RootedString text(cx, own.toString());
    if (!JS_CompareStrings(cx, text, id, &order)) return false;
    if (order == 0) { out.set(item); return true; }
  }
  return true;
}
bool StreamActive(JSContext* cx, JS::HandleObject stream, bool* active) {
  JS::RootedObject tracks(cx);
  if (!StreamTrackArray(cx, stream, 0, &tracks)) return false;
  uint32_t length = 0;
  JS::RootedValue item(cx);
  *active = false;
  if (!JS::GetArrayLength(cx, tracks, &length)) return false;
  for (uint32_t i = 0; i < length && !*active; ++i) {
    if (!JS_GetElement(cx, tracks, i, &item)) return false;
    *active = TrackIsLive(js::CheckedUnwrapStatic(&item.toObject()));
  }
  return true;
}
}  // namespace ruyijs::web_media
