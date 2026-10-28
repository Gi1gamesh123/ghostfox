// Identity and native lifetime publication, independent of mutation recording.
#include "document_model/state.h"

namespace ruyijs::document_model {
void Realm::SetNodeLifecycleSink(NodeLifecycleSink sink, void* context) {
  state_->nodeLifecycleConsumer = sink;
  state_->nodeLifecycleContext = sink ? context : nullptr;
}

void State::NotifyNodeLifecycle(const NodeLifecycleNotice& notice) noexcept {
  if (nodeLifecycleConsumer)
    nodeLifecycleConsumer(nodeLifecycleContext, notice);
}

void State::RetireNodeLifecycle() noexcept {
  // Clear before calling the consumer; this owner's identity lifetime ends
  // once, before any model or wrapper storage is destroyed.
  const auto sink = nodeLifecycleConsumer;
  void* context = nodeLifecycleContext;
  nodeLifecycleConsumer = nullptr;
  nodeLifecycleContext = nullptr;
  if (sink) sink(context, {NodeLifecycleKind::Retired, {}, {}});
}
}  // namespace ruyijs::document_model
