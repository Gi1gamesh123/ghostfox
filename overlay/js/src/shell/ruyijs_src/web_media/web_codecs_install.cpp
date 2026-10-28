// The one production installer of the WebCodecs interfaces. It runs on both the
// Window path (Realm::Install) and the dedicated Worker path (Realm::InstallWorker),
// matching Firefox's [Exposed=(Window,DedicatedWorker)] surface. Codec prototypes
// extend the Interaction owner's one EventTarget prototype; chunks extend Object.
#include "web_media/web_codecs.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/PropertyAndElement.h"
#include "js/Symbol.h"
namespace ruyijs::web_media {
namespace {
bool Begin(State& s, WebFace& face) {
  JS::RootedId tag(s.cx, JS::PropertyKey::Symbol(
      JS::GetWellKnownSymbol(s.cx, JS::SymbolCode::toStringTag)));
  return JS_DeleteProperty(s.cx, face.prototype, "constructor") &&
         JS_DeletePropertyById(s.cx, face.prototype, tag);
}
bool Finish(State& s, WebFace& face, const char* name) {
  JS::RootedString text(s.cx, JS_NewStringCopyZ(s.cx, name));
  JS::RootedId tag(s.cx, JS::PropertyKey::Symbol(
      JS::GetWellKnownSymbol(s.cx, JS::SymbolCode::toStringTag)));
  return text && JS_DefineProperty(s.cx, face.prototype, "constructor", face.constructor, 0) &&
         JS_DefinePropertyById(s.cx, face.prototype, tag, text, JSPROP_READONLY);
}
bool InstallOne(State& s, CodecKind kind) {
  auto& k = s.kit;
  static const char* names[4] = {"AudioDecoder", "VideoDecoder", "AudioEncoder", "VideoEncoder"};
  static const char* process[4] = {"decode", "decode", "encode", "encode"};
  static const char* queue[4] = {"decodeQueueSize", "decodeQueueSize", "encodeQueueSize", "encodeQueueSize"};
  WebFace* faces[4] = {&s.audioDecoder, &s.videoDecoder, &s.audioEncoder, &s.videoEncoder};
  WebFace& face = *faces[kind];
  JS::RootedObject eventTarget(s.cx, s.events.prototype());
  if (!eventTarget || !k.Define(names[kind], &kCodecClass, eventTarget, CodecConstructor, &face, 1) ||
      !Begin(s, face)) return false;
  js::SetFunctionNativeReserved(face.constructor, 1, JS::Int32Value(int(kind)));
  if (!k.Method(face, names[kind], "configure", CodecMember, 1, kind * 16 + CConfigure) ||
      !k.Method(face, names[kind], process[kind], CodecMember, 1, kind * 16 + CProcess) ||
      !k.Method(face, names[kind], "flush", CodecMember, 0, kind * 16 + CFlush) ||
      !k.Method(face, names[kind], "reset", CodecMember, 0, kind * 16 + CReset) ||
      !k.Method(face, names[kind], "close", CodecMember, 0, kind * 16 + CClose) ||
      !k.Accessor(face, names[kind], "state", CodecStateGetter, nullptr, kind * 16 + CStateGet) ||
      !k.Accessor(face, names[kind], queue[kind], CodecQueueGetter, nullptr, kind * 16 + CQueueGet) ||
      !k.Accessor(face, names[kind], "ondequeue", CodecOndequeueGet, CodecOndequeueSet, kind * 16 + COndequeueGet))
    return false;
  if (!Finish(s, face, names[kind])) return false;
  JS::RootedObject supported(s.cx, k.Function(CodecIsConfigSupported, "isConfigSupported", 1, kind));
  return supported && JS_DefineProperty(s.cx, face.constructor, "isConfigSupported", supported, JSPROP_ENUMERATE);
}
bool InstallChunk(State& s, bool video) {
  auto& k = s.kit;
  const char* name = video ? "EncodedVideoChunk" : "EncodedAudioChunk";
  WebFace& face = video ? s.encodedVideoChunk : s.encodedAudioChunk;
  JS::RootedObject parent(s.cx, JS::GetRealmObjectPrototype(s.cx));
  if (!parent || !k.Define(name, &kChunkClass, parent, ChunkConstructor, &face, 1) ||
      !Begin(s, face) ||
      !k.Method(face, name, "copyTo", ChunkCopyTo, 1, video ? 1 : 0))
    return false;
  js::SetFunctionNativeReserved(face.constructor, 1, JS::Int32Value(video ? 1 : 0));
  static const char* fields[4] = {"type", "timestamp", "duration", "byteLength"};
  for (int i = 0; i < 4; ++i)
    if (!k.Accessor(face, name, fields[i], ChunkGetter, nullptr, i + (video ? 10 : 0))) return false;
  return Finish(s, face, name);
}
}
bool InstallWebCodecs(State& s, bool worker) {
  // The ondequeue event handler attribute swaps a listener on the shared
  // EventTarget prototype; Workers reach that path too, so the two methods are
  // captured here rather than only in the Window-only stream installer.
  JS::RootedObject target(s.cx, s.events.prototype());
  if (!target || !JS_GetProperty(s.cx, target, "addEventListener", &s.addListener) ||
      !JS_GetProperty(s.cx, target, "removeEventListener", &s.removeListener)) return false;
  for (CodecKind kind : {AudioDecode, VideoDecode, AudioEncode, VideoEncode})
    if (!InstallOne(s, kind)) return false;
  if (!InstallChunk(s, false) || !InstallChunk(s, true)) return false;
  // WebCodecs is [Exposed=(Window,DedicatedWorker)]; a ServiceWorker does not call this.
  return true;
}
}
