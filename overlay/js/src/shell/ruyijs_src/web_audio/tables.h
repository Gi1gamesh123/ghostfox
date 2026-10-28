#ifndef RUYIJS_WEB_AUDIO_TABLES_H
#define RUYIJS_WEB_AUDIO_TABLES_H

#include <cstdint>

// The ordered install tables (factory slots, context accessors, node
// attributes, promise edges, param methods). Data only: the one installer in
// install.cpp registers every entry, so no second list exists anywhere.
namespace ruyijs::web_audio {
namespace tables {

struct MethodDef {
  const char* name;
  int32_t slot;
};

// The ordered BaseAudioContext factory surface from ruyijs audio_bindings.cpp.
// The slot is the kNodeDefs index or a kSlot* constant for non-node results.
constexpr MethodDef kFactories[] = {
    {"createOscillator", kSlotOscillator},
    {"createGain", 0},
    {"createAnalyser", 1},
    {"createBiquadFilter", 2},
    {"createChannelSplitter", 3},
    {"createChannelMerger", 4},
    {"createConvolver", 5},
    {"createDelay", 6},
    {"createDynamicsCompressor", 7},
    {"createIIRFilter", 8},
    {"createPanner", 9},
    {"createScriptProcessor", 10},
    {"createStereoPanner", 11},
    {"createWaveShaper", 12},
    {"createConstantSource", 13},
    {"createBufferSource", 14},
    {"createBuffer", kSlotAudioBuffer},
    {"createPeriodicWave", kSlotPeriodicWave}};

constexpr MethodDef kContextAccessors[] = {
    {"currentTime", 0}, {"sampleRate", 1}, {"state", 2},
    {"destination", 3}, {"listener", 4},
    {"audioWorklet", 8}};

// The AudioNode identity/mixing attributes; everything from channelCount on has
// a setter (numberOfInputs/Outputs/context are readonly).
constexpr MethodDef kNodeAttributes[] = {
    {"numberOfInputs", kAttrNumberOfInputs},
    {"numberOfOutputs", kAttrNumberOfOutputs},
    {"context", kAttrContext},
    {"channelCount", kAttrChannelCount},
    {"channelCountMode", kAttrChannelCountMode},
    {"channelInterpretation", kAttrChannelInterpretation}};

// resume/suspend/close run the same state machine; the slot picks the edge.
constexpr MethodDef kContextPromises[] = {{"resume", 0}, {"suspend", 1}, {"close", 2}};

constexpr const char* kParamMethods[] = {
    "setValueAtTime", "linearRampToValueAtTime", "exponentialRampToValueAtTime",
    "setTargetAtTime", "setValueCurveAtTime", "cancelScheduledValues"};

constexpr const char* kCompressorParams[] = {"threshold", "knee", "ratio",
                                             "attack", "release"};

}  // namespace tables
}  // namespace ruyijs::web_audio

#endif /* RUYIJS_WEB_AUDIO_TABLES_H */
