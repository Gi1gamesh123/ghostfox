// Original MutationObserver registration and queue lifecycle.
#include "web_observers/internal.h"
#include "document_model/realm.h"
#include "jsapi.h"

namespace ruyijs::web_observers {
void State::Observe(Observer& observer, const Registration& registration) {
  for (Registration& existing : observer.registrations) {
    if (existing.origin || existing.source != registration.source ||
        existing.target.document != registration.target.document ||
        existing.target.node != registration.target.node) continue;
    // observe() replaces the options for one target and, per DOM, drops the
    // transient registrations that came from it.
    const uint64_t id = existing.id;
    existing = registration;
    existing.id = id;
    RemoveTransients(observer, id);
    return;
  }
  observer.registrations.push_back(registration);
  observer.registrations.back().id = ++observer.nextRegistration;
  ++registration.source->registrationCount;
  registration.source->RefreshRecording();
}

bool State::AddTransients(Observer& observer, const MutationNotice& notice) {
  if (notice.kind != MutationKind::ChildList || notice.removed.empty()) return true;
  const size_t count = observer.registrations.size();
  for (size_t i = 0; i < count; ++i) {
    const Registration entry = observer.registrations[i];
    if (entry.source != this || !entry.subtree || notice.sequence <= entry.baseline) continue;
    bool ancestor = false;
    if (!document->AncestorOrSelf(entry.target, notice.target, &ancestor)) return false;
    if (!ancestor) continue;
    for (NodeKey removed : notice.removed) {
      Registration transient = entry;
      transient.target = removed;
      transient.baseline = notice.sequence;
      transient.id = ++observer.nextRegistration;
      transient.origin = entry.origin ? entry.origin : entry.id;
      observer.registrations.push_back(std::move(transient));
      ++registrationCount;
    }
  }
  return true;
}

void State::RemoveTransients(Observer& observer, uint64_t origin) {
  auto& entries = observer.registrations;
  for (auto it = entries.begin(); it != entries.end();) {
    if (!it->origin || (origin && it->origin != origin)) { ++it; continue; }
    State* source = it->source;
    it = entries.erase(it);
    --source->registrationCount;
    source->RefreshRecording();
  }
}

void State::Disconnect(Observer& observer) {
  for (const Registration& entry : observer.registrations) {
    --entry.source->registrationCount;
    entry.source->RefreshRecording();
  }
  observer.registrations.clear();
  observer.pending.get().clearAndFree();
  observer.scheduled = false;
}

bool State::TakeRecords(Observer& observer, JS::MutableHandleObject result) {
  JS::Rooted<PendingRecords> batch(cx, PendingRecords());
  batch.get().swap(observer.pending.get());
  observer.scheduled = false;
  return BuildBatch(batch.get(), result);
}

void State::RefreshRecording() {
  if (!document) return;
  const bool any = registrationCount != 0;
  if (any == recording) return;
  recording = any;
  document->SetMutationRecording(any);
  if (any) {
    cursor = document->MutationSequence();
    document->SetNodeLifecycleSink(&NodeLifecycleThunk, this);
    document->SetMutationSink(&SinkThunk, this);
  } else {
    document->SetNodeLifecycleSink(nullptr, nullptr);
    document->SetMutationSink(nullptr, nullptr);
    cursor = 0;
  }
}
}  // namespace ruyijs::web_observers
