#include "web_audio/render_graph.h"

#include <algorithm>

#include "js/Array.h"
#include "js/String.h"

// The graph the offline render walks. A node publishes one outgoing-edge array
// (kNodeConnectionsSlot, written by graph.cpp); edges into an AudioParam carry
// no audio and are skipped here.
namespace ruyijs::web_audio {
namespace {

bool Connections(JSContext* cx, JSObject* node, std::vector<JSObject*>* out) {
  JS::Value value = JS::GetReservedSlot(node, kNodeConnectionsSlot);
  if (!value.isObject()) return true;
  JS::RootedObject array(cx, &value.toObject());
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, array, &length)) return false;
  for (uint32_t index = 0; index < length; ++index) {
    JS::RootedValue element(cx);
    if (!JS_GetElement(cx, array, index, &element)) return false;
    if (!element.isObject()) continue;
    JS::RootedObject target(cx, &element.toObject());
    if (JS::GetClass(target) != &kNodeClass) continue;  // AudioParam edge
    out->push_back(target);
  }
  return true;
}

size_t IndexOf(const std::vector<RenderNode>& nodes, JSObject* node) {
  for (size_t index = 0; index < nodes.size(); ++index)
    if (nodes[index].node == node) return index;
  return nodes.size();
}

// Firefox's AudioNode::ChannelCountForInput: "explicit" uses channelCount,
// "clamped-max" clamps it to the largest input, "max" follows the inputs.
uint32_t ChannelCount(JSContext* cx, JS::HandleObject node, int32_t kind,
                      uint32_t inputs, uint32_t destination_channels) {
  if (kind == kOscillatorNode) return 1;  // a source always publishes one channel
  if (kind == kDestinationNode) return destination_channels;
  const uint32_t count =
      uint32_t(JS::GetReservedSlot(node, kNodeChannelCountSlot).toInt32());
  JS::Value mode = JS::GetReservedSlot(node, kNodeChannelModeSlot);
  bool explicit_mode = false, clamped = false;
  if (mode.isString()) {
    JS::RootedString text(cx, mode.toString());
    if (!JS_StringEqualsAscii(cx, text, "explicit", &explicit_mode) ||
        !JS_StringEqualsAscii(cx, text, "clamped-max", &clamped))
      return 1;
  }
  const uint32_t largest = std::max(inputs, 1u);
  if (explicit_mode) return std::max(count, 1u);
  if (clamped) return std::max(std::min(count, largest), 1u);
  return largest;
}

// Appends a node once unless it is already known.
bool Add(std::vector<RenderNode>* nodes, JSObject* node) {
  if (IndexOf(*nodes, node) != nodes->size()) return true;
  nodes->push_back(RenderNode());
  nodes->back().node = node;
  return true;
}

}  // namespace

bool BuildRenderNodes(State& state, JS::HandleObject context, float sample_rate,
                      std::vector<RenderNode>* nodes) {
  JSContext* cx = state.cx;
  JS::Value destination_value = JS::GetReservedSlot(context, kContextDestinationSlot);
  JS::Value list = JS::GetReservedSlot(context, kContextNodesSlot);
  if (!destination_value.isObject() || !list.isObject()) return true;
  JS::RootedObject sources(cx, &list.toObject());
  uint32_t source_count = 0;
  if (!JS::GetArrayLength(cx, sources, &source_count)) return false;
  if (!source_count) return true;  // no source: the buffer stays silent
  for (uint32_t index = 0; index < source_count; ++index) {
    JS::RootedValue element(cx);
    if (!JS_GetElement(cx, sources, index, &element)) return false;
    if (!element.isObject()) continue;
    JS::RootedObject node(cx, &element.toObject());
    Add(nodes, node);
  }
  // Everything reachable from the sources, plus the destination itself.
  for (size_t index = 0; index < nodes->size(); ++index) {
    std::vector<JSObject*> edges;
    if (!Connections(cx, (*nodes)[index].node, &edges)) return false;
    for (JSObject* target : edges) Add(nodes, target);
  }
  JS::RootedObject destination(cx, &destination_value.toObject());
  Add(nodes, destination);
  // Kahn's algorithm: emit a node once every node feeding it has been emitted.
  std::vector<size_t> indegree(nodes->size(), 0);
  for (size_t index = 0; index < nodes->size(); ++index) {
    std::vector<JSObject*> edges;
    if (!Connections(cx, (*nodes)[index].node, &edges)) return false;
    for (JSObject* target : edges) {
      const size_t found = IndexOf(*nodes, target);
      if (found != nodes->size()) ++indegree[found];
    }
  }
  std::vector<RenderNode> ordered;
  std::vector<bool> placed(nodes->size(), false);
  while (ordered.size() < nodes->size()) {
    bool progressed = false;
    for (size_t index = 0; index < nodes->size(); ++index) {
      if (placed[index] || indegree[index]) continue;
      placed[index] = true;
      ordered.push_back((*nodes)[index]);
      std::vector<JSObject*> edges;
      if (!Connections(cx, (*nodes)[index].node, &edges)) return false;
      for (JSObject* target : edges) {
        const size_t found = IndexOf(*nodes, target);
        if (found != nodes->size() && indegree[found]) --indegree[found];
      }
      progressed = true;
    }
    if (!progressed) break;  // a cycle cannot happen; keep the rest in place
  }
  for (size_t index = 0; index < nodes->size(); ++index)
    if (!placed[index]) ordered.push_back((*nodes)[index]);
  *nodes = std::move(ordered);
  // Channel counts and one quantum of input/output per node, in graph order.
  const uint32_t destination_channels =
      uint32_t(JS::GetReservedSlot(context, kContextChannelsSlot).toInt32());
  for (size_t index = 0; index < nodes->size(); ++index) {
    RenderNode& current = (*nodes)[index];
    JS::RootedObject node(cx, current.node);
    current.kind = JS::GetReservedSlot(node, kNodeKindSlot).toInt32();
    uint32_t inputs = 0;
    for (size_t other = 0; other < index; ++other) {  // upstream nodes only
      std::vector<JSObject*> edges;
      if (!Connections(cx, (*nodes)[other].node, &edges)) return false;
      if (std::find(edges.begin(), edges.end(), current.node) != edges.end()) {
        current.inputs.push_back(other);
        inputs = std::max(inputs, (*nodes)[other].channels);
      }
    }
    current.channels =
        ChannelCount(cx, node, current.kind, inputs, destination_channels);
    current.input.assign(size_t(current.channels) * kRenderQuantum, 0.0f);
    current.output.assign(size_t(current.channels) * kRenderQuantum, 0.0f);
    current.compressor.Configure(sample_rate, current.channels);
  }
  return true;
}

}  // namespace ruyijs::web_audio
