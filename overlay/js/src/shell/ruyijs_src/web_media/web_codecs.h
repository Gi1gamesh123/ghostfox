#ifndef RUYIJS_WEB_MEDIA_WEB_CODECS_H
#define RUYIJS_WEB_MEDIA_WEB_CODECS_H
#include "web_media/state.h"
#include <string>
namespace ruyijs::web_media {
// WebCodecs AudioDecoder / AudioEncoder / VideoDecoder / VideoEncoder and the
// EncodedAudioChunk / EncodedVideoChunk data objects. The codecs reuse this
// domain's existing Web Media owner and install path (Window Install and Worker
// InstallWorker), never a second browser-environment layer. Actual encode and
// decode are not implemented; the interfaces expose Firefox 158's surface and
// state-machine errors only.
enum CodecKind { AudioDecode = 0, VideoDecode = 1, AudioEncode = 2, VideoEncode = 3 };
enum CodecState { Unconfigured = 0, Configured = 1, Closed = 2 };
// Reserved slots of a codec instance: [kind, state, ondequeue].
enum CodecSlot { SlotKind = 0, SlotState = 1, SlotOndequeue = 2 };
enum CodecMember { CConfigure = 0, CProcess = 1, CFlush = 2, CReset = 3, CClose = 4,
                  CStateGet = 5, CQueueGet = 6, COndequeueGet = 7, COndequeueSet = 8 };
extern const JSClass kCodecClass, kChunkClass;
bool InstallWebCodecs(State&, bool worker);
bool CodecConstructor(JSContext*, unsigned, JS::Value*);
bool CodecMember(JSContext*, unsigned, JS::Value*);
bool CodecStateGetter(JSContext*, unsigned, JS::Value*);
bool CodecQueueGetter(JSContext*, unsigned, JS::Value*);
bool CodecOndequeueGet(JSContext*, unsigned, JS::Value*);
bool CodecOndequeueSet(JSContext*, unsigned, JS::Value*);
bool CodecIsConfigSupported(JSContext*, unsigned, JS::Value*);  // web_codecs_support.cpp
// A DOMException `name` with `message` in the owner's realm (rejections and throws).
bool CodecError(State&, const char* name, const std::string& message, JS::MutableHandleValue);
bool ChunkConstructor(JSContext*, unsigned, JS::Value*);
bool ChunkCopyTo(JSContext*, unsigned, JS::Value*);
bool ChunkGetter(JSContext*, unsigned, JS::Value*);
}
#endif
