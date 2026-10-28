#include "document_model/html_control.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
namespace {
const JSClass kValidityClass = {"ValidityState", JSCLASS_HAS_RESERVED_SLOTS(1), nullptr};
const char* const kNames[] = {"valueMissing","typeMismatch","patternMismatch","tooLong","tooShort",
  "rangeUnderflow","rangeOverflow","stepMismatch","badInput","customError","valid"};
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int field = FunctionSlot(args);
  JS::RootedObject object(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (!object || JS::GetClass(object) != &kValidityClass)
    return TypeError(cx, "ValidityState receiver is incompatible");
  JS::RootedValue owner(cx, JS::GetReservedSlot(object, 0));
  uint32_t flags = 0;
  // An ElementInternals owner holds its own setValidity() flags (element_internals.cpp).
  if (InternalsValidityFlags(&owner.toObject(), &flags)) {
    args.rval().setBoolean(field == 10 ? !flags : (flags & (1u << field)) != 0);
    return true;
  }
  auto* entry = ControlReceiver(cx, owner, nullptr);
  if (!entry) return false;
  {
    JSAutoRealm realm(cx, entry->owner->page);
    entry->owner->Observe("ValidityState", kNames[field], int(ApiAccess::Read));
    if (!entry->owner->Check(entry->owner->model.ControlValidation(entry->key, &flags))) return false;
  }
  args.rval().setBoolean(field == 10 ? !flags : (flags & (1u << field)) != 0);
  return true;
}
}
bool ControlValidity(Wrapper& entry, JS::MutableHandleValue out) {
  auto& state = *entry.owner;
  JS::RootedObject node(state.cx, entry.object);
  // Slot 3 owns only the facade identity; the facade traces the original node.
  const auto& cached = JS::GetReservedSlot(node, 3);
  if (cached.isObject()) { out.set(cached); return true; }
  JS::RootedObject prototype(state.cx, state.Find("ValidityState")->prototype);
  JS::RootedObject object(state.cx, JS_NewObjectWithGivenProto(state.cx, &kValidityClass, prototype));
  if (!object) return false;
  JS::SetReservedSlot(object, 0, JS::ObjectValue(*node));
  JS::SetReservedSlot(node, 3, JS::ObjectValue(*object));
  out.setObject(*object); return true;
}
bool NewInternalsValidity(State& state, JS::HandleObject internals, JS::MutableHandleObject out) {
  JS::RootedObject prototype(state.cx, state.Find("ValidityState")->prototype);
  out.set(JS_NewObjectWithGivenProto(state.cx, &kValidityClass, prototype));
  if (!out) return false;
  JS::SetReservedSlot(out, 0, JS::ObjectValue(*internals));
  return true;
}
bool InstallControlValidity(State& state) {
  for (int i = 0; i < int(std::size(kNames)); ++i)
    if (!state.Accessor("ValidityState", kNames[i], Get, i)) return false;
  return true;
}
}
