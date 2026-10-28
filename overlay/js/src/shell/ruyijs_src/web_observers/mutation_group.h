#ifndef RUYIJS_WEB_OBSERVERS_MUTATION_GROUP_H
#define RUYIJS_WEB_OBSERVERS_MUTATION_GROUP_H
#include <vector>

namespace ruyijs::document_model { class Realm; }
namespace ruyijs::web_observers {
struct State;
struct Observer;

// Borrowed routing and creation order within the existing browsing Context.
// States retain sole ownership of observers, registrations and pending records.
struct MutationGroup {
  std::vector<State*> sources;
  Observer* first = nullptr;
  Observer* last = nullptr;
  bool deliveryQueued = false;
  State* Find(document_model::Realm*) const;
  void Link(Observer&);
  void Unlink(Observer&);
};
}  // namespace ruyijs::web_observers
#endif
