#ifndef RUYIJS_WEB_MEDIA_TRACK_H
#define RUYIJS_WEB_MEDIA_TRACK_H
#include "web_media/state.h"
namespace ruyijs::web_media {
// MediaStreamTrack instances (track.cpp factory, track_members.cpp members).
// Every observable value is a reserved slot of the track; the origin decides
// getSettings(). No other file creates a track object.
enum TrackSlot { MSTrackOwner, MSTrackVideo, MSTrackKind, MSTrackId, MSTrackLabel, MSTrackEnabled, MSTrackMuted, MSTrackEnded,
                 MSTrackOrigin, MSTrackCanvas, MSTrackOnMute, MSTrackOnUnmute, MSTrackOnEnded,
                 MSTrackMuteListener, MSTrackUnmuteListener, MSTrackEndedListener, MSTrackSlotCount };
// Where a track's media comes from: an RTCRtpReceiver (remote) or a canvas
// element (HTMLCanvasElement.captureStream()).
enum class Origin : int32_t { Remote = 0, Canvas = 1 };
struct TrackSource {
  bool video = false;
  Origin origin = Origin::Remote;
  const char* label = "";
  bool muted = false;
  JSObject* canvas = nullptr;  // Origin::Canvas only
};
JSObject* NewTrack(State&, const TrackSource&);
// A new track with a new id over the same source, as MediaStreamTrack.clone().
JSObject* CloneTrack(State&, JS::HandleObject track);
// `object` is already unwrapped.
bool IsTrack(JSObject* object);
bool TrackIsVideo(JSObject* track);
bool TrackIsLive(JSObject* track);
bool InstallTrack(State&, JS::HandleObject eventTargetConstructor);
// A v4 UUID in braces, from the OS random source ({xxxxxxxx-...}, 38 chars).
JSString* NewMediaId(State&);
bool TrackConstructor(JSContext*, unsigned, JS::Value*);  // Illegal constructor
}
#endif
