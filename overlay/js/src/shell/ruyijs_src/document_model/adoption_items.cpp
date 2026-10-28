// The existing variadic union conversion, with explicit native node sources.
#include "document_model/foreign_adoption.h"
#include "js/Object.h"
#include "js/Wrapper.h"
#include <memory>

namespace ruyijs::document_model {
namespace {
Wrapper* NodeItem(JS::HandleValue value) {
  JSObject* object = value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr;
  auto* entry = object && JS::GetClass(object) == &kNodeClass
      ? static_cast<Wrapper*>(JS::GetReservedSlot(object, 0).toPrivate()) : nullptr;
  return entry && entry->owner && !entry->dead ? entry : nullptr;
}

bool SingleForeign(Wrapper& receiver, InsertMode mode, Wrapper& source) {
  State& state = *receiver.owner;
  NodeSnapshot node;
  if (!state.Check(state.model.Inspect(receiver.key, &node))) return false;
  const bool inside = mode == InsertMode::Append || mode == InsertMode::Prepend;
  if (!inside && !node.parent.node) return true;
  NodeKey parent = inside ? receiver.key : node.parent;
  NodeKey before = mode == InsertMode::Prepend ? node.first_child
      : mode == InsertMode::Before ? receiver.key
      : mode == InsertMode::After ? node.next_sibling : NodeKey{};
  NodeKey replaced = mode == InsertMode::ReplaceWith ? receiver.key : NodeKey{};
  JS::RootedValue parentValue(state.cx);
  if (!state.Wrap(parent, &parentValue)) return false;
  Wrapper* parentWrapper = NodeReceiver(state.cx, parentValue);
  return parentWrapper && InsertForeign(*parentWrapper, source, before, replaced);
}
}

bool ConvertNodeItems(JSContext* cx, const JS::CallArgs& args, std::vector<Item>* items,
                      std::vector<Wrapper*>* nodes) {
  items->resize(args.length());
  nodes->resize(args.length(), nullptr);
  // Finish all script conversions before reading each node's current owner.
  for (size_t i = 0; i < args.length(); ++i) {
    (*nodes)[i] = NodeItem(args[i]);
    if (!(*nodes)[i] && !String(cx, args[i], &(*items)[i].text)) return false;
  }
  return true;
}

bool InsertNodeItems(Wrapper& receiver, InsertMode mode, const JS::CallArgs& args) {
  JSContext* cx = receiver.owner->cx;
  std::vector<Item> items;
  std::vector<Wrapper*> nodes;
  if (mode != InsertMode::Detach && !ConvertNodeItems(cx, args, &items, &nodes)) return false;
  const size_t length = items.size();
  State& state = *receiver.owner;
  bool foreign = false;
  for (size_t i = 0; i < length; ++i) {
    if (!nodes[i]) continue;
    items[i].node = nodes[i]->key;
    foreign = foreign || nodes[i]->owner != &state;
  }
  if (foreign && length == 1) return SingleForeign(receiver, mode, *nodes[0]);
  std::vector<std::pair<NodeKey, NodeKey>> rekey;
  if (!foreign) {
    const Status status = state.model.InsertItems(receiver.key, mode, items, &rekey);
    state.Rekey(rekey);
    return state.Check(status);
  }
  NodeSnapshot target;
  if (!state.Check(state.model.Inspect(receiver.key, &target))) return false;
  if (mode != InsertMode::Append && mode != InsertMode::Prepend && !target.parent.node) return true;
  const NodeKey document = target.kind == NodeKind::Document ? receiver.key : target.owner_document;
  // DOM's convert-nodes-into-a-node step happens before target pre-insertion.
  // Its fragment and moved children remain observable even if the eventual
  // target rejects the fragment (for example a Document rejects Text).
  Draft fragmentDraft;
  JS::RootedValue fragmentValue(cx);
  if (!state.Check(state.model.CreateFragment(document, &fragmentDraft)) ||
      !state.Commit(fragmentDraft, &fragmentValue)) return false;
  Wrapper* fragment = NodeReceiver(cx, fragmentValue);
  if (!fragment) return false;
  for (size_t i = 0; i < length; ++i) {
    if (nodes[i] && nodes[i]->owner != &state) {
      if (!InsertForeign(*fragment, *nodes[i], {}, {})) return false;
      continue;
    }
    JS::RootedValue text(cx);
    NodeKey key;
    if (nodes[i]) key = nodes[i]->key;
    else {
      Draft draft;
      if (!state.Check(state.model.Create(document, {NodeKind::Text, {}, items[i].text}, &draft)) ||
          !state.Commit(draft, &text)) return false;
      key = draft.root;
    }
    rekey.clear();
    const auto status = state.model.Insert(fragment->key, key, {}, &rekey);
    state.Rekey(rekey);
    if (!state.Check(status)) return false;
  }
  rekey.clear();
  const Status status = state.model.InsertItems(receiver.key, mode, {{fragment->key, {}}}, &rekey);
  state.Rekey(rekey);
  return state.Check(status);
}
}  // namespace ruyijs::document_model
