#include "document_model/xpath.h"
#include "document_model/state.h"
#include "domain/document_model/xpath_values.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::document_model {
namespace {
namespace values = ruyijs::domain::document_model;
enum Field { Type, Number, StringValue, Boolean, Single, Invalid, Length };
constexpr const char* fields[] = {"resultType", "numberValue", "stringValue", "booleanValue",
                                  "singleNodeValue", "invalidIteratorState", "snapshotLength"};
bool Read(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  const auto field = Field(FunctionSlot(args));
  JS::RootedObject object(cx);
  if (!XPathReceiver(cx, args.thisv(), XPathKind::Result, &object)) return false;
  const auto type = uint16_t(JS::GetReservedSlot(object, 1).toInt32());
  state.Observe("XPathResult", fields[field], int(ApiAccess::Read));
  if ((field == Number && type != 1) || (field == StringValue && type != 2) ||
      (field == Boolean && type != 3) || (field == Single && !values::xpath_single_node(type)) ||
      (field == Length && !values::xpath_snapshot(type)))
    return TypeError(cx, "XPathResult field is incompatible with its result type");
  if (field == Type) args.rval().setInt32(type);
  else if (field == Number || field == Length) args.rval().setInt32(0);
  else if (field == Boolean || field == Invalid) args.rval().setBoolean(false);
  else if (field == Single) args.rval().setNull();
  else return Value(cx, DomStringView(), args.rval());
  return true;
}
bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  const bool snapshot = FunctionSlot(args) == 1;
  JS::RootedObject object(cx);
  if (!XPathReceiver(cx, args.thisv(), XPathKind::Result, &object)) return false;
  const auto type = uint16_t(JS::GetReservedSlot(object, 1).toInt32());
  state.Observe("XPathResult", snapshot ? "snapshotItem" : "iterateNext", int(ApiAccess::Call));
  if (!(snapshot ? values::xpath_snapshot(type) : values::xpath_iterator(type)))
    return TypeError(cx, "XPathResult method is incompatible with its result type");
  if (snapshot) {
    if (!argc) return TypeError(cx, "snapshotItem requires one argument");
    uint32_t index;
    if (!JS::ToUint32(cx, args[0], &index)) return false;
  }
  args.rval().setNull(); return true;
}
}
bool InstallXPathResult(State& state) {
  for (int field = Type; field <= Length; ++field)
    if (!state.Accessor("XPathResult", fields[field], Read, field)) return false;
  if (!state.Method("XPathResult", "iterateNext", Call, 0, 0) ||
      !state.Method("XPathResult", "snapshotItem", Call, 1, 1)) return false;
  constexpr const char* constants[] = {"ANY_TYPE", "NUMBER_TYPE", "STRING_TYPE", "BOOLEAN_TYPE",
      "UNORDERED_NODE_ITERATOR_TYPE", "ORDERED_NODE_ITERATOR_TYPE", "UNORDERED_NODE_SNAPSHOT_TYPE",
      "ORDERED_NODE_SNAPSHOT_TYPE", "ANY_UNORDERED_NODE_TYPE", "FIRST_ORDERED_NODE_TYPE"};
  JS::RootedObject prototype(state.cx, state.Find("XPathResult")->prototype);
  JS::RootedObject constructor(state.cx, state.Find("XPathResult")->constructor);
  for (int i = 0; i < int(std::size(constants)); ++i) {
    const unsigned flags = JSPROP_READONLY | JSPROP_PERMANENT | JSPROP_ENUMERATE;
    if (!JS_DefineProperty(state.cx, prototype, constants[i], i, flags & ~JSPROP_PERMANENT) ||  // see binding order
        !JS_DefineProperty(state.cx, constructor, constants[i], i, flags)) return false;
  }
  return true;
}
}
