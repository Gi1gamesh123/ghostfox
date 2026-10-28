#ifndef RUYIJS_WEB_AUDIO_RENDER_GRAPH_H
#define RUYIJS_WEB_AUDIO_RENDER_GRAPH_H
#include <cstdint>
#include <vector>

#include "web_audio/compressor.h"
#include "web_audio/internal.h"
#include "web_audio/wave_tables.h"

namespace ruyijs::web_audio {

// Gecko's WEBAUDIO_BLOCK_SIZE: offline rendering runs one quantum at a time.
inline constexpr int32_t kRenderQuantum = 128;

// One node taking part in an offline render: the mixed input and the output of
// the current render quantum, plus the DSP state the node's algorithm owns for
// the duration of the render.
struct RenderNode {
  JSObject* node = nullptr;
  int32_t kind = kGenericNode;
  uint32_t channels = 1;
  std::vector<size_t> inputs;  // graph positions of the nodes feeding this one
  std::vector<float> input;
  std::vector<float> output;
  OscillatorVoice voice;
  DynamicsCompressor compressor;
};

// Collects the context's source nodes, every node reachable from them and the
// destination, computes each node's channel count from its channelCountMode,
// and orders the list so a node always follows the nodes feeding it.
bool BuildRenderNodes(State& state, JS::HandleObject context, float sample_rate,
                      std::vector<RenderNode>* nodes);

}  // namespace ruyijs::web_audio
#endif
