#include "document_model/range.h"

#include "document_model/state.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
namespace {
uint32_t Length(const NodeSnapshot& value) {
  return value.kind == NodeKind::Text || value.kind == NodeKind::Comment || value.kind == NodeKind::ProcessingInstruction
             ? uint32_t(value.data_length) : uint32_t(value.child_count);
}

// setStart/setEnd (mode 0) and the four before/after shorthands (1 = before,
// 2 = after). Validation order matches the reference: offset, then root.
bool SetBoundary(State& state, Range& range, const JS::CallArgs& args,
                 bool start, int mode) {
  NodeKey node;
  if (!NodeArgument(state, args.get(0), &node)) return false;
  BoundaryPoint point{node, 0};
  if (mode == 0) {
    if (!JS::ToUint32(state.cx, args.get(1), &point.offset)) return false;
  } else {
    NodeSnapshot value;
    if (!state.Check(state.model.Inspect(node, &value))) return false;
    if (Same(value.parent, {}))
      return ThrowDOM(state, "InvalidNodeTypeError", "Node has no parent");
    point.node = value.parent;
    if (!RangeIndex(state, value.parent, node, &point.offset)) return false;
    if (mode == 2) ++point.offset;
  }
  NodeSnapshot value;
  if (!state.Check(state.model.Inspect(point.node, &value))) return false;
  if (point.offset > Length(value))
    return ThrowDOM(state, "IndexSizeError", "Offset is greater than the node length");
  // Firefox nsRange::SetStart/SetEnd: a boundary in another root, or a start
  // after the end (an end before the start), collapses the range onto it.
  const BoundaryPoint other = start ? range.end : range.start;
  NodeSnapshot otherValue;
  if (!state.Check(state.model.Inspect(other.node, &otherValue))) return false;
  bool collapse = !Same(value.root, otherValue.root);
  if (!collapse) {
    int order = 0;
    if (!RangeComparePoints(state, point, other, &order)) return false;
    collapse = start ? order > 0 : order < 0;
  }
  if (collapse) range.start = range.end = point;
  else if (start) range.start = point;
  else range.end = point;
  args.rval().setUndefined();
  return true;
}

bool SelectNode(State& state, Range& range, const JS::CallArgs& args) {
  NodeKey node;
  if (!NodeArgument(state, args.get(0), &node)) return false;
  NodeSnapshot value;
  if (!state.Check(state.model.Inspect(node, &value))) return false;
  if (Same(value.parent, {}))
    return ThrowDOM(state, "InvalidNodeTypeError", "Node has no parent");
  uint32_t index = 0;
  if (!RangeIndex(state, value.parent, node, &index)) return false;
  range.start = BoundaryPoint{value.parent, index};
  range.end = BoundaryPoint{value.parent, index + 1};
  args.rval().setUndefined();
  return true;
}

bool SelectContents(State& state, Range& range, const JS::CallArgs& args) {
  NodeKey node;
  if (!NodeArgument(state, args.get(0), &node)) return false;
  NodeSnapshot value;
  if (!state.Check(state.model.Inspect(node, &value))) return false;
  range.start = BoundaryPoint{node, 0};
  range.end = BoundaryPoint{node, Length(value)};
  args.rval().setUndefined();
  return true;
}

bool Collapse(State& state, Range& range, const JS::CallArgs& args) {
  if (JS::ToBoolean(args.get(0))) range.end = range.start;
  else range.start = range.end;
  args.rval().setUndefined();
  return true;
}

// The clone belongs to the caller's realm and shares the boundary tree.
bool CloneRange(State& state, Range& range, JS::MutableHandleValue result) {
  Range* copy = NewRange(state, result);
  if (!copy) return false;
  copy->document = range.document;
  RangeRehome(*copy, RangeTree(range), range.start);
  copy->end = range.end;
  return true;
}

// A node of another realm's tree moves the range there before the boundary
// methods run (the new root collapses the range onto the new boundary).
void Follow(Range& range, JS::HandleValue value) {
  JSObject* object = value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr;
  if (!object || JS::GetClass(object) != &kNodeClass) return;
  auto* node = static_cast<Wrapper*>(JS::GetReservedSlot(object, 0).toPrivate());
  if (!node || node->dead || !node->owner || node->owner == &RangeTree(range)) return;
  RangeRehome(range, *node->owner, BoundaryPoint{node->key, 0});
}

bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  Range* range = RangeOf(state, args);
  if (!range) return false;
  State& tree = RangeTree(*range);
  JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx, tree.page);
    NodeKey common;
    if (!RangeCommonAncestor(tree, range->start, range->end, &common) || !tree.Wrap(common, &result))
      return false;
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result);
  return true;
}

bool Dispatch(State& state, Range* range, const JS::CallArgs& args);

bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  Range* range = RangeOf(state, args);
  if (!range) return false;
  const auto method = static_cast<RangeMethod>(FunctionSlot(args));
  if (method == RangeMethod::CloneRange) return CloneRange(state, *range, args.rval());
  // A failed call leaves the range where it was, in the tree it was in.
  State& before = RangeTree(*range);
  const BoundaryPoint start = range->start, end = range->end;
  if (method <= RangeMethod::SelectNodeContents) Follow(*range, args.get(0));
  State& tree = RangeTree(*range);
  bool ok;
  {
    JSAutoRealm realm(cx, tree.page);
    ok = Dispatch(tree, range, args);
  }
  if (!ok) {
    if (&before != &tree) { RangeRehome(*range, before, start); range->end = end; }
    return false;
  }
  return JS_WrapValue(cx, args.rval());
}

bool Dispatch(State& state, Range* range, const JS::CallArgs& args) {
  switch (static_cast<RangeMethod>(FunctionSlot(args))) {
    case RangeMethod::SetStart: return SetBoundary(state, *range, args, true, 0);
    case RangeMethod::SetEnd: return SetBoundary(state, *range, args, false, 0);
    case RangeMethod::SetStartBefore: return SetBoundary(state, *range, args, true, 1);
    case RangeMethod::SetStartAfter: return SetBoundary(state, *range, args, true, 2);
    case RangeMethod::SetEndBefore: return SetBoundary(state, *range, args, false, 1);
    case RangeMethod::SetEndAfter: return SetBoundary(state, *range, args, false, 2);
    case RangeMethod::SelectNode: return SelectNode(state, *range, args);
    case RangeMethod::SelectNodeContents: return SelectContents(state, *range, args);
    case RangeMethod::Collapse: return Collapse(state, *range, args);
    case RangeMethod::CloneContents: return RangeCloneContents(state, *range, args.rval());
    case RangeMethod::ExtractContents: return RangeExtractContents(state, *range, args.rval());
    case RangeMethod::DeleteContents:
      if (!RangeDeleteContents(state, *range)) return false;
      args.rval().setUndefined();
      return true;
    case RangeMethod::InsertNode:
      if (!RangeInsertNode(state, *range, args.get(0))) return false;
      args.rval().setUndefined();
      return true;
    case RangeMethod::CloneRange: return false;  // handled by Call in the caller's realm
    case RangeMethod::Detach: args.rval().setUndefined(); return true;
    case RangeMethod::ToString: return RangeText(state, *range, args.rval());
  }
  return false;
}
}  // namespace

bool InstallRange(State& state) {
  if (!InstallAbstractRange(state) ||
      !state.Accessor("Range", "commonAncestorContainer", Get, 0)) return false;
  struct Operation { const char* name; RangeMethod method; unsigned length; };
  static const Operation methods[] = {
    {"setStart", RangeMethod::SetStart, 2}, {"setEnd", RangeMethod::SetEnd, 2},
    {"setStartBefore", RangeMethod::SetStartBefore, 1},
    {"setStartAfter", RangeMethod::SetStartAfter, 1},
    {"setEndBefore", RangeMethod::SetEndBefore, 1},
    {"setEndAfter", RangeMethod::SetEndAfter, 1},
    {"selectNode", RangeMethod::SelectNode, 1},
    {"selectNodeContents", RangeMethod::SelectNodeContents, 1},
    {"collapse", RangeMethod::Collapse, 0},
    {"cloneContents", RangeMethod::CloneContents, 0},
    {"extractContents", RangeMethod::ExtractContents, 0},
    {"deleteContents", RangeMethod::DeleteContents, 0},
    {"insertNode", RangeMethod::InsertNode, 1},
    {"cloneRange", RangeMethod::CloneRange, 0},
    {"detach", RangeMethod::Detach, 0},
    {"toString", RangeMethod::ToString, 0}
  };
  for (const Operation& method : methods)
    if (!state.Method("Range", method.name, Call, method.length,
                      int32_t(method.method))) return false;
  return InstallRangeStubs(state);
}
}  // namespace ruyijs::document_model
