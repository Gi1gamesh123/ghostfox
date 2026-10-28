// Update original registrations at the DOM owner's native lifetime boundary.
#include "web_observers/internal.h"
#include "document_model/realm.h"
#include <algorithm>

namespace ruyijs::web_observers {
namespace {
bool SameTarget(NodeKey a, NodeKey b) {
  return a.document == b.document && a.node == b.node;
}
}

void NodeLifecycleThunk(void* context,
                        const document_model::NodeLifecycleNotice& notice) noexcept {
  static_cast<State*>(context)->NodeLifecycle(notice);
}

void State::NodeLifecycle(const document_model::NodeLifecycleNotice& notice) noexcept {
  using document_model::NodeLifecycleKind;
  State* destination = notice.kind == NodeLifecycleKind::Transferred
      ? mutationGroup->Find(notice.destination) : nullptr;
  for (Observer* observer = mutationGroup->first; observer; observer = observer->nextInGroup) {
    auto& entries = observer->registrations;
    if (notice.kind == NodeLifecycleKind::Rekeyed || notice.kind == NodeLifecycleKind::Transferred) {
      for (Registration& entry : entries) {
        if (entry.source != this) continue;
        for (const auto& pair : notice.rekeyed) {
          if (!SameTarget(entry.target, pair.first)) continue;
          if (notice.kind == NodeLifecycleKind::Transferred) {
            if (!destination) break;
            entry.source = destination;
            entry.baseline = destination->document->MutationSequence();
            --registrationCount;
            ++destination->registrationCount;
          }
          entry.target = pair.second;
          break;
        }
      }
    } else if (notice.kind == NodeLifecycleKind::Released) {
      const auto before = entries.size();
      entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const Registration& entry) {
        return entry.source == this && std::any_of(notice.released.begin(), notice.released.end(), [&](NodeKey key) {
          return SameTarget(entry.target, key);
        });
      }), entries.end());
      registrationCount -= before - entries.size();
    } else if (notice.kind == NodeLifecycleKind::Retired) {
      entries.erase(std::remove_if(entries.begin(), entries.end(), [this](const Registration& entry) {
        return entry.source == this;
      }), entries.end());
    }
  }
  if (notice.kind == NodeLifecycleKind::Retired) {
    document = nullptr;
    recording = false;
    cursor = 0;
    registrationCount = 0;
  } else if (notice.kind == NodeLifecycleKind::Released || notice.kind == NodeLifecycleKind::Transferred) {
    // These ports only change recording flags and callbacks; no tree read,
    // allocation, JS evaluation or collection runs inside this notification.
    RefreshRecording();
    if (destination) destination->RefreshRecording();
  }
}
}  // namespace ruyijs::web_observers
