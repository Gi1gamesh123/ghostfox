#include "document_model/html_select.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#include <cstring>

namespace ruyijs::document_model {
namespace {
CollectionRecord* Receiver(JSContext* cx, const JS::CallArgs& args) {
  JS::RootedObject object(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  auto* record = object && JS::GetClass(object) == &kCollectionClass
      ? static_cast<CollectionRecord*>(JS::GetReservedSlot(object, 0).toPrivate()) : nullptr;
  if (!record || record->filter != CollectionFilter::SelectOptions) {
    TypeError(cx, "HTMLOptionsCollection receiver is incompatible"); return nullptr;
  }
  return record;
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  int field = FunctionSlot(args);
  auto* record = Receiver(cx, args);
  if (!record) return false;
  args.rval().setNumber(field ? -1 : double(CollectionCount(*record))); return true;
}
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  int field = FunctionSlot(args);
  if (!Receiver(cx, args)) return false;
  uint32_t length; int32_t index;
  if (field ? !JS::ToInt32(cx, args.get(0), &index) : !JS::ToUint32(cx, args.get(0), &length)) return false;
  args.rval().setUndefined(); return true;
}
bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  int op = FunctionSlot(args);
  if (!Receiver(cx, args)) return false;
  if (!argc) return TypeError(cx, "HTMLOptionsCollection method requires an argument");
  if (op == 0) {
    JS::RootedObject object(cx, args[0].isObject() ? js::CheckedUnwrapStatic(&args[0].toObject()) : nullptr);
    auto* entry = object && JS::GetClass(object) == &kNodeClass
        ? static_cast<Wrapper*>(JS::GetReservedSlot(object, 0).toPrivate()) : nullptr;
    if (!entry || entry->dead || !entry->owner) return TypeError(cx, "Expected an option or optgroup");
    JSAutoRealm realm(cx, entry->owner->page);
    NodeSnapshot node;
    if (!entry->owner->Check(entry->owner->model.Inspect(entry->key, &node))) return false;
    if (std::strcmp(node.interface_name, "HTMLOptionElement") &&
        std::strcmp(node.interface_name, "HTMLOptGroupElement")) return TypeError(cx, "Expected an option or optgroup");
  } else {
    int32_t index;
    if (!JS::ToInt32(cx, args[0], &index)) return false;
  }
  args.rval().setUndefined(); return true;
}
}
bool SelectOptions(Wrapper& entry, JS::MutableHandleValue out) {
  auto& state = *entry.owner;
  auto* list = state.FindCollection(CollectionKind::HTMLCollection, entry.key, CollectionFilter::SelectOptions, u"option");
  if (!list) list = state.NewCollection(CollectionKind::HTMLCollection, entry.key, CollectionFilter::SelectOptions, u"option", true);
  if (!list || !state.SyncCollections()) return false;
  JS::SetReservedSlot(list->object, 2, JS::ObjectValue(*entry.object));
  out.setObject(*list->object); return true;
}
bool InstallOptionsCollection(State& state) {
  return state.Accessor("HTMLOptionsCollection", "length", Get, 0, Set) &&
      state.Accessor("HTMLOptionsCollection", "selectedIndex", Get, 1, Set) &&
      state.Method("HTMLOptionsCollection", "add", Call, 1, 0) &&
      state.Method("HTMLOptionsCollection", "remove", Call, 1, 1) &&
      InstallCollectionIterator(state, "HTMLOptionsCollection", int32_t(CollectionIteration::Values), 0);
}
}
