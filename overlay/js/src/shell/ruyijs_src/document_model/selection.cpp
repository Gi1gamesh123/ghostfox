#include "document_model/selection.h"
#include "document_model/state.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include "js/Object.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
// Empty public selection: no native pointers or retained Node/Range arguments.
const JSClass kSelectionClass = {"Selection", 0, nullptr};
namespace {
enum Field { Anchor, AnchorOffset, Focus, FocusOffset, Collapsed, Count, Type };
constexpr const char* fields[] = {"anchorNode", "anchorOffset", "focusNode", "focusOffset",
                                  "isCollapsed", "rangeCount", "type"};
enum Method { GetRange, AddRange, RemoveRange, RemoveAll, Empty, Collapse, Position,
              Start, End, Extend, BaseExtent, AllChildren, Delete, Contains, Stringify, Modify };
constexpr const char* methods[] = {"getRangeAt", "addRange", "removeRange", "removeAllRanges", "empty",
  "collapse", "setPosition", "collapseToStart", "collapseToEnd", "extend", "setBaseAndExtent",
  "selectAllChildren", "deleteFromDocument", "containsNode", "toString", "modify"};
bool Receiver(JSContext* cx, JS::HandleValue value) {
  JSObject* object = value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr;
  return (object && JS::GetClass(object) == &kSelectionClass) ||
      TypeError(cx, "Selection receiver is incompatible");
}
bool Read(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  const auto field = Field(FunctionSlot(args));
  if (!Receiver(cx, args.thisv())) return false;
  state.Observe("Selection", fields[field], int(ApiAccess::Read));
  if (field == Anchor || field == Focus) args.rval().setNull();
  else if (field == Collapsed) args.rval().setBoolean(true);
  else if (field == Type) return Value(cx, DomStringView(u"None"), args.rval());
  else args.rval().setInt32(0);
  return true;
}
bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  const auto method = Method(FunctionSlot(args));
  if (!Receiver(cx, args.thisv())) return false;
  state.Observe("Selection", methods[method], int(ApiAccess::Call));
  if ((method == GetRange || method == Contains) && !argc)
    return TypeError(cx, "Selection method requires one argument");
  if (method == GetRange) {
    uint32_t index;
    if (!JS::ToUint32(cx, args[0], &index)) {
      if (!JS_IsExceptionPending(cx) || JS_IsThrowingOutOfMemory(cx)) return false;
      JS_ClearPendingException(cx);
    }
    return ThrowDOM(state, "IndexSizeError", "Invalid range index");
  }
  if (method == Start || method == End)
    return ThrowDOM(state, "InvalidStateError", "There is no selection to collapse");
  if (method == Collapse || method == Position) {
    uint32_t offset;
    if (!JS::ToUint32(cx, args.get(1), &offset)) return false;
  }
  if (method == Stringify) return Value(cx, DomStringView(), args.rval());
  if (method == Contains) args.rval().setBoolean(false);
  else args.rval().setUndefined();
  return true;
}
}
bool InstallSelectionMembers(State& state) {
  for (int field = Anchor; field <= Type; ++field)
    if (!state.Accessor("Selection", fields[field], Read, field)) return false;
  constexpr unsigned lengths[] = {1,1,1,0,0,1,1,0,0,1,4,1,0,1,0,0};
  for (int method = GetRange; method <= Modify; ++method)
    if (!state.Method("Selection", methods[method], Call, lengths[method], method)) return false;
  return true;
}
}
