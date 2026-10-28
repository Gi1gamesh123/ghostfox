#include "web_audio/node_fields.h"

#include "interaction/realm.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "web_audio/tables.h"

using ruyijs::web_audio::tables::kCompressorParams;
using ruyijs::web_audio::tables::kContextAccessors;
using ruyijs::web_audio::tables::kFactories;
using ruyijs::web_audio::tables::kNodeAttributes;
using ruyijs::web_audio::tables::kParamMethods;
using ruyijs::web_audio::tables::MethodDef;

namespace ruyijs::web_audio {
namespace {

// Registers an accessor on a prototype that the Face loop above already built.
// The prototype is a handle: the registration allocates, so a raw JSObject*
// read out of a rooted vector or face could be moved by a GC mid-call.
bool AccessorOn(WebKit& kit, JS::HandleObject prototype, const char* iface, const char* name,
                JSNative getter, JSNative setter, int32_t slot) {
  WebFace face(kit.cx);
  face.prototype.set(prototype);
  return kit.Accessor(face, iface, name, getter, setter, slot);
}

}  // namespace

bool Realm::Install() {
  State& s = *state_;
  // The constructor, prototypes, parameter objects, buffers and promises all
  // belong to the installing realm, so an iframe gets realm-local interfaces.
  const JSAutoRealm realm(s.cx, s.kit.page);
  WebKit& kit = s.kit;
  JS::RootedObject objectProto(s.cx, JS::GetRealmObjectPrototype(s.cx));
  // BaseAudioContext is an EventTarget: statechange travels the interaction tree.
  JS::RootedObject eventProto(s.cx, s.events->prototype());
  if (!objectProto || !eventProto) return false;
  if (!kit.Define("AudioParam", &kParamClass, objectProto, WebKit::Reject, &s.param) ||
      !kit.Define("BaseAudioContext", nullptr, eventProto, WebKit::Reject, &s.base) ||
      !kit.Define("AudioContext", &kContextClass, s.base.prototype, ContextCtor,
                  &s.context) ||
      !kit.Define("OfflineAudioContext", &kContextClass, s.base.prototype, ContextCtor,
                  &s.offline) ||
      !kit.Define("AudioNode", &kNodeClass, eventProto, WebKit::Reject, &s.node) ||
      !kit.Define("AudioScheduledSourceNode", &kNodeClass, s.node.prototype,
                  WebKit::Reject, &s.scheduled) ||
      !kit.Define("OscillatorNode", &kNodeClass, s.scheduled.prototype, OscillatorCtor,
                  &s.oscillator)) return false;
  for (int32_t index = 0; index < kNodeDefCount; ++index) {
    WebFace face(s.cx);
    JS::RootedObject parent(s.cx, kNodeDefs[index].scheduled ? s.scheduled.prototype.get()
                                                             : s.node.prototype.get());
    if (!kit.Define(kNodeDefs[index].name, &kNodeClass, parent,
                    index == 10 ? WebKit::Reject : NodeCtor, &face) ||
        !s.nodePrototypes.get().append(face.prototype.get()) ||
        !s.nodeConstructors.get().append(face.constructor.get())) return false;
    if (kNodeDefs[index].kind == kCompressorNode) {
      s.compressor.prototype = face.prototype.get();
      s.compressor.constructor = face.constructor.get();
    }
  }
  // AudioDestinationNode extends AudioNode and is not constructible. Each
  // context creates its own instance in ContextCtor; this only installs the
  // one interface and its maxChannelCount accessor.
  if (!kit.Define("AudioDestinationNode", &kNodeClass, s.node.prototype,
                  WebKit::Reject, &s.destination) ||
      !kit.Accessor(s.destination, "AudioDestinationNode", "maxChannelCount",
                    DestinationGet, nullptr, 0) ||
      !kit.Define("AudioBuffer", &kBufferClass, objectProto, BufferCtor, &s.buffer) ||
      !kit.Define("PeriodicWave", nullptr, objectProto, PlainCtor, &s.periodic) ||
      !kit.Define("AudioListener", &kListenerClass, objectProto, WebKit::Reject,
                  &s.listener) ||
      !kit.Define("Worklet", &kWorkletBaseClass, objectProto, WebKit::Reject, &s.worklet) ||
      !kit.Define("AudioWorklet", &kWorkletClass, s.worklet.prototype, WebKit::Reject,
                  &s.audioWorklet) ||
      !InstallAuxiliary(s, objectProto))
    return false;
  // AudioWorklet's constructor inherits from Worklet's constructor (Firefox parity).
  {
    JS::RootedObject awCtor(s.cx, s.audioWorklet.constructor);
    JS::RootedObject wCtor(s.cx, s.worklet.constructor);
    if (!JS_SetPrototype(s.cx, awCtor, wCtor)) return false;
  }
  for (const MethodDef& factory : kFactories)
    if (!kit.Method(s.base, "BaseAudioContext", factory.name, MakeNode,
                     factory.slot==kSlotAudioBuffer?3:
                     (factory.slot==8 || factory.slot==kSlotPeriodicWave)?2:0, factory.slot))
      return false;
  for (const MethodDef& accessor : kContextAccessors)
    if (!kit.Accessor(s.base, "BaseAudioContext", accessor.name, ContextGet, nullptr,
                      accessor.slot)) return false;
  if (!InstallContextSurface(s) || !InstallNodeSurface(s)) return false;
  if (!kit.Method(s.node, "AudioNode", "connect", NodeConnect, 1, kGenericNode) ||
      !kit.Method(s.node, "AudioNode", "disconnect", NodeDisconnect, 0, kGenericNode))
    return false;
  for (const MethodDef& attribute : kNodeAttributes) {
    const bool writable = attribute.slot >= kAttrChannelCount;
    if (!kit.Accessor(s.node, "AudioNode", attribute.name, NodeAttributeGet,
                      writable ? NodeAttributeSet : nullptr, attribute.slot))
      return false;
  }
  if (!kit.Method(s.listener, "AudioListener", "setPosition", ListenerSetPosition, 3,
                  0) ||
      !kit.Method(s.listener, "AudioListener", "setOrientation", ListenerSetOrientation,
                  6, 0) ||
      !kit.Method(s.context, "AudioContext", "getOutputTimestamp", ContextTimestamp, 0,
                  0))
    return false;
  for (const char* name : {"start", "stop"}) {
    if (!kit.Method(s.scheduled, "AudioScheduledSourceNode", name, NodeMethod, 0,
                    kGenericNode))
      return false;
  }
  if (!kit.Accessor(s.oscillator, "OscillatorNode", "frequency", ChildGet, nullptr,
                    kOscillatorNode * 8) ||
      !kit.Accessor(s.oscillator, "OscillatorNode", "detune", ChildGet, nullptr,
                    kOscillatorNode * 8 + 1) ||
      !kit.Accessor(s.oscillator, "OscillatorNode", "type", OscillatorTypeGet,
                    OscillatorTypeSet, 0)) return false;
  JS::RootedObject gainPrototype(s.cx, s.nodePrototypes.get()[0]);
  if (!AccessorOn(kit, gainPrototype, "GainNode", "gain", ChildGet, nullptr,
                  kGainNode * 8)) return false;
  for (int32_t index = 0; index < 5; ++index)
    if (!kit.Accessor(s.compressor, "DynamicsCompressorNode", kCompressorParams[index],
                      ChildGet, nullptr, kCompressorNode * 8 + index)) return false;
  if (!kit.Accessor(s.param, "AudioParam", "value", ParamGet, ParamSet, 0) ||
      !kit.Accessor(s.param, "AudioParam", "defaultValue", ParamGet, nullptr, 1) ||
      !kit.Accessor(s.param, "AudioParam", "minValue", ParamGet, nullptr, 2) ||
      !kit.Accessor(s.param, "AudioParam", "maxValue", ParamGet, nullptr, 3))
    return false;
  constexpr size_t kParamMethodCount = sizeof(kParamMethods) / sizeof(kParamMethods[0]);
  for (size_t index = 0; index < kParamMethodCount; ++index)
    if (!kit.Method(s.param, "AudioParam", kParamMethods[index], ParamAutomate,
                    index>=5?1:index>=3?3:2,
                    int32_t(index))) return false;
  if (!AccessorOn(kit, s.buffer.prototype, "AudioBuffer", "length", BufferGet,
                  nullptr, 0) ||
      !AccessorOn(kit, s.buffer.prototype, "AudioBuffer", "numberOfChannels",
                  BufferGet, nullptr, 1) ||
      !AccessorOn(kit, s.buffer.prototype, "AudioBuffer", "sampleRate", BufferGet,
                  nullptr, 2) ||
      !AccessorOn(kit, s.buffer.prototype, "AudioBuffer", "duration", BufferGet,
                  nullptr, 3) ||
      !kit.Method(s.buffer, "AudioBuffer", "getChannelData", ChannelData, 1, 0) ||
      !kit.Method(s.buffer, "AudioBuffer", "copyFromChannel", CopyChannel, 2, 0) ||
      !kit.Method(s.buffer, "AudioBuffer", "copyToChannel", CopyChannel, 2, 1))
    return false;
  if (!kit.Method(s.offline, "OfflineAudioContext", "startRendering", StartRendering, 0,
                  0) ||
      !AccessorOn(kit, s.offline.prototype, "OfflineAudioContext", "length",
                  ContextGet, nullptr, 5) ||
      !AccessorOn(kit, s.offline.prototype, "OfflineAudioContext", "oncomplete",
                  AudioEventGet, AudioEventSet, kContextOnCompleteSlot) ||
      !kit.Method(s.worklet, "Worklet", "addModule", ResolvedPromise, 1, 0) ||
      !kit.Accessor(s.audioWorklet, "AudioWorklet", "port", WorkletPort, nullptr, 0))
    return false;
  // Firefox exposes no `webkitAudioContext` alias; installing one is a
  // Chromium fingerprint (see the D34 correction).
  return true;
}

}  // namespace ruyijs::web_audio
