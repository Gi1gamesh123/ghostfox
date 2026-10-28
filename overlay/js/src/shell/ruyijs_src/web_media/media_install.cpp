#include "web_media/media_codecs.h"
#include "web_media/legacy_values.h"
#include "web_media/media_session.h"
#include "web_media/media_keys.h"
#include "web_media/media_runtime.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/PropertyAndElement.h"
#include "js/Symbol.h"
namespace ruyijs::web_media {
namespace {
bool Static(State& state, WebFace& face, JSNative native) {
  JS::RootedObject method(state.cx, state.kit.Function(native, "isTypeSupported", 1, 0));
  return method && JS_DefineProperty(state.cx, face.constructor, "isTypeSupported", method, JSPROP_ENUMERATE) &&
      state.kit.helpers->Member(method, face.clasp->name, "isTypeSupported", "call");
}
bool Source(State& state, JS::HandleObject parent) {
  if (!state.kit.Define("MediaSource", &kMediaSourceClass, parent, MediaSourceConstructor, &state.mediaSource)) return false;
  const char* methods[] = {"addSourceBuffer", "removeSourceBuffer", "endOfStream", "setLiveSeekableRange", "clearLiveSeekableRange"};
  const unsigned lengths[] = {1, 1, 0, 2, 0};
  for (int i = 0; i < 5; ++i)
    if (!state.kit.Method(state.mediaSource, "MediaSource", methods[i], MediaSourceMember, lengths[i], i + 7)) return false;
  const char* fields[] = {"sourceBuffers", "activeSourceBuffers", "readyState", "duration", "onsourceopen", "onsourceended", "onsourceclose"};
  for (int i = 0; i < 7; ++i)
    if (!state.kit.Accessor(state.mediaSource, "MediaSource", fields[i], MediaSourceMember, i >= 3 ? MediaSourceSet : nullptr, i)) return false;
  return Static(state, state.mediaSource, MediaSourceStatic) && MediaConstructorLast(state, state.mediaSource);
}
bool Buffers(State& state, JS::HandleObject parent) {
  if (!state.kit.Define("SourceBuffer", &kSourceBufferClass, parent, CapabilitiesConstructor, &state.sourceBuffer)) return false;
  const char* methods[] = {"appendBuffer", "abort", "remove", "changeType"};
  const unsigned lengths[] = {1, 0, 2, 1};
  for (int i = 0; i < 4; ++i)
    if (!state.kit.Method(state.sourceBuffer, "SourceBuffer", methods[i], SourceBufferMember, lengths[i], i)) return false;
  const char* fields[] = {"mode", "updating", "buffered", "timestampOffset", "appendWindowStart", "appendWindowEnd", "onupdatestart", "onupdate", "onupdateend", "onerror", "onabort"};
  for (int i = 0; i < 11; ++i)
    if (!state.kit.Accessor(state.sourceBuffer, "SourceBuffer", fields[i], SourceBufferMember, i == 1 || i == 2 ? nullptr : SourceBufferMember, i)) return false;
  if (!MediaConstructorLast(state, state.sourceBuffer) || !state.kit.Define("SourceBufferList", &kSourceBufferListClass, parent, CapabilitiesConstructor, &state.sourceBufferList)) return false;
  const char* list[] = {"length", "onaddsourcebuffer", "onremovesourcebuffer"};
  for (int i = 0; i < 3; ++i)
    if (!state.kit.Accessor(state.sourceBufferList, "SourceBufferList", list[i], SourceBufferListMember, i ? SourceBufferListSet : nullptr, i)) return false;
  JS::RootedId id(state.cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(state.cx, JS::SymbolCode::iterator)));
  JS::RootedObject iterator(state.cx, state.kit.Function(SourceBufferListIterator, "values", 0, 0));
  JS::RootedId tag(state.cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(state.cx, JS::SymbolCode::toStringTag)));
  JS::RootedValue label(state.cx);
  if (!JS_GetPropertyById(state.cx, state.sourceBufferList.prototype, tag, &label) ||
      !JS_DeletePropertyById(state.cx, state.sourceBufferList.prototype, tag)) return false;
  return MediaConstructorLast(state, state.sourceBufferList) && iterator &&
      JS_DefinePropertyById(state.cx, state.sourceBufferList.prototype, id, iterator, 0) &&
      JS_DefinePropertyById(state.cx, state.sourceBufferList.prototype, tag, label, JSPROP_READONLY);
}
bool Recorder(State& state, JS::HandleObject parent) {
  if (!state.kit.Define("MediaRecorder", &kMediaRecorderClass, parent, MediaRecorderConstructor, &state.mediaRecorder)) return false;
  JS::RootedValue length(state.cx, JS::Int32Value(1));
  if (!JS_DefineProperty(state.cx, state.mediaRecorder.constructor, "length", length, JSPROP_READONLY)) return false;
  const char* methods[] = {"start", "stop", "pause", "resume", "requestData"};
  for (int i = 0; i < 5; ++i)
    if (!state.kit.Method(state.mediaRecorder, "MediaRecorder", methods[i], MediaRecorderMember, 0, 100 + i)) return false;
  const char* fields[] = {"stream", "mimeType", "state", "onstart", "onstop", "ondataavailable", "onpause", "onresume", "onerror", "videoBitsPerSecond", "audioBitsPerSecond"};
  const int slots[] = {RecorderStream, RecorderMime, RecorderState, RecorderStart, RecorderStop, RecorderData, RecorderPause, RecorderResume, RecorderError, RecorderVideoRate, RecorderAudioRate};
  for (int i = 0; i < 11; ++i)
    if (!state.kit.Accessor(state.mediaRecorder, "MediaRecorder", fields[i], MediaRecorderMember, i >= 3 && i <= 8 ? MediaRecorderSet : nullptr, slots[i])) return false;
  return Static(state, state.mediaRecorder, MediaRecorderStatic) && MediaConstructorLast(state, state.mediaRecorder);
}
}
bool InstallMediaBase(State& state) {
  JS::RootedObject array(state.cx);
  return JS_GetProperty(state.cx, state.kit.page, "DOMException", &state.exceptionConstructor) &&
         JS_GetClassPrototype(state.cx, JSProto_Array, &array) &&
         JS_GetProperty(state.cx, array, "values", &state.arrayValues);
}
bool InstallMediaCodecs(State& state) {
  JS::RootedObject parent(state.cx, state.events.prototype());
  return InstallMediaBase(state) && InstallLegacyValues(state) && InstallTextTracks(state) &&
         InstallMediaSession(state) && InstallMediaKeys(state) && Source(state, parent) &&
         Buffers(state, parent) && Recorder(state, parent) && InstallMediaCapabilities(state);
}
bool InstallMediaCapabilities(State& state) {
  JS::RootedObject objectProto(state.cx, JS::GetRealmObjectPrototype(state.cx));
  if (!state.kit.Define("MediaCapabilities", &kCapabilitiesClass, objectProto, CapabilitiesConstructor, &state.capabilities) ||
      !state.kit.Method(state.capabilities, "MediaCapabilities", "decodingInfo", CapabilitiesMember, 1, 0) ||
      !state.kit.Method(state.capabilities, "MediaCapabilities", "encodingInfo", CapabilitiesMember, 1, 1) ||
      !MediaConstructorLast(state, state.capabilities)) return false;
  state.capabilitiesObject = state.kit.New(&kCapabilitiesClass, state.capabilities.prototype);
  if (!state.capabilitiesObject) return false;
  JS::SetReservedSlot(state.capabilitiesObject, 0, JS::PrivateValue(&state));
  state.environment.SetNavigatorMediaCapabilities(state.capabilitiesObject);
  JS::RootedObject proto(state.cx, state.environment.navigatorPrototype());
  JS::RootedObject getter(state.cx, state.kit.Function(NavigatorCapabilities, "get mediaCapabilities", 0, 0)), setter(state.cx);
  return getter && JS_DefineProperty(state.cx, proto, "mediaCapabilities", getter, setter, JSPROP_ENUMERATE) &&
      state.kit.helpers->Accessor("Navigator", proto, "mediaCapabilities");
}
}
