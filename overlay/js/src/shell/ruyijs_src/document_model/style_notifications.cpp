// Original committed DOM changes touch only existing traced CSS associations.
#include "document_model/state.h"
#include "document_model/style_source_values.h"
#include "js/Exception.h"
#include "js/Object.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
void Realm::SetStyleNotifyPort(StyleNotifyPort port, void* context) {
  state_->styleNotify = port; state_->styleNotifyContext = context;
  state_->model.SetStyleSink(port ? NotifyStyleMutation : nullptr, port ? state_.get() : nullptr);
}
bool NotifyStyleAssociations(State& state, Wrapper& entry, bool migration) {
  if (!state.styleNotify || entry.dead || !entry.object) return true;
  if (JS_IsExceptionPending(state.cx)) return false;
  JSAutoRealm realm(state.cx, state.page);
  JS::RootedObject node(state.cx, entry.object);
  JS::RootedValue value(state.cx, JS::GetReservedSlot(node, kNodeStyleAssociationSlot));
  if (value.isObject()) {
    JS::RootedObject associated(state.cx, &value.toObject());
    if (!state.styleNotify(state.styleNotifyContext, associated, StyleNotifyAction::RetireSheet))
      return false;
  }
  value.set(JS::GetReservedSlot(node, kNodeAdoptedStylesSlot));
  if (migration && value.isObject()) {
    JS::RootedObject associated(state.cx, &value.toObject());
    if (!state.styleNotify(state.styleNotifyContext, associated, StyleNotifyAction::MigrateAdopted))
      return false;
  }
  return true;
}
void NotifyStyleMutation(void* context, NodeKey key, MutationKind kind, DomStringView name,
                         DomStringView space, const DomString* old) {
  State& state = *static_cast<State*>(context);
  if (JS_IsExceptionPending(state.cx)) return;
  if (kind == MutationKind::Attributes) {
    if (!space.empty()) return;
    if (name!=u"title" && name!=u"media" && name!=u"type" && name!=u"href" &&
        name!=u"rel" && name!=u"disabled" && name!=u"crossorigin") return;
    NodeSnapshot snapshot;
    if (!state.Check(state.model.Inspect(key, &snapshot))) return;
    const bool link = std::string(snapshot.interface_name) == "HTMLLinkElement";
    if (name != u"title" && name != u"media" && name != u"type" &&
        (!link || (name != u"href" && name != u"rel" && name != u"disabled" &&
                    name != u"crossorigin"))) return;
    std::optional<DomString> current;
    Status status = state.model.GetAttribute(key, name, &current);
    if (status.code != ErrorCode::None) { state.Check(status); return; }
    if ((!old && !current) || (old && current && *old == *current)) return;
    if (link) { NotifyLinkStyle(state, key); return; }
    Wrapper* entry = state.Find(key);
    if (entry) NotifyStyleAssociations(state, *entry);
    return;
  }
  // HTMLStyleElement's mutation listener observes its descendants. Walk the
  // original parents, stopping at a ShadowRoot; never traverse host boundaries.
  std::vector<NodeKey> parents;
  Status status = state.model.AncestorPath(key, &parents);
  if (status.code != ErrorCode::None) { state.Check(status); return; }
  for (NodeKey current : parents) {
    Wrapper* entry = state.Find(current);
    if (entry && !NotifyStyleAssociations(state, *entry)) return;
  }
}
}
