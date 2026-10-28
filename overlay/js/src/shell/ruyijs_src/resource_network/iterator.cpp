#include "resource_network/state.h"
#include "jsfriendapi.h"
#include "js/Array.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::resource_network {
bool Iterate(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* record = Receiver(cx, args);
  if (!record) return false;
  if (State* owner = FactoryOwner(args))
    owner->Observe("Headers",
                   OperationName(Operation(js::GetFunctionNativeReserved(&args.callee(), 1).toInt32())),
                   int(ApiAccess::Call));
  JS::RootedObject holder(cx, &js::GetFunctionNativeReserved(&args.callee(), 0).toObject());
  JS::RootedObject prototype(cx, &JS::GetReservedSlot(holder, 2).toObject());
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, &kIteratorClass, prototype));
  if (!object) return false;
  IteratorTicket* ticket = nullptr;
  if (!Report(cx, NewTicket(record, &ticket))) return false;
  JS::SetReservedSlot(object, 0, args.thisv());
  JS::SetReservedSlot(object, 1, JS::NumberValue(0));
  JS::SetReservedSlot(object, 2, js::GetFunctionNativeReserved(&args.callee(), 1));
  JS::SetReservedSlot(object, 3, JS::PrivateValue(ticket));
  args.rval().setObject(*object); return true;
}
bool Next(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  if (!args.thisv().isObject() || JS::GetClass(&args.thisv().toObject()) != &kIteratorClass)
    return Error(cx, "Headers iterator called on incompatible receiver");
  if (State* owner = FactoryOwner(args))
    owner->Observe("Headers Iterator", "next", int(ApiAccess::Call));
  JS::RootedObject iterator(cx, &args.thisv().toObject());
  auto owner = JS::GetReservedSlot(iterator, 0);
  if (!owner.isObject()) return Error(cx, "Headers iterator is not initialized");
  auto* record = static_cast<HeaderRecord*>(JS::GetReservedSlot(&owner.toObject(), 0).toPrivate());
  const size_t index = size_t(JS::GetReservedSlot(iterator, 1).toNumber());
  const auto kind = Operation(JS::GetReservedSlot(iterator, 2).toInt32());
  std::string key, text;
  bool found = false;
  if (!Report(cx, Entry(record, index, &key, &text, &found))) return false;
  JS::RootedValue value(cx), first(cx), second(cx), done(cx, JS::BooleanValue(!found));
  if (found && kind == Operation::Entries) {
    JS::RootedObject pair(cx, JS::NewArrayObject(cx, 2));
    if (!pair || !Value(cx, key, &first) || !Value(cx, text, &second) ||
        !JS_DefineElement(cx, pair, 0, first, JSPROP_ENUMERATE) ||
        !JS_DefineElement(cx, pair, 1, second, JSPROP_ENUMERATE)) return false;
    value.setObject(*pair);
  } else if (found && !Value(cx, kind == Operation::Keys ? key : text, &value)) return false;
  JS::RootedObject result(cx, JS_NewPlainObject(cx));
  if (!result || !JS_DefineProperty(cx, result, "done", done, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, result, "value", value, JSPROP_ENUMERATE)) return false;
  if (found) JS::SetReservedSlot(iterator, 1, JS::NumberValue(double(index + 1)));
  args.rval().setObject(*result); return true;
}
}
