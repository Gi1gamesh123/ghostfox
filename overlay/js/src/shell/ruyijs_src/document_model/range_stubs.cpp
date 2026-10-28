// Range comparisons, surroundContents and createContextualFragment (DOM
// Standard / DOM Parsing) over the unique tree: boundary geometry from
// range_geom.cpp, fragment parsing from the Model's one HTML fragment path.
#include "document_model/range.h"
#include "document_model/state.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"
#include <string>

namespace ruyijs::document_model {
namespace {
enum Method { CompareBoundaries, ComparePoint, PointInRange, Intersects, Surround, Fragment };
bool Snap(State& state, NodeKey node, NodeSnapshot* value) { return state.Check(state.model.Inspect(node, value)); }
bool Length(const NodeSnapshot& value, uint32_t* length) {
  const bool data = value.kind == NodeKind::Text || value.kind == NodeKind::Comment ||
                    value.kind == NodeKind::ProcessingInstruction;
  *length = uint32_t(data ? value.data_length : value.child_count);
  return true;
}
bool SameRoot(State& state, NodeKey a, NodeKey b, bool* same) {
  NodeSnapshot x, y;
  if (!Snap(state, a, &x) || !Snap(state, b, &y)) return false;
  *same = Same(x.root, y.root);
  return true;
}
// comparePoint / isPointInRange step 1-3: same root, not a doctype, offset within the node.
bool CheckPoint(State& state, Range& range, NodeKey node, uint32_t offset, bool* sameRoot, const char* method) {
  if (!SameRoot(state, node, range.start.node, sameRoot)) return false;
  NodeSnapshot value;
  if (!Snap(state, node, &value)) return false;
  if (!*sameRoot) return true;
  if (value.kind == NodeKind::DocumentType) return ThrowDOM(state, "InvalidNodeTypeError", (std::string(method) + ": Trying to compare with a document").c_str());
  uint32_t length = 0;
  Length(value, &length);
  if (offset > length) return ThrowDOM(state, "IndexSizeError", (std::string(method) + ": Offset is out of bounds").c_str());
  return true;
}
bool PartiallyContainedNonText(State& state, Range& range, bool* found) {
  *found = false;
  for (int side = 0; side < 2 && !*found; ++side) {
    NodeKey from = side ? range.end.node : range.start.node, other = side ? range.start.node : range.end.node;
    for (NodeKey node = from; !Same(node, {});) {
      bool ancestor = false;
      NodeSnapshot value;
      if (!RangeIsAncestor(state, node, other, &ancestor) || !Snap(state, node, &value)) return false;
      if (ancestor) break;
      if (value.kind != NodeKind::Text) { *found = true; break; }
      node = value.parent;
    }
  }
  return true;
}
bool SurroundContents(State& state, Range& range, JS::HandleValue parentValue) {
  NodeKey parent;
  if (!NodeArgument(state, parentValue, &parent)) return false;
  bool partial = false;
  if (!PartiallyContainedNonText(state, range, &partial)) return false;
  if (partial) return ThrowDOM(state, "InvalidStateError", "An attempt was made to use an object that is not, or is no longer, usable");
  NodeSnapshot value;
  if (!Snap(state, parent, &value)) return false;
  if (value.kind == NodeKind::Document || value.kind == NodeKind::DocumentType || value.kind == NodeKind::DocumentFragment)
    return ThrowDOM(state, "InvalidNodeTypeError", "The supplied node is incorrect or has an incorrect ancestor for this operation.");
  JS::RootedValue fragmentValue(state.cx);
  NodeKey fragment;
  if (!RangeExtractContents(state, range, &fragmentValue) || !NodeArgument(state, fragmentValue, &fragment)) return false;
  std::vector<NodeKey> children;
  if (!state.Check(state.model.Children(parent, &children))) return false;
  for (const auto& child : children)
    if (!state.Check(state.model.Remove(parent, child))) return false;
  if (!RangeInsertNode(state, range, parentValue)) return false;
  if (!state.Check(state.model.Insert(parent, fragment))) return false;
  if (!Snap(state, parent, &value) || Same(value.parent, {})) return true;
  uint32_t index = 0;
  if (!RangeIndex(state, value.parent, parent, &index)) return false;
  range.start = BoundaryPoint{value.parent, index};
  range.end = BoundaryPoint{value.parent, index + 1};
  return true;
}
bool ContextualFragment(State& state, Range& range, JS::HandleValue input, JS::MutableHandleValue result) {
  DomString markup;
  if (!String(state.cx, input, &markup)) return false;
  NodeSnapshot start;
  if (!Snap(state, range.start.node, &start)) return false;
  NodeKey context;
  if (start.kind == NodeKind::Element) context = range.start.node;
  else if (start.kind != NodeKind::Document && start.kind != NodeKind::DocumentFragment &&
           start.kind != NodeKind::DocumentType) context = start.parent_element;
  const NodeKey document = start.kind == NodeKind::Document ? range.start.node : start.owner_document;
  Draft fragment;
  if (!state.Check(state.model.CreateFragment(document, &fragment))) return false;
  if (!state.Check(state.model.ParseContextualFragment(context, fragment.root, markup))) {
    state.model.Rollback(fragment.token);
    return false;
  }
  return state.Commit(fragment, result);
}
bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& owner = *FunctionState(args);
  auto method = static_cast<Method>(FunctionSlot(args));
  Range* range = RangeOf(owner, args);
  if (!range) return false;
  State& state = RangeTree(*range);
  if (argc < (method <= PointInRange ? 2u : 1u))
    return TypeError(cx, "Range method requires more arguments");
  switch (method) {
    case CompareBoundaries: {
      uint16_t how;
      if (!JS::ToUint16(cx, args[0], &how)) return false;
      JS::RootedObject source(cx, args[1].isObject() ? js::CheckedUnwrapStatic(&args[1].toObject()) : nullptr);
      if (!source || JS::GetClass(source) != &kRangeClass)
        return TypeError(cx, "compareBoundaryPoints requires a Range");
      if (how > 3) return ThrowDOM(state, "NotSupportedError", "Operation is not supported");
      Range* other = static_cast<Range*>(JS::GetReservedSlot(source, 0).toPrivate());
      bool same = false;
      if (!other || !SameRoot(state, range->start.node, other->start.node, &same)) return false;
      if (!same || &RangeTree(*other) != &state) return ThrowDOM(state, "WrongDocumentError", "Node cannot be used in a document other than the one in which it was created");
      const BoundaryPoint& mine = how == 0 || how == 3 ? range->start : range->end;
      const BoundaryPoint& theirs = how == 0 || how == 1 ? other->start : other->end;
      int order = 0;
      if (!RangeComparePoints(state, mine, theirs, &order)) return false;
      args.rval().setInt32(order);
      return true;
    }
    case ComparePoint: case PointInRange: {
      NodeKey node;
      uint32_t offset;
      bool same = false;
      if (!NodeArgument(state, args[0], &node) || !JS::ToUint32(cx, args[1], &offset)) return false;
      if (!CheckPoint(state, *range, node, offset, &same, method == ComparePoint ? "Range.comparePoint" : "Range.isPointInRange")) return false;
      if (!same) {
        if (method == ComparePoint) return ThrowDOM(state, "WrongDocumentError", "Node cannot be used in a document other than the one in which it was created");
        args.rval().setBoolean(false);
        return true;
      }
      int before = 0, after = 0;
      if (!RangeComparePoints(state, BoundaryPoint{node, offset}, range->start, &before) ||
          !RangeComparePoints(state, BoundaryPoint{node, offset}, range->end, &after)) return false;
      if (method == ComparePoint) args.rval().setInt32(before < 0 ? -1 : after > 0 ? 1 : 0);
      else args.rval().setBoolean(before >= 0 && after <= 0);
      return true;
    }
    case Intersects: {
      NodeKey node;
      bool same = false;
      if (!NodeArgument(state, args[0], &node) || !SameRoot(state, node, range->start.node, &same)) return false;
      NodeSnapshot value;
      if (!same || !Snap(state, node, &value)) { args.rval().setBoolean(false); return same ? false : true; }
      if (Same(value.parent, {})) { args.rval().setBoolean(true); return true; }
      uint32_t index = 0;
      int toEnd = 0, toStart = 0;
      if (!RangeIndex(state, value.parent, node, &index) ||
          !RangeComparePoints(state, BoundaryPoint{value.parent, index}, range->end, &toEnd) ||
          !RangeComparePoints(state, BoundaryPoint{value.parent, index + 1}, range->start, &toStart)) return false;
      args.rval().setBoolean(toEnd < 0 && toStart > 0);
      return true;
    }
    case Surround:
      if (!SurroundContents(state, *range, args[0])) return false;
      args.rval().setUndefined();
      return true;
    case Fragment:
      return ContextualFragment(state, *range, args[0], args.rval());
  }
  return false;
}
}

bool InstallRangeStubs(State& state) {
  struct Spec { const char* name; Method method; unsigned length; };
  const Spec methods[] = {{"compareBoundaryPoints", CompareBoundaries, 2},
    {"comparePoint", ComparePoint, 2}, {"isPointInRange", PointInRange, 2},
    {"intersectsNode", Intersects, 1}, {"surroundContents", Surround, 1},
    {"createContextualFragment", Fragment, 1}};
  for (const auto& spec : methods)
    if (!state.Method("Range", spec.name, Call, spec.length, spec.method)) return false;
  const char* names[] = {"START_TO_START", "START_TO_END", "END_TO_END", "END_TO_START"};
  JS::RootedObject proto(state.cx, state.Find("Range")->prototype);
  JS::RootedObject ctor(state.cx, state.Find("Range")->constructor);
  for (int i = 0; i < 4; ++i) {
    unsigned flags = JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT;
    if (!JS_DefineProperty(state.cx, proto, names[i], i, flags & ~JSPROP_PERMANENT) ||  // see binding order
        !JS_DefineProperty(state.cx, ctor, names[i], i, flags)) return false;
  }
  return true;
}
}
