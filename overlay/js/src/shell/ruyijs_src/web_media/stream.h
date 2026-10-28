#ifndef RUYIJS_WEB_MEDIA_STREAM_H
#define RUYIJS_WEB_MEDIA_STREAM_H
#include "web_media/state.h"
namespace ruyijs::web_media {
enum StreamSlot { StreamId, StreamAddHandler, StreamRemoveHandler, StreamOwner,
                  StreamAddListener, StreamRemoveListener,
                  StreamCanvas,  // CanvasCaptureMediaStream: its source canvas
                  StreamTracks,  // the ordered track set: an array in the stream's realm
                  StreamSlotCount };
enum StreamMethodOp { AddTrack, CloneStream, AudioTracks, TrackById, AllTracks, VideoTracks, RemoveTrack };
extern const JSClass kStreamClass, kTrackClass;
bool InstallStreams(State&);
bool InstallCanvasCapture(State&);
bool StreamConstructor(JSContext*, unsigned, JS::Value*);
bool StreamGet(JSContext*, unsigned, JS::Value*);
bool StreamSet(JSContext*, unsigned, JS::Value*);
bool StreamMethod(JSContext*, unsigned, JS::Value*);
// Brand checks. TrackReceiver reports "<where> does not implement interface
// MediaStreamTrack." for an argument; StreamReceiver brands the call's this.
bool StreamReceiver(JSContext*, const JS::CallArgs&, JS::MutableHandleObject);
bool TrackReceiver(JSContext*, JS::HandleValue, JS::MutableHandleObject, const char* where);
// A MediaStream argument (MediaRecorder's overload), with its existing text.
bool StreamArgument(JSContext*, JS::HandleValue, JS::MutableHandleObject);
JSObject* NewStream(State&, JS::HandleObject);
// One EventHandler attribute of a stream or track (stream_handlers.cpp).
struct EventHandlerSlots { int handler, listener, owner; const char* type; };
bool SetEventHandler(State&, JS::HandleObject self, const EventHandlerSlots&, JS::HandleValue value);
// The track set (stream_tracks.cpp). `stream` is unwrapped; a track of any
// realm is stored as seen from the stream's realm, once.
bool AddStreamTrack(JSContext*, JS::HandleObject stream, JS::HandleObject track);
bool RemoveStreamTrack(JSContext*, JS::HandleObject stream, JS::HandleObject track);
// A new array in the caller's realm: every track (kind 0), audio (1) or video (2).
bool StreamTrackArray(JSContext*, JS::HandleObject stream, int kind, JS::MutableHandleObject out);
bool StreamTrackById(JSContext*, JS::HandleObject stream, JS::HandleString id, JS::MutableHandleValue out);
bool StreamActive(JSContext*, JS::HandleObject stream, bool* active);
}
#endif
