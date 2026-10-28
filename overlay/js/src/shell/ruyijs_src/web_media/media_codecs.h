#ifndef RUYIJS_WEB_MEDIA_CODECS_H
#define RUYIJS_WEB_MEDIA_CODECS_H
#include "runtime/web_kit.h"
#include "web_media/text_track.h"
namespace ruyijs::web_media {
struct State;
extern const JSClass kMediaSourceClass, kSourceBufferClass, kSourceBufferListClass,
    kMediaRecorderClass, kCapabilitiesClass;
bool InstallMediaCodecs(State&);
// The realm-shared lookups (DOMException, Array.prototype.values) and the
// MediaCapabilities interface + navigator.mediaCapabilities, the parts of
// InstallMediaCodecs a Worker realm also exposes.
bool InstallMediaBase(State&);
bool InstallMediaCapabilities(State&);
bool MediaSourceConstructor(JSContext*, unsigned, JS::Value*);
bool MediaSourceMember(JSContext*, unsigned, JS::Value*);
bool MediaSourceSet(JSContext*, unsigned, JS::Value*);
bool MediaSourceStatic(JSContext*, unsigned, JS::Value*);
bool SourceBufferMember(JSContext*, unsigned, JS::Value*);
bool SourceBufferListSet(JSContext*, unsigned, JS::Value*);
bool SourceBufferListIterator(JSContext*, unsigned, JS::Value*);
bool SourceBufferListMember(JSContext*, unsigned, JS::Value*);
bool MediaRecorderConstructor(JSContext*, unsigned, JS::Value*);
bool MediaRecorderMember(JSContext*, unsigned, JS::Value*);
bool MediaRecorderSet(JSContext*, unsigned, JS::Value*);
bool MediaRecorderStatic(JSContext*, unsigned, JS::Value*);
bool CapabilitiesConstructor(JSContext*, unsigned, JS::Value*);
bool CapabilitiesMember(JSContext*, unsigned, JS::Value*);
bool NavigatorCapabilities(JSContext*, unsigned, JS::Value*);
}
#endif
