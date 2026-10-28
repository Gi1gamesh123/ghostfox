#include "web_media/text_track.h"
#include "web_media/media_runtime.h"
#include "web_media/state.h"
#include "jsapi.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Symbol.h"

namespace ruyijs::web_media {
namespace {
bool Iterator(State& state, JS::HandleObject prototype, JSNative native,
              const char* iface, const char* name) {
  JS::RootedId id(state.cx, JS::PropertyKey::Symbol(
      JS::GetWellKnownSymbol(state.cx, JS::SymbolCode::iterator)));
  JS::RootedObject function(state.cx, state.kit.Function(native, name, 0, 0));
  return function && JS_DefinePropertyById(state.cx, prototype, id, function, 0);
}
bool Track(State& state, JS::HandleObject parent) {
  if (!state.kit.Define("TextTrack", &kTextTrackClass, parent,
                        TextTrackConstructor, &state.textTrack)) return false;
  const char* fields[] = {"kind", "label", "language", "id", "cues", "activeCues",
                          "inBandMetadataTrackDispatchType", "mode", "oncuechange"};
  for (int i = 0; i < 9; ++i)
    if (!state.kit.Accessor(state.textTrack, "TextTrack", fields[i], TextTrackMember,
                            i == TrackMode || i == TrackCueChange ? TextTrackSet : nullptr, i)) return false;
  if (!state.kit.Method(state.textTrack, "TextTrack", "addCue", TextTrackMember, 1, 100) ||
      !state.kit.Method(state.textTrack, "TextTrack", "removeCue", TextTrackMember, 1, 101)) return false;
  return MediaConstructorLast(state, state.textTrack);
}
bool Cue(State& state, JS::HandleObject parent) {
  if (!state.kit.Define("TextTrackCue", &kTextTrackCueClass, parent,
                        TextCueConstructor, &state.textCue)) return false;
  const char* fields[] = {"track", "id", "startTime", "endTime", "pauseOnExit", "onenter", "onexit"};
  for (int i = 0; i < 7; ++i)
    if (!state.kit.Accessor(state.textCue, "TextTrackCue", fields[i], TextCueMember,
                            i >= CueId ? TextCueSet : nullptr, i)) return false;
  return MediaConstructorLast(state, state.textCue);
}
bool CueList(State& state, JS::HandleObject parent) {
  if (!state.kit.Define("TextTrackCueList", &kTextTrackCueListClass, parent,
                        TextCueConstructor, &state.cueList) ||
      !state.kit.Method(state.cueList, "TextTrackCueList", "getCueById", CueListMember, 1, 101) ||
      !state.kit.Accessor(state.cueList, "TextTrackCueList", "length", CueListMember,
                           nullptr, 90) ||
      !state.kit.Method(state.cueList, "TextTrackCueList", "item", CueListMember, 1, 100) ||
      !Iterator(state, state.cueList.prototype, TextCueListIterator, "TextTrackCueList", "values")) return false;
  return MediaConstructorLast(state, state.cueList);
}
bool TrackList(State& state, JS::HandleObject parent) {
  if (!state.kit.Define("TextTrackList", &kTextTrackListClass, parent,
                        TextCueConstructor, &state.trackList) ||
      !state.kit.Method(state.trackList, "TextTrackList", "getTrackById", TrackListMember, 1, 100) ||
      !state.kit.Accessor(state.trackList, "TextTrackList", "length", TrackListMember,
                           nullptr, 90)) return false;
  const char* events[] = {"onchange", "onaddtrack", "onremovetrack"};
  for (int i = 0; i < 3; ++i)
    if (!state.kit.Accessor(state.trackList, "TextTrackList", events[i], TrackListMember,
                            TrackListSet, TrackListChange + i)) return false;
  if (!Iterator(state, state.trackList.prototype, TextTrackListIterator, "TextTrackList", "values")) return false;
  return MediaConstructorLast(state, state.trackList);
}
bool Vtt(State& state) {
  if (!state.kit.Define("VTTCue", &kVTTCueClass, state.textCue.prototype,
                        VTTCueConstructor, &state.vttCue)) return false;
  JS::RootedValue length(state.cx, JS::Int32Value(3));
  if (!JS_DefineProperty(state.cx, state.vttCue.constructor, "length", length, JSPROP_READONLY)) return false;
  const char* fields[] = {"region", "vertical", "snapToLines", "line", "lineAlign",
                          "position", "positionAlign", "size", "align", "text"};
  for (int i = 0; i < 10; ++i)
    if (!state.kit.Accessor(state.vttCue, "VTTCue", fields[i], VTTCueMember,
                            VTTCueSet, VttRegion + i)) return false;
  if (!state.kit.Method(state.vttCue, "VTTCue", "getCueAsHTML", VTTCueMember, 0, 100)) return false;
  return MediaConstructorLast(state, state.vttCue);
}
}

bool InstallTextTracks(State& state) {
  JS::RootedObject events(state.cx, state.events.prototype());
  JS::RootedObject object(state.cx, JS::GetRealmObjectPrototype(state.cx));
  return Track(state, events) && Cue(state, events) && CueList(state, object) &&
      TrackList(state, events) && Vtt(state) && InstallVTTRegion(state);
}
}
