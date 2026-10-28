// Publication timing for the existing mutation-log consumer, owned by the DOM.
#include "document_model/state.h"

namespace ruyijs::document_model {
namespace {
void MutationChanged(void* context) {
  static_cast<State*>(context)->NotifyMutation();
}
}

void State::NotifyMutation() {
  if (!mutationConsumer) return;
  mutationNotificationPending = true;
  FlushMutationNotification();
}

void State::FlushMutationNotification() {
  if (publishingMutation || model.IdentityTransferPending()) return;
  // Nested notifications share the original log. The consumer finishes its
  // current cursor before seeing changes made while handling that cursor.
  publishingMutation = true;
  while (mutationConsumer && mutationNotificationPending) {
    mutationNotificationPending = false;
    mutationConsumer(mutationConsumerContext);
  }
  publishingMutation = false;
}

void Realm::SetMutationSink(void (*sink)(void*), void* context) {
  State& state = *state_;
  state.mutationConsumer = sink;
  state.mutationConsumerContext = sink ? context : nullptr;
  state.mutationNotificationPending = false;
  state.model.SetMutationSink(sink ? MutationChanged : nullptr, sink ? &state : nullptr);
}
}  // namespace ruyijs::document_model
