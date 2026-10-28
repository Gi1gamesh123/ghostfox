// Context-local routing of original mutation owners and their lifecycle.
#include "web_observers/internal.h"
#include "document_model/realm.h"
#include "jsapi.h"
#include "js/Object.h"
#include <algorithm>
#include <cassert>

namespace ruyijs::web_observers {
State* MutationGroup::Find(document_model::Realm* document) const {
  for (State* source : sources) if (source->document == document) return source;
  return nullptr;
}

void MutationGroup::Link(Observer& observer) {
  observer.previousInGroup = last;
  if (last) last->nextInGroup = &observer;
  else first = &observer;
  last = &observer;
}

void MutationGroup::Unlink(Observer& observer) {
  if (observer.previousInGroup) observer.previousInGroup->nextInGroup = observer.nextInGroup;
  else first = observer.nextInGroup;
  if (observer.nextInGroup) observer.nextInGroup->previousInGroup = observer.previousInGroup;
  else last = observer.previousInGroup;
  observer.previousInGroup = observer.nextInGroup = nullptr;
}

void State::JoinMutationGroup(const std::shared_ptr<MutationGroup>& group) {
  if (mutationGroup == group) return;
  // Frames joins after installation and before any child script can run.
  assert(!observers && !registrationCount);
  auto& sources = mutationGroup->sources;
  sources.erase(std::remove(sources.begin(), sources.end(), this), sources.end());
  mutationGroup = group;
  mutationGroup->sources.push_back(this);
}

State::~State() {
  // First discard registrations pointing into this source, including foreign
  // observers. Other source counts are released through the original lists.
  for (Observer* observer = mutationGroup->first; observer; observer = observer->nextInGroup) {
    auto& entries = observer->registrations;
    entries.erase(std::remove_if(entries.begin(), entries.end(), [this](const Registration& entry) {
      return entry.source == this;
    }), entries.end());
  }
  registrationCount = 0;
  RefreshRecording();
  for (Observer* observer = observers.get(); observer; observer = observer->next.get()) {
    Disconnect(*observer);
    mutationGroup->Unlink(*observer);
    JS::SetReservedSlot(observer->object, 0, JS::PrivateValue(nullptr));
  }
  auto& sources = mutationGroup->sources;
  sources.erase(std::remove(sources.begin(), sources.end(), this), sources.end());
}
}  // namespace ruyijs::web_observers
