#include "web_media/stream.h"
#include "web_media/track.h"
#include "jsapi.h"
#include "js/PropertyAndElement.h"
namespace ruyijs::web_media {
const JSClass kStreamClass = {"MediaStream", JSCLASS_HAS_RESERVED_SLOTS(StreamSlotCount)};
namespace {
bool ConstructorLast(State& s, WebFace& face) {
  return JS_DeleteProperty(s.cx, face.prototype, "constructor") &&
         JS_DefineProperty(s.cx, face.prototype, "constructor", face.constructor, 0);
}
}
bool InstallStreams(State& s) {
  JS::RootedObject parent(s.cx, s.events.prototype());
  JS::RootedValue target(s.cx), event(s.cx), eventPrototype(s.cx);
  if (!JS_GetProperty(s.cx, parent, "constructor", &target) || !target.isObject() ||
      !JS_GetProperty(s.cx, s.kit.page, "Event", &event) || !event.isObject()) return false;
  JS::RootedObject eventObject(s.cx, &event.toObject()), targetObject(s.cx, &target.toObject());
  if (!JS_GetProperty(s.cx, eventObject, "prototype", &eventPrototype) || !eventPrototype.isObject()) return false;
  JS::RootedObject eventParent(s.cx, &eventPrototype.toObject());
  if (!JS_GetProperty(s.cx, eventParent, "preventDefault", &s.streamPreventDefault)) return false;
  if (!JS_GetProperty(s.cx, parent, "addEventListener", &s.addListener) ||
      !JS_GetProperty(s.cx, parent, "removeEventListener", &s.removeListener) ||
      !s.kit.Define("MediaStream", &kStreamClass, parent, StreamConstructor, &s.stream) ||
      !JS_SetPrototype(s.cx, s.stream.constructor, targetObject)) return false;
  const char* methods[] = {"addTrack", "clone", "getAudioTracks", "getTrackById",
                           "getTracks", "getVideoTracks", "removeTrack"};
  for (int op : {AudioTracks, VideoTracks, AllTracks, TrackById, AddTrack, RemoveTrack, CloneStream})
    if (!s.kit.Method(s.stream, "MediaStream", methods[op], StreamMethod,
                      op == AddTrack || op == TrackById || op == RemoveTrack ? 1 : 0, op)) return false;
  if (
      !s.kit.Accessor(s.stream, "MediaStream", "id", StreamGet, nullptr, StreamId) ||
      !s.kit.Accessor(s.stream, "MediaStream", "active", StreamGet, nullptr, -1) ||
      !s.kit.Accessor(s.stream, "MediaStream", "onaddtrack", StreamGet, StreamSet, StreamAddHandler) ||
      !s.kit.Accessor(s.stream, "MediaStream", "onremovetrack", StreamGet, StreamSet, StreamRemoveHandler))
    return false;
  return ConstructorLast(s, s.stream) && InstallTrack(s, targetObject);
}
}
