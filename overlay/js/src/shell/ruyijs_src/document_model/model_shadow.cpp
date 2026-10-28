#include "document_model/model_internal.h"

namespace ruyijs::document_model {
Status Model::AttachShadow(NodeKey host, const ShadowOptions& options, Draft* result) noexcept {
  return Protect([&] {
    auto& owner = State().owner(host);
    State().budget(1, 0);
    domain::ShadowOptions native{options.closed, options.delegates_focus, options.clonable,
                                 options.serializable, options.manual_slots};
    const auto root = owner.attach_shadow_root(host.node, native);
    try { *result = State().remember(Key(host.document, root), false); }
    catch (...) { owner.remove_node(root); throw; }
    State().Touch();
  });
}
Status Model::InspectShadow(NodeKey key, ShadowSnapshot* result) const noexcept {
  return Protect([&] {
    const auto* root = State().owner(key).shadow_node(key.node);
    if (!root) throw domain::DomError(domain::DomErrorCode::InvalidState, "Expected a ShadowRoot identity");
    const auto& native = root->options();
    *result = {Key(key.document, root->shadow_host_id()),
      {native.closed, native.delegates_focus, native.clonable, native.serializable, native.manual_slots}};
  });
}
Status Model::ShadowScope(NodeKey key, NodeKey* result) const noexcept {
  return Protect([&] {
    const auto& owner = State().owner(key);
    const auto root = owner.root_id(key.node);
    *result = root != key.node && owner.shadow_node(root) ? Key(key.document, root) : NodeKey{};
  });
}
}
