#include "document_model/html_control.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
namespace {
enum Kind { Null, For, Control, Dormant };
struct Member { const char* iface; const char* name; Kind kind; bool set; int method; };
const Member kMembers[] = {
  {"HTMLLabelElement","form",Null,false,-1},
  {"HTMLLabelElement","htmlFor",For,true,-1},
  {"HTMLLabelElement","control",Control,false,-1},
  {"HTMLDataListElement","options",Null,false,-1},
  {"HTMLFieldSetElement","disabled",Dormant,true,-1},
  {"HTMLFieldSetElement","form",Dormant,false,-1},
  {"HTMLFieldSetElement","name",Dormant,true,-1},
  {"HTMLFieldSetElement","type",Dormant,false,-1},
  {"HTMLFieldSetElement","elements",Dormant,false,-1},
  {"HTMLFieldSetElement","willValidate",Dormant,false,-1},
  {"HTMLFieldSetElement","validity",Dormant,false,-1},
  {"HTMLFieldSetElement","validationMessage",Dormant,false,-1},
  {"HTMLFieldSetElement","checkValidity",Dormant,false,0},
  {"HTMLFieldSetElement","reportValidity",Dormant,false,0},
  {"HTMLFieldSetElement","setCustomValidity",Dormant,false,1},
  {"HTMLOutputElement","htmlFor",Dormant,false,-1},
  {"HTMLOutputElement","form",Dormant,false,-1},
  {"HTMLOutputElement","name",Dormant,true,-1},
  {"HTMLOutputElement","type",Dormant,false,-1},
  {"HTMLOutputElement","defaultValue",Dormant,true,-1},
  {"HTMLOutputElement","value",Dormant,true,-1},
  {"HTMLOutputElement","willValidate",Dormant,false,-1},
  {"HTMLOutputElement","validity",Dormant,false,-1},
  {"HTMLOutputElement","validationMessage",Dormant,false,-1},
  {"HTMLOutputElement","checkValidity",Dormant,false,0},
  {"HTMLOutputElement","reportValidity",Dormant,false,0},
  {"HTMLOutputElement","setCustomValidity",Dormant,false,1},
  {"HTMLOutputElement","labels",Dormant,false,-1},
  {"HTMLLegendElement","form",Dormant,false,-1},
  {"HTMLLegendElement","align",Dormant,true,-1}
};
bool Read(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto& m = kMembers[FunctionSlot(args)];
  auto* entry = ControlReceiver(cx, args.thisv(), m.iface);
  if (!entry) return false;
  auto& state = *entry->owner;
  JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe(m.iface, m.name, int(m.method >= 0 ? ApiAccess::Call : ApiAccess::Read));
    if (m.kind == Dormant) return TypeError(cx, "Public form auxiliary member has no initialized implementation");
    if (m.kind == Null) result.setNull();
    else if (m.kind == Control) {
      NodeKey key;
      if (!state.Check(state.model.LabelControl(entry->key, &key)) || !state.Wrap(key, &result)) return false;
    } else {
      std::optional<DomString> value;
      if (!state.Check(state.model.GetAttribute(entry->key, u"for", &value)) ||
          !Value(cx, DomStringView(value.value_or(u"")), &result)) return false;
    }
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}
bool Write(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto& m = kMembers[FunctionSlot(args)];
  auto* entry = ControlReceiver(cx, args.thisv(), m.iface);
  if (!entry) return false;
  auto& state = *entry->owner;
  DomString value;
  if (m.kind != Dormant && !ControlString(cx, args.get(0), &value)) return false;
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe(m.iface, m.name, int(ApiAccess::Write));
    if (m.kind == Dormant) return TypeError(cx, "Public form auxiliary member has no initialized implementation");
    if (!state.Check(state.model.SetAttribute(entry->key, u"for", value))) return false;
  }
  args.rval().setUndefined(); return true;
}
}
bool InstallFormAux(State& state) {
  for (size_t i = 0; i < std::size(kMembers); ++i) {
    const auto& m = kMembers[i];
    if (m.method >= 0) {
      if (!state.Method(m.iface, m.name, Read, m.method, int32_t(i))) return false;
    } else if (!state.Accessor(m.iface, m.name, Read, int32_t(i), m.set ? Write : nullptr)) return false;
  }
  return true;
}
}
