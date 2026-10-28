// Read-only document port for the existing WindowProperties factory.
#include "document_model/state.h"
#include "document_model/collections.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
namespace {
bool Resolve(State& state, DomStringView name, JS::MutableHandleValue result) {
  result.setUndefined();
  if (name.empty()) return true;
  std::vector<NodeKey> nodes;
  // Child contexts take precedence over elements. Walk the original tree for
  // ordering and exclude shadow trees; FrameLink supplies the existing identity.
  if (!state.Check(state.model.Query(state.main, QueryKind::TagName, u"iframe", &nodes))) return false;
  for (const auto key : nodes) {
    std::optional<DomString> value;
    if (!state.Check(state.model.GetAttribute(key, u"name", &value))) return false;
    if (!value || DomStringView(*value) != name) continue;
    for (auto* link = state.frames.get(); link; link = link->next.get()) {
      if (!Same(link->frame, key)) continue;
      result.setObject(*link->window);
      return JS_WrapValue(state.cx, result);
    }
  }
  DocumentSnapshot document;
  if (!state.Check(state.model.InspectDocument(state.main, &document))) return false;
  if (document.mode != DocumentMode::Html) return true;
  if (!state.Check(state.model.Query(state.main, QueryKind::Id, name, &nodes))) return false;
  if (!nodes.empty()) return state.Wrap(nodes.front(), result);
  if (!state.Check(state.model.WindowNamed(state.main, name, &nodes))) return false;
  if (nodes.empty()) return true;
  if (nodes.size() == 1) return state.Wrap(nodes.front(), result);
  auto* collection = state.FindCollection(CollectionKind::HTMLCollection, state.main,
                                           CollectionFilter::WindowNamed, name);
  if (!collection) collection = state.NewCollection(CollectionKind::HTMLCollection, state.main,
                                                     CollectionFilter::WindowNamed, name, true);
  if (!collection || !state.SyncCollections()) return false;
  result.setObject(*collection->object);
  return true;
}
}
bool Realm::ResolveWindowName(JS::HandleString name, JS::MutableHandleValue result) {
  State& state = *state_;
  JS::RootedValue value(state.cx, JS::StringValue(name));
  DomString text;
  if (!String(state.cx, value, &text)) return false;
  {
    JSAutoRealm realm(state.cx, state.page);
    if (!Resolve(state, text, &value)) return false;
  }
  if (!JS_WrapValue(state.cx, &value)) return false;
  result.set(value);
  return true;
}
}
