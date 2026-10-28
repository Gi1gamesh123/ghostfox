#include "document_model/state.h"
#include <map>
#include <set>

namespace ruyijs::document_model {
// A temporary snapshot of the original domain associations, rebuilt only at
// a reclamation point. No second index or state survives this call.
bool DocumentReachability(State& state, std::set<NodeKey, NodeKeyLess>* kept,
                          std::vector<NodeKey>* candidates) {
  std::vector<std::pair<NodeKey, NodeKey>> links;
  if (state.model.DocumentLinks(&links).code != ErrorCode::None) return false;
  if (links.empty()) return true;
  std::map<NodeKey, std::vector<NodeKey>, NodeKeyLess> edges;
  std::set<NodeKey, NodeKeyLess> examined;
  std::vector<NodeKey> pending;
  for (const auto& [host, fragment] : links) {
    NodeKey parentRoot, contentRoot; bool parentConnected = false, contentConnected = false;
    if (state.model.Root(host, &parentRoot, &parentConnected).code != ErrorCode::None ||
        state.model.Root(fragment, &contentRoot, &contentConnected).code != ErrorCode::None) return false;
    edges[parentRoot].push_back(contentRoot);
    candidates->push_back(fragment);
    for (const auto& item : {std::pair{parentRoot, parentConnected}, std::pair{contentRoot, contentConnected}}) {
      if (!examined.insert(item.first).second) continue;
      if (item.second || state.Pinned(item.first)) {
        kept->insert(item.first); pending.push_back(item.first);
      }
    }
  }
  for (size_t i = 0; i < pending.size(); ++i) {
    auto found = edges.find(pending[i]);
    if (found == edges.end()) continue;
    for (NodeKey target : found->second)
      if (kept->insert(target).second) pending.push_back(target);
  }
  return true;
}
}
