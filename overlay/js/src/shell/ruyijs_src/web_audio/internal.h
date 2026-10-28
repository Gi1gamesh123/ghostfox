#ifndef RUYIJS_WEB_AUDIO_INTERNAL_H
#define RUYIJS_WEB_AUDIO_INTERNAL_H
#include "web_audio/state.h"

#include "js/Object.h"

// Helpers shared by the node, oscillator and graph translation units. They only
// read the shared node slots, so no unit owns a second node or edge store.
namespace ruyijs::web_audio::detail {

inline State* Self(const JS::CallArgs& args) {
  return static_cast<State*>(WebKit::Owner(args));
}

inline bool IsContext(JSContext* cx, JS::HandleValue value) {
  if (value.isObject() && JS::GetClass(&value.toObject()) == &kContextClass)
    return true;
  return WebKit::Fail(cx, "Web Audio member called on an incompatible receiver");
}

inline bool IsContextKind(JSContext* cx, JS::HandleValue value, bool offline) {
  if (!IsContext(cx, value)) return false;
  if (JS::GetReservedSlot(&value.toObject(), kContextOfflineSlot).toBoolean() == offline)
    return true;
  return WebKit::Fail(cx, "Web Audio member called on an incompatible context");
}

inline bool NodeReceiver(JSContext* cx, const JS::CallArgs& args, int32_t required,
                         JS::MutableHandleObject self) {
  if (!args.thisv().isObject())
    return WebKit::Fail(cx, "Web Audio member called on an incompatible receiver");
  JS::RootedObject object(cx, &args.thisv().toObject());
  if (JS::GetClass(object) != &kNodeClass || (required != kGenericNode &&
      JS::GetReservedSlot(object, kNodeKindSlot).toInt32() != required))
    return WebKit::Fail(cx, "Web Audio member called on an incompatible receiver");
  self.set(object);
  return true;
}

inline JSObject* ContextOf(JSObject* node) {
  JS::Value value = JS::GetReservedSlot(node, kNodeContextSlot);
  return value.isObject() ? &value.toObject() : nullptr;
}

}  // namespace ruyijs::web_audio::detail

namespace ruyijs::web_audio {
// Fills a node's reserved AudioParam slots from the shared domain specs. The
// oscillator constructor lives in another translation unit, so the one
// parameter factory is declared here rather than duplicated per unit.
bool AttachParams(State& state, JS::HandleObject node, int32_t kind);
// Appends a source node (an oscillator) to its owning context's node list so
// startRendering can render it. A node without a context is ignored.
bool RegisterSourceNode(State& state, JS::HandleObject node);
// Renders the node graph into the offline buffer, one 128-frame quantum at a
// time (render.cpp; D34).
bool RenderPlaceholder(State& state, JS::HandleObject context,
                       JS::HandleObject buffer);
}
#endif
