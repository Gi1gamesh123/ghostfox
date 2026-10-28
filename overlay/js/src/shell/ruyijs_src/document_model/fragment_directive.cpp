// The native Document wrapper owns its SameObject directive.
#include "document_model/fragment_directive.h"
#include "document_model/state.h"
#include "runtime/page_helpers.h"
#include "js/Object.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
namespace {
// The only instance edge is its original Document; no private native state.
const JSClass kFragmentDirectiveClass = {
    "FragmentDirective", JSCLASS_HAS_RESERVED_SLOTS(1)};

bool GetFragmentDirective(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Wrapper* entry = NodeReceiver(cx, args.thisv(), NodeKind::Document);
  if (!entry) return false;
  JS::RootedObject source(cx, entry->object);
  State& factory = entry->factory ? *entry->factory : *entry->owner;
  FunctionState(args)->Observe("Document", "fragmentDirective", int(ApiAccess::Read));
  JS::RootedValue result(cx, JS::GetReservedSlot(source, kNodeFragmentDirectiveSlot));
  if (result.isUndefined()) {
    {
      JSAutoRealm realm(cx, factory.page);
      Interface* iface = factory.Find("FragmentDirective");
      if (!iface) return TypeError(cx, "FragmentDirective factory is not installed");
      JS::RootedObject parent(cx, source), prototype(cx, iface->prototype);
      if (!JS_WrapObject(cx, &parent)) return false;
      JS::RootedObject object(cx,
          JS_NewObjectWithGivenProto(cx, &kFragmentDirectiveClass, prototype));
      if (!object) return false;
      JS::SetReservedSlot(object, 0, JS::ObjectValue(*parent));
      if (!factory.helpers.Object("FragmentDirective", object)) return false;
      result.setObject(*object);
    }
    {
      JSAutoRealm realm(cx, source);
      if (!JS_WrapValue(cx, &result)) return false;
      JS::SetReservedSlot(source, kNodeFragmentDirectiveSlot, result);
    }
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result);
  return true;
}
}  // namespace

bool InstallFragmentDirective(State& state) {
  JS::RootedObject parent(state.cx, JS::GetRealmObjectPrototype(state.cx));
  return parent && state.DefineInterface("FragmentDirective", parent, Illegal, false) &&
      state.Accessor("Document", "fragmentDirective", GetFragmentDirective, 0);
}
}  // namespace ruyijs::document_model
