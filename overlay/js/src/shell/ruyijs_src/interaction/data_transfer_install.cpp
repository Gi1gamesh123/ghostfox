#include "interaction/data_transfer.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::interaction {
namespace {
void Finalize(JS::GCContext*, JSObject* object) {
  const auto value = JS::GetReservedSlot(object, kTransferModel);
  if (!value.isUndefined()) DeleteTransferModel(static_cast<TransferModel*>(value.toPrivate()));
}
const JSClassOps transferOps = {
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, Finalize, nullptr, nullptr, nullptr};
bool Illegal(JSContext* cx, unsigned, JS::Value*) {
  return Error(cx, "TypeError", "Illegal constructor");
}
bool Construct(JSContext* cx, unsigned argc, JS::Value* vp) {
  const auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  if (!args.isConstructing()) { args.rval().setUndefined(); return trace.Finish(true); }
  State* state = RealmState(cx);
  if (!state) return Error(cx, "TypeError", "DataTransfer realm is not installed");
  ObserveTransfer(cx, "DataTransfer", "constructor", ApiAccess::Construct);
  JS::RootedObject object(cx, JS_NewObjectForConstructor(cx, &kTransferClass, args));
  if (!object) return false;
  TransferModel* model = NewTransferModel();
  if (!model) { JS_ReportOutOfMemory(cx); return false; }
  JS::SetReservedSlot(object, kTransferModel, JS::PrivateValue(model));
  JS::RootedObject items(cx, JS_NewObjectWithGivenProto(cx, &kTransferListClass,
                                                      state->transferListPrototype));
  if (!items) return false;
  JS::SetReservedSlot(items, 0, JS::ObjectValue(*object));
  JS::SetReservedSlot(object, kTransferItems, JS::ObjectValue(*items));
  args.rval().setObject(*object);
  return trace.Finish(true);
}
bool Define(State& state, const char* name, const JSClass* clasp, JSNative constructor,
             const JSPropertySpec* properties, const JSFunctionSpec* methods,
             JS::MutableHandleObject out) {
  JSContext* cx = state.cx;
  JS::RootedObject parent(cx, JS::GetRealmObjectPrototype(cx));
  out.set(JS_InitClass(cx, state.page, clasp, parent, name, constructor, 0,
                      properties, methods, nullptr, nullptr));
  if (!out || !Tag(cx, out, name)) return false;
  JS::RootedValue ctor(cx);
  if (!JS_GetProperty(cx, state.page, name, &ctor) || !ctor.isObject()) return false;
  JS::RootedObject ctorObject(cx, &ctor.toObject());
  if (!state.helpers->Interface(name, out) || !state.helpers->Constructor(name, ctorObject))
    return false;
  for (auto p = properties; p->name.string(); ++p)
    if (!state.helpers->Accessor(name, out, p->name.string())) return false;
  for (auto m = methods; m->name.string(); ++m) {
    JS::RootedValue value(cx);
    if (!JS_GetProperty(cx, out, m->name.string(), &value) || !value.isObject()) return false;
    JS::RootedObject function(cx, &value.toObject());
    if (!state.helpers->Member(function, name, m->name.string(), "call")) return false;
  }
  return true;
}
}
const JSClass kTransferClass = {"DataTransfer",
    JSCLASS_HAS_RESERVED_SLOTS(kTransferSlots) | JSCLASS_FOREGROUND_FINALIZE, &transferOps};
const JSClass kTransferListClass = {"DataTransferItemList", JSCLASS_HAS_RESERVED_SLOTS(1)};
const JSClass kTransferItemClass = {"DataTransferItem", 0};
bool InstallDataTransfer(State& s) {
  JS::RootedObject prototype(s.cx);
  if (!Define(s, "DataTransfer", &kTransferClass, Construct,
              kTransferProperties, kTransferMethods, &prototype)) return false;
  s.transferPrototype = prototype;
  if (!Define(s, "DataTransferItem", &kTransferItemClass, Illegal,
              kTransferItemProperties, kTransferItemMethods, &prototype)) return false;
  s.transferItemPrototype = prototype;
  if (!Define(s, "DataTransferItemList", &kTransferListClass, Illegal,
              kTransferListProperties, kTransferListMethods, &prototype)) return false;
  s.transferListPrototype = prototype;
  return true;
}
}  // namespace ruyijs::interaction
