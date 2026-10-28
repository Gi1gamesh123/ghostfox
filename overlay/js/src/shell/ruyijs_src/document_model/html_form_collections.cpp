#include "document_model/html_form.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
namespace {
CollectionRecord* Receiver(JSContext* cx, const JS::CallArgs& args, CollectionFilter filter, State** state) {
  JS::RootedObject object(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  auto* record = object && JS::GetClass(object) == &kCollectionClass
      ? static_cast<CollectionRecord*>(JS::GetReservedSlot(object, 0).toPrivate()) : nullptr;
  if (!record || record->filter != filter) {
    TypeError(cx, "Form collection receiver is incompatible"); return nullptr;
  }
  JS::RootedValue scope(cx, JS::GetReservedSlot(object, 2));
  auto* entry = ControlReceiver(cx, scope, "HTMLFormElement");
  if (!entry) return nullptr;
  *state = entry->owner; return record;
}
bool Named(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = nullptr;
  auto* record = Receiver(cx, args, CollectionFilter::FormControls, &state);
  if (!record) return false;
  DomString name;
  if (!String(cx, args.get(0), &name)) {
    if (!JS_IsExceptionPending(cx) || JS_IsThrowingOutOfMemory(cx)) return false;
    JS_ClearPendingException(cx); args.rval().setNull(); return true;
  }
  JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx, state->page);
    state->Observe("HTMLFormControlsCollection", "namedItem", int(ApiAccess::Call));
    if (!FormNamed(*state, record->scope, name, &result)) return false;
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}
bool RadioGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = nullptr;
  auto* record = Receiver(cx, args, CollectionFilter::RadioGroup, &state);
  if (!record) return false;
  DomString value;
  {
    JSAutoRealm realm(cx, state->page);
    state->Observe("RadioNodeList", "value", int(ApiAccess::Read));
    if (!state->Check(state->model.RadioValue(record->scope, record->argument, &value))) return false;
  }
  return Value(cx, DomStringView(value), args.rval());
}
bool RadioSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = nullptr;
  auto* record = Receiver(cx, args, CollectionFilter::RadioGroup, &state);
  if (!record) return false;
  DomString value;
  if (!String(cx, args.get(0), &value)) return false;
  {
    JSAutoRealm realm(cx, state->page);
    state->Observe("RadioNodeList", "value", int(ApiAccess::Write));
    if (!state->Check(state->model.SetRadioValue(record->scope, record->argument, value))) return false;
  }
  args.rval().setUndefined(); return true;
}
}
bool InstallFormCollections(State& state) {
  return state.Method("HTMLFormControlsCollection", "namedItem", Named, 1, 0) &&
      state.Accessor("RadioNodeList", "value", RadioGet, 0, RadioSet);
}
}
