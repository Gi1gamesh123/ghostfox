#ifndef RUYIJS_WEB_AUDIO_STATE_H
#define RUYIJS_WEB_AUDIO_STATE_H
#include <cstdint>

#include "interaction/realm.h"
#include "js/GCVector.h"
#include "js/RootingAPI.h"
#include "jsapi.h"
#include "runtime/web_kit.h"
#include "web_audio/model.h"
#include "web_audio/profile.h"
#include "web_audio/realm.h"

namespace ruyijs::web_audio {

// Every node instance shares kNodeClass so AudioNode.prototype methods accept
// any node. Reserved slot 0 keeps the NodeKind, which makes the oscillator and
// compressor accessors precise without one JSClass per interface.
enum NodeKind : int32_t {
  kGenericNode = 0,
  kOscillatorNode = 1,
  kCompressorNode = 2,
  kGainNode = 3,
  kDestinationNode = 4,
};

inline constexpr int32_t kNodeKindSlot = 0;
inline constexpr int32_t kNodeContextSlot = 1;      // owning BaseAudioContext
inline constexpr int32_t kNodeParamBase = 2;        // AudioParam region
inline constexpr int32_t kNodeParamSlots = 5;       // compressor is the widest
inline constexpr int32_t kNodeConnectionsSlot = 7;  // outgoing edges (JS array)
inline constexpr int32_t kNodeTypeSlot = 8;         // OscillatorNode.type
inline constexpr int32_t kNodeDefIndexSlot = 9;     // kNodeDefs row, or -1
inline constexpr int32_t kNodeChannelCountSlot = 10;
inline constexpr int32_t kNodeChannelModeSlot = 11;     // ChannelCountMode string
inline constexpr int32_t kNodeInterpretationSlot = 12;  // ChannelInterpretation string
inline constexpr int32_t kNodeSlotCount = 15;

// The AudioNode attribute accessors share one native; the slot names the
// member. They are read-only except channelCount/Mode/Interpretation.
enum NodeAttribute : int32_t {
  kAttrNumberOfInputs = 0,
  kAttrNumberOfOutputs = 1,
  kAttrContext = 2,
  kAttrChannelCount = 3,
  kAttrChannelCountMode = 4,
  kAttrChannelInterpretation = 5,
};
// Firefox WebAudioUtils::MaxChannelCount: channelCount is validated [1, 32].
inline constexpr uint32_t kMaxChannelCount = 32;
// AudioListener setPosition/setOrientation coordinates (no Firefox getters).
inline constexpr int32_t kListenerSlotCount = 9;
// Gecko buffers nothing between rendering and the audio subsystem, so baseLatency
// is 0.0; outputLatency takes the resist-fingerprinting Windows figure from
// AudioContext::OutputLatency (D44) and only while the context runs.
inline constexpr double kBaseLatency = 0.0;
inline constexpr double kOutputLatency = 0.04;
enum ContextState : int32_t {  // BaseAudioContext's state, on the instance
  kSuspendedState = 0,
  kRunningState = 1,
  kClosedState = 2,
};
// AudioContext::OutputLatency: Gecko reports no latency until the context runs.
inline double OutputLatencyFor(int32_t s) noexcept {
  const Profile p = CurrentProfile();
  if (p.hasOutputLatency) return p.outputLatency;
  return s == kRunningState ? kOutputLatency : 0.0;
}
inline constexpr int32_t kContextStateSlot = 0;
inline constexpr int32_t kContextChannelsSlot = 1;
inline constexpr int32_t kContextLengthSlot = 2;
inline constexpr int32_t kContextRateSlot = 3;
// The context's single destination node and listener; they are created with
// the context so `destination`/`listener` are stable instances, not null.
inline constexpr int32_t kContextDestinationSlot = 4;
inline constexpr int32_t kContextListenerSlot = 5;
inline constexpr int32_t kContextWorkletSlot = 6;  // SameObject AudioWorklet
// The source-node list startRendering walks (D34); oncomplete is the
// EventHandler attribute, null until set; kOnCompleteMember selects it in
// ContextGet/ContextEventSet.
inline constexpr int32_t kContextNodesSlot = 7;
inline constexpr int32_t kContextOnCompleteSlot = 8;
// An OfflineAudioContext whose rendering has started (startRendering() once).
inline constexpr int32_t kContextRenderingSlot = 9;
inline constexpr int32_t kContextOfflineSlot = 10;
inline constexpr int32_t kContextSlotCount = 11;
inline constexpr int32_t kOnCompleteMember = 9;

// AudioBuffer keeps one cached Float32Array per channel; the rendered buffer is
// deterministic and silent, and getChannelData returns the same array each call.
inline constexpr int32_t kBufferDataSlot = 0;
inline constexpr int32_t kBufferLengthSlot = 1;
inline constexpr int32_t kBufferCountSlot = 2;
inline constexpr int32_t kBufferRateSlot = 3;
inline constexpr int32_t kBufferSlotCount = 4;

struct NodeDef {
  const char* name;
  bool scheduled;
  NodeKind kind;
  uint32_t inputs;   // AudioNode.numberOfInputs
  uint32_t outputs;  // AudioNode.numberOfOutputs
};

// Stable node indexes; the Firefox track source extends the original table.
inline constexpr NodeDef kNodeDefs[] = {
    {"GainNode", false, kGainNode, 1, 1},
    {"AnalyserNode", false, kGenericNode, 1, 1},
    {"BiquadFilterNode", false, kGenericNode, 1, 1},
    {"ChannelSplitterNode", false, kGenericNode, 1, 6},
    {"ChannelMergerNode", false, kGenericNode, 6, 1},
    {"ConvolverNode", false, kGenericNode, 1, 1},
    {"DelayNode", false, kGenericNode, 1, 1},
    {"DynamicsCompressorNode", false, kCompressorNode, 1, 1},
    {"IIRFilterNode", false, kGenericNode, 1, 1},
    {"PannerNode", false, kGenericNode, 1, 1},
    {"ScriptProcessorNode", false, kGenericNode, 1, 1},
    {"StereoPannerNode", false, kGenericNode, 1, 1},
    {"WaveShaperNode", false, kGenericNode, 1, 1},
    {"ConstantSourceNode", true, kGenericNode, 0, 1},
    {"AudioBufferSourceNode", true, kGenericNode, 0, 1},
    {"AudioWorkletNode", false, kGenericNode, 1, 1},
    {"MediaElementAudioSourceNode", false, kGenericNode, 0, 1},
    {"MediaStreamAudioSourceNode", false, kGenericNode, 0, 1},
    {"MediaStreamAudioDestinationNode", false, kGenericNode, 1, 0},
    {"MediaStreamTrackAudioSourceNode", false, kGenericNode, 0, 1},
};
inline constexpr int32_t kNodeDefCount = 20;

// Factory slots that are not a kNodeDefs index.
inline constexpr int32_t kSlotOscillator = 21;
inline constexpr int32_t kSlotAudioBuffer = 22;
inline constexpr int32_t kSlotPeriodicWave = 23;

struct State {
  JSContext* cx;
  WebKit kit;
  Model model;
  interaction::Realm* events = nullptr;
  Realm::ClockPort clock = nullptr;
  void* clockContext = nullptr;
  Realm::TaskPort taskPort = nullptr;
  void* taskContext = nullptr;
  Realm::ChannelPort channelPort = nullptr;
  void* channelContext = nullptr;
  WebFace base, context, offline, node, scheduled, oscillator, compressor, param,
      buffer, periodic, listener, worklet, audioWorklet, destination, paramMap;
  JS::PersistentRooted<JS::GCVector<JSObject*>> nodePrototypes, nodeConstructors;
  State(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
        interaction::Realm& events);
  // BaseAudioContext.currentTime in seconds, read from the Context's one clock.
  double Now() const noexcept { return clock ? clock(clockContext) : 0.0; }
};

extern const JSClass kContextClass, kNodeClass, kParamClass, kBufferClass,
    kListenerClass, kWorkletClass, kWorkletBaseClass;

bool ContextCtor(JSContext*, unsigned, JS::Value*);
bool ContextGet(JSContext*, unsigned, JS::Value*);
bool ContextPromise(JSContext*, unsigned, JS::Value*);
bool ResolvedPromise(JSContext*, unsigned, JS::Value*);
bool StartRendering(JSContext*, unsigned, JS::Value*);
bool MakeNode(JSContext*, unsigned, JS::Value*);
bool NodeCtor(JSContext*, unsigned, JS::Value*);
bool NodeConnect(JSContext*, unsigned, JS::Value*);
bool NodeDisconnect(JSContext*, unsigned, JS::Value*);
bool NodeMethod(JSContext*, unsigned, JS::Value*);
bool OscillatorCtor(JSContext*, unsigned, JS::Value*);
bool ChildGet(JSContext*, unsigned, JS::Value*);
// AudioNode attribute surface (numberOfInputs/Outputs, context, channel state).
bool NodeAttributeGet(JSContext*, unsigned, JS::Value*);
bool NodeAttributeSet(JSContext*, unsigned, JS::Value*);
bool InitNodeChannels(State&, JS::HandleObject, int32_t);
// AudioContext.getOutputTimestamp() from the one clock (A13b.3).
bool ContextTimestamp(JSContext*, unsigned, JS::Value*);
// OfflineAudioContext.oncomplete EventHandler setter (null when not callable).
// AudioListener coordinate setters (Firefox IDL exposes no getters).
bool ListenerSetPosition(JSContext*, unsigned, JS::Value*);
bool ListenerSetOrientation(JSContext*, unsigned, JS::Value*);
bool OscillatorTypeGet(JSContext*, unsigned, JS::Value*);
bool OscillatorTypeSet(JSContext*, unsigned, JS::Value*);
bool DestinationGet(JSContext*, unsigned, JS::Value*);
bool ParamGet(JSContext*, unsigned, JS::Value*);
bool ParamSet(JSContext*, unsigned, JS::Value*);
bool ParamAutomate(JSContext*, unsigned, JS::Value*);
bool BufferCtor(JSContext*, unsigned, JS::Value*);
bool BufferGet(JSContext*, unsigned, JS::Value*);
bool ChannelData(JSContext*, unsigned, JS::Value*);
bool CopyChannel(JSContext*, unsigned, JS::Value*);
bool PlainCtor(JSContext*, unsigned, JS::Value*);
bool Noop(JSContext*, unsigned, JS::Value*);

// Creates the AudioParam object for spec and registers its id with the model.
JSObject* NewParam(State& state, const ParamSpec& spec);
// A silent AudioBuffer with the given dimensions and cached zero channels.
JSObject* NewBuffer(State& state, int32_t channels, int32_t length, double rate);
// Copies every channel's samples of `from` into `to` (same dimensions).
bool CopyBufferContents(State& state, JS::HandleObject from, JS::HandleObject to);
// createBuffer(numberOfChannels, length, sampleRate): the same silent buffer
// with the requested, range-checked dimensions.
bool CreateBuffer(State& state, const JS::CallArgs& args,
                  JS::MutableHandleValue result);
// The one error value factory: the page realm's DOMException, or the context's
// pending exception when the caller must throw (never a second error type).
bool MakeDOMException(State& state, const char* name, const char* message,
                      JS::MutableHandleValue result);
bool ThrowDOMException(State& state, const char* name, const char* message);

}  // namespace ruyijs::web_audio
#endif
