#include "document_model/state.h"
#include "document_model/range.h"
#include "js/Object.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
JSObject* Realm::rangePrototype() {
  Interface* iface = state_->Find("Range");
  return iface ? iface->prototype.get() : nullptr;
}
bool Realm::RangeContents(JS::HandleValue value, JS::MutableHandleObjectVector elements) {
  JSContext* cx = state_->cx;
  JS::RootedObject object(cx, value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  Range* range = object && JS::GetClass(object) == &kRangeClass
      ? static_cast<Range*>(JS::GetReservedSlot(object, 0).toPrivate()) : nullptr;
  if (!range || !range->owner) return TypeError(cx, "Range geometry called on an incompatible receiver");
  State& tree = RangeTree(*range);
  JSAutoRealm realm(cx, tree.page);
  NodeKey common;
  std::vector<NodeKey> contained;
  if (!RangeCommonAncestor(tree, range->start, range->end, &common) ||
      !RangeTopLevelContained(tree, range->start, range->end, common, &contained)) return false;
  // Firefox walks every node of the range in tree order (PreContentIterator):
  // the contained top-level nodes and all their descendants.
  std::vector<NodeKey> stack(contained.rbegin(), contained.rend()), children;
  while (!stack.empty()) {
    const NodeKey key = stack.back();
    stack.pop_back();
    NodeSnapshot node;
    if (!tree.Check(tree.model.Inspect(key, &node))) return false;
    if (node.kind != NodeKind::Element && node.kind != NodeKind::Text) continue;
    JS::RootedValue wrapper(cx);
    if (!tree.Wrap(key, &wrapper) || !elements.append(&wrapper.toObject())) return false;
    if (node.kind != NodeKind::Element) continue;
    if (!tree.Check(tree.model.Children(key, &children))) return false;
    stack.insert(stack.end(), children.rbegin(), children.rend());
  }
  return true;
}
}
