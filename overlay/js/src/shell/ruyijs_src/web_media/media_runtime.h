#ifndef RUYIJS_WEB_MEDIA_RUNTIME_H
#define RUYIJS_WEB_MEDIA_RUNTIME_H
#include "web_media/state.h"
namespace ruyijs::web_media {
enum SourceSlot { SourceBuffers, SourceActive, SourceReady, SourceDuration, SourceOpen, SourceEnded, SourceClose, SourceOwner, SourceSlotCount };
enum ListSlot { ListLength, ListAdd, ListRemove, ListOwner, ListSlotCount };
enum RecorderSlot { RecorderStream, RecorderMime, RecorderState, RecorderVideoRate, RecorderAudioRate, RecorderStart, RecorderStop, RecorderData, RecorderPause, RecorderResume, RecorderError, RecorderOwner, RecorderSlotCount };
bool MediaReceiver(JSContext*, JS::HandleValue, const JSClass*, JS::MutableHandleObject);
bool MediaGetSlot(JSContext*, JS::HandleObject, unsigned, JS::MutableHandleValue);
bool MediaHandlerSet(State&, JS::HandleObject, unsigned, const char*, JS::HandleValue);
bool MediaException(State&, const char*, const char*);
bool MediaInvalidState(State&);
bool RejectMediaException(JSContext*, JS::HandleObject, JS::MutableHandleValue);
bool MediaConstructorLast(State&, WebFace&);
// The realm's MediaStreamError (media_stream_error.cpp), the one factory for it.
bool NewMediaStreamError(State&, const char* name, const char* message, const char* constraint,
                         JS::MutableHandleObject out);
}
#endif
