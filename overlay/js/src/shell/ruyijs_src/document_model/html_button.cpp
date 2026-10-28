#include "document_model/html_control.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
namespace {
enum Kind { Text, Boolean, Empty, Null, DisabledValidation, Validity, Form };
struct Property { const char* name; const char16_t* attr; Kind kind; bool set; };
const Property kProperties[] = {
  {"disabled",u"disabled",Boolean,true},{"form",u"",Form,false},
  {"formAction",u"formaction",Text,true},{"formEnctype",u"formenctype",Text,true},
  {"formMethod",u"formmethod",Text,true},{"formNoValidate",u"formnovalidate",Boolean,true},
  {"formTarget",u"formtarget",Text,true},{"name",u"name",Text,true},
  {"type",u"type",Text,true},{"value",u"value",Text,true},
  {"willValidate",u"",DisabledValidation,false},{"validity",u"",Validity,false},
  {"validationMessage",u"",Empty,false},{"labels",u"",Null,false}
};
// Public ruyijs's button conversion catches a thrown string coercion and writes
// an empty value. Keep that handling local; do not consume OOM or interrupts.
bool ButtonString(JSContext* cx, JS::HandleValue value, DomString* text) {
  if (String(cx, value, text)) return true;
  if (!JS_IsExceptionPending(cx) || JS_IsThrowingOutOfMemory(cx)) return false;
  JS_ClearPendingException(cx); text->clear(); return true;
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto& p = kProperties[FunctionSlot(args)];
  auto* entry = ControlReceiver(cx, args.thisv(), "HTMLButtonElement");
  if (!entry) return false;
  auto& state = *entry->owner;
  JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe("HTMLButtonElement", p.name, int(ApiAccess::Read));
    DomString text; std::optional<DomString> attr;
    if (p.kind == Null) result.setNull();
    else if (p.kind == Form) { if (!ControlForm(*entry, &result)) return false; }
    else if (p.kind == DisabledValidation) result.setBoolean(false);
    else if (p.kind == Validity) {
      if (!ControlValidity(*entry, &result)) return false;
    } else if (p.kind == Boolean) {
      if (!state.Check(state.model.GetAttribute(entry->key, p.attr, &attr))) return false;
      result.setBoolean(attr.has_value());
    } else {
      if (p.kind == Text && !state.Check(state.model.ButtonAttribute(entry->key, p.attr, &text))) return false;
      if (!Value(cx, DomStringView(text), &result)) return false;
    }
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto& p = kProperties[FunctionSlot(args)];
  auto* entry = ControlReceiver(cx, args.thisv(), "HTMLButtonElement");
  if (!entry) return false;
  bool present = true; DomString text;
  if (p.kind == Boolean) present = JS::ToBoolean(args.get(0));
  else if (!ButtonString(cx, args.get(0), &text)) return false;
  auto& state = *entry->owner;
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe("HTMLButtonElement", p.name, int(ApiAccess::Write));
    if (!state.Check(present ? state.model.SetAttribute(entry->key, p.attr, text)
                            : state.model.RemoveAttribute(entry->key, p.attr))) return false;
  }
  args.rval().setUndefined(); return true;
}
bool Check(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  int slot = FunctionSlot(args);
  auto* entry = ControlReceiver(cx, args.thisv(), "HTMLButtonElement");
  if (!entry) return false;
  {
    JSAutoRealm realm(cx, entry->owner->page);
    entry->owner->Observe("HTMLButtonElement", slot ? "reportValidity" : "checkValidity", int(ApiAccess::Call));
  }
  args.rval().setBoolean(true); return true;
}
bool CustomValidity(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  auto* entry = ControlReceiver(cx, args.thisv(), "HTMLButtonElement");
  if (!entry) return false;
  if (!argc) return TypeError(cx, "setCustomValidity requires an argument");
  DomString text;
  if (!ButtonString(cx, args.get(0), &text)) return false;
  {
    JSAutoRealm realm(cx, entry->owner->page);
    entry->owner->Observe("HTMLButtonElement", "setCustomValidity", int(ApiAccess::Call));
    if (!entry->owner->Check(entry->owner->model.SetCustomValidity(entry->key, text))) return false;
  }
  args.rval().setUndefined(); return true;
}
}
bool InstallButtonElement(State& state) {
  for (size_t i = 0; i < std::size(kProperties); ++i) {
    const auto& p = kProperties[i];
    if (!state.Accessor("HTMLButtonElement", p.name, Get, int32_t(i), p.set ? Set : nullptr)) return false;
  }
  return state.Method("HTMLButtonElement", "checkValidity", Check, 0, 0) &&
      state.Method("HTMLButtonElement", "reportValidity", Check, 0, 1) &&
      state.Method("HTMLButtonElement", "setCustomValidity", CustomValidity, 1, 0);
}
}
