#include "document_model/html_form.h"
#include "document_model/form_submission.h"
#include "js/Conversions.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
namespace {
enum Kind { Text, NoValidate, Elements, Length };
struct Property { const char* name; const char16_t* attr; Kind kind = Text; };
const Property kProperties[] = {
  {"acceptCharset",u"accept-charset"},{"action",u"action"},{"autocomplete",u"autocomplete"},
  {"enctype",u"enctype"},{"encoding",u"enctype"},{"method",u"method"},{"name",u"name"},
  {"noValidate",u"novalidate",NoValidate},{"target",u"target"},
  {"elements",u"",Elements},{"length",u"",Length}
};
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto& p = kProperties[FunctionSlot(args)];
  auto* entry = ControlReceiver(cx, args.thisv(), "HTMLFormElement");
  if (!entry) return false;
  auto& state = *entry->owner;
  JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe("HTMLFormElement", p.name, int(ApiAccess::Read));
    if (p.kind == Elements || p.kind == Length) {
      auto* list = FormCollection(state, entry->key);
      if (!list) return false;
      if (p.kind == Length) result.setNumber(double(list->record->items.size()));
      else result.setObject(*list->object);
    } else {
      DomString text;
      if (!state.Check(state.model.FormAttribute(entry->key, p.attr, &text))) return false;
      if (p.kind == NoValidate) {
        std::optional<DomString> attr;
        if (!state.Check(state.model.GetAttribute(entry->key, p.attr, &attr))) return false;
        result.setBoolean(attr.has_value());
      }
      else if (!Value(cx, DomStringView(text), &result)) return false;
    }
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto& p = kProperties[FunctionSlot(args)];
  auto* entry = ControlReceiver(cx, args.thisv(), "HTMLFormElement");
  if (!entry) return false;
  DomString text; bool present = true;
  if (p.kind == NoValidate) present = JS::ToBoolean(args.get(0));
  else if (!String(cx, args.get(0), &text)) return false;
  auto& state = *entry->owner;
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe("HTMLFormElement", p.name, int(ApiAccess::Write));
    if (!state.Check(present ? state.model.SetFormAttribute(entry->key, p.attr, text)
                            : state.model.RemoveAttribute(entry->key, p.attr))) return false;
  }
  args.rval().setUndefined(); return true;
}
const char* const kMethods[] = {"submit","requestSubmit","reset","checkValidity","reportValidity"};
bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  int op = FunctionSlot(args);
  auto* entry = ControlReceiver(cx, args.thisv(), "HTMLFormElement");
  if (!entry) return false;
  {
    JSAutoRealm realm(cx, entry->owner->page);
    entry->owner->Observe("HTMLFormElement", kMethods[op], int(ApiAccess::Call));
  }
  if (op <= 1) {
    if (!SubmitForm(*entry, args.get(0), op == 1)) return false;
    args.rval().setUndefined(); return true;
  }
  if (op == 2) {
    if (!ResetForm(*entry)) return false;
    args.rval().setUndefined();
  } else if (op >= 3) {
    JSAutoRealm realm(cx, entry->owner->page);
    bool valid;
    if (!ValidateForm(*entry, &valid)) return false;
    args.rval().setBoolean(valid);
  }
  else args.rval().setUndefined();
  return true;
}
}
bool InstallFormMembers(State& state) {
  for (size_t i = 0; i < std::size(kProperties); ++i) {
    const auto& p = kProperties[i];
    if (!state.Accessor("HTMLFormElement", p.name, Get, int32_t(i), p.kind <= NoValidate ? Set : nullptr)) return false;
  }
  for (int i = 0; i < int(std::size(kMethods)); ++i)
    if (!state.Method("HTMLFormElement", kMethods[i], Call, 0, i)) return false;
  return true;
}
}
