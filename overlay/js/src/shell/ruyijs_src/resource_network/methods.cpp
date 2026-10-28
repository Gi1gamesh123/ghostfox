#include "resource_network/state.h"
#include "jsfriendapi.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::resource_network {
bool Method(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* record = Receiver(cx, args);
  if (!record) return false;
  const auto operation = Operation(js::GetFunctionNativeReserved(&args.callee(), 1).toInt32());
  // Reported after receiver validation, so an invalid receiver still throws
  // from the same place and records nothing.
  if (State* owner = FactoryOwner(args))
    owner->Observe("Headers", OperationName(operation), int(ApiAccess::Call));
  if (operation == Operation::Cookies) {
    std::vector<std::string> values;
    if (!Report(cx, Cookies(record, &values))) return false;
    JS::RootedObject array(cx, JS::NewArrayObject(cx, values.size()));
    if (!array) return false;
    JS::RootedValue value(cx);
    for (size_t i = 0; i < values.size(); ++i)
      if (!Value(cx, values[i], &value) || !JS_DefineElement(cx, array, i, value, JSPROP_ENUMERATE)) return false;
    args.rval().setObject(*array); return true;
  }
  const bool two = operation == Operation::Append || operation == Operation::Set;
  if (argc < (two ? 2u : 1u)) return Error(cx, "Missing required Headers argument");
  std::string name, value;
  if (!Convert(cx, args[0], &name) || (two && !Convert(cx, args[1], &value))) return false;
  if (operation == Operation::Get || operation == Operation::Has) {
    bool found = false;
    if (operation == Operation::Has) {
      if (!Report(cx, Has(record, name, &found))) return false;
      args.rval().setBoolean(found); return true;
    }
    if (!Report(cx, Get(record, name, &value, &found))) return false;
    if (!found) { args.rval().setNull(); return true; }
    return Value(cx, value, args.rval());
  }
  Mutation mutation = operation == Operation::Delete ? Mutation::Delete :
      operation == Operation::Set ? Mutation::Set : Mutation::Append;
  if (!Report(cx, Mutate(record, mutation, name, value))) return false;
  args.rval().setUndefined(); return true;
}
bool ForEach(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* record = Receiver(cx, args);
  if (!record) return false;
  if (State* owner = FactoryOwner(args))
    owner->Observe("Headers", "forEach", int(ApiAccess::Call));
  if (!argc || !args[0].isObject() || !JS::IsCallable(&args[0].toObject()))
    return Error(cx, "Headers forEach callback must be callable");
  JS::RootedValue callback(cx, args[0]), thisValue(cx, args.get(1)), ignored(cx);
  JS::RootedValueArray<3> callArgs(cx);
  callArgs[2].set(args.thisv());
  for (size_t index = 0;; ++index) {
    std::string key, value;
    bool found = false;
    if (!Report(cx, Entry(record, index, &key, &value, &found))) return false;
    if (!found) break;
    if (!Value(cx, value, callArgs[0]) || !Value(cx, key, callArgs[1]) ||
        !JS::Call(cx, thisValue, callback, callArgs, &ignored)) return false;
  }
  args.rval().setUndefined(); return true;
}
}
