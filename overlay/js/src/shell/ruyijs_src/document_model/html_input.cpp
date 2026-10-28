#include "document_model/html_input.h"
#include "js/Conversions.h"
#include "js/Realm.h"
#include <cmath>

namespace ruyijs::document_model {
namespace {
enum Kind { Text, Boolean, Checked, Numeric, Null, Indeterminate, Zero, Direction, Will, Validity, Message, Number, DefaultChecked, Form, Files };
struct Property { const char* name; const char16_t* attr; Kind kind; bool set; };
const Property kProperties[] = {
  {"accept",u"accept",Text,true},{"alt",u"alt",Text,true},{"autocomplete",u"autocomplete",Text,true},
  {"defaultChecked",u"checked",DefaultChecked,true},{"checked",u"checked",Checked,true},
  {"dirName",u"dirname",Text,true},{"disabled",u"disabled",Boolean,true},{"form",u"",Form,false},
  {"files",u"",Files,true},{"formAction",u"formaction",Text,true},{"formEnctype",u"formenctype",Text,true},
  {"formMethod",u"formmethod",Text,true},{"formNoValidate",u"formnovalidate",Boolean,true},
  {"formTarget",u"formtarget",Text,true},{"height",u"height",Numeric,true},{"indeterminate",u"",Indeterminate,true},
  {"list",u"",Null,false},{"max",u"max",Text,true},{"maxLength",u"maxlength",Numeric,true},
  {"min",u"min",Text,true},{"minLength",u"minlength",Numeric,true},{"multiple",u"multiple",Boolean,true},
  {"name",u"name",Text,true},{"pattern",u"pattern",Text,true},{"placeholder",u"placeholder",Text,true},
  {"readOnly",u"readonly",Boolean,true},{"required",u"required",Boolean,true},{"size",u"size",Numeric,true},
  {"src",u"src",Text,true},{"step",u"step",Text,true},{"type",u"type",Text,true},
  {"defaultValue",u"defaultValue",Text,true},{"value",u"value",Text,true},{"valueAsDate",u"",Null,true},
  {"valueAsNumber",u"valueAsNumber",Number,true},{"width",u"width",Numeric,true},
  {"willValidate",u"",Will,false},{"validity",u"",Validity,false},{"validationMessage",u"",Message,false},
  {"labels",u"",Null,false},{"selectionStart",u"",Zero,true},{"selectionEnd",u"",Zero,true},
  {"selectionDirection",u"",Direction,true},{"align",u"align",Text,true},{"useMap",u"usemap",Text,true},
  {"webkitdirectory",u"webkitdirectory",Boolean,true}
};
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto& p = kProperties[FunctionSlot(args)];
  auto* entry = ControlReceiver(cx, args.thisv(), "HTMLInputElement");
  if (!entry) return false;
  auto& state = *entry->owner;
  JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe("HTMLInputElement", p.name, int(ApiAccess::Read));
    DomString text; std::optional<DomString> attr;
    if (p.kind == Null) result.setNull();
    else if (p.kind == Files) { if (!InputFiles(*entry, &result)) return false; }
    else if (p.kind == Form) { if (!ControlForm(*entry, &result)) return false; }
    else if (p.kind == Checked || p.kind == Indeterminate) {
      bool value;
      if (!state.Check(state.model.InputChecked(entry->key, &value, p.kind == Indeterminate))) return false;
      result.setBoolean(value);
    }
    else if (p.kind == Zero) result.setInt32(0);
    else if (p.kind == Numeric || p.kind == Number) {
      double number;
      if (!state.Check(state.model.InputNumber(entry->key, p.attr, &number))) return false;
      result.setNumber(number);
    } else if (p.kind == Will) {
      bool will;
      if (!state.Check(state.model.InputWillValidate(entry->key, &will))) return false;
      result.setBoolean(will);
    } else if (p.kind == Validity) {
      if (!ControlValidity(*entry, &result)) return false;
    } else if (p.kind == Boolean || p.kind == DefaultChecked) {
      if (!state.Check(p.kind == DefaultChecked ? state.model.GetAttributeNS(entry->key, {}, p.attr, &attr)
                        : state.model.GetAttribute(entry->key, p.attr, &attr))) return false;
      result.setBoolean(attr.has_value());
    } else {
      if (p.kind == Text && !state.Check(state.model.InputText(entry->key, p.attr, &text))) return false;
      if (p.kind == Direction) text = u"none";
      uint32_t flags;
      if (p.kind == Message && !state.Check(state.model.ControlValidation(entry->key, &flags, &text))) return false;
      if (!Value(cx, DomStringView(text), &result)) return false;
    }
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto& p = kProperties[FunctionSlot(args)];
  auto* entry = ControlReceiver(cx, args.thisv(), "HTMLInputElement");
  if (!entry) return false;
  auto& state = *entry->owner;
  DomString text; bool present = true, apply = true;
  if (p.kind == Boolean || p.kind == Checked || p.kind == DefaultChecked || p.kind == Indeterminate)
    present = JS::ToBoolean(args.get(0));
  else if (p.kind == Text || p.kind == Direction) {
    if (DomStringView(p.attr) == u"value" && args.get(0).isNull()) {
      text.clear();
    } else if (DomStringView(p.attr) == u"value" || DomStringView(p.attr) == u"defaultValue" ||
               (DomStringView(p.attr) == u"type" && args.get(0).isNumber())) {
      if (!String(cx, args.get(0), &text)) return false;
    } else if (!ControlString(cx, args.get(0), &text)) return false;
  } else if (p.kind == Numeric) {
    const double number = args.get(0).isNumber() ? args[0].toNumber() : -1;
    const bool length = DomStringView(p.attr) == u"maxlength" || DomStringView(p.attr) == u"minlength";
    apply = std::isfinite(number) && std::trunc(number) == number && number >= 0 &&
        number <= (length ? double(INT32_MAX) : double(UINT32_MAX)) &&
        (DomStringView(p.attr) != u"size" || number > 0);
    if (apply && !String(cx, args[0], &text)) return false;
  }
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe("HTMLInputElement", p.name, int(ApiAccess::Write));
    if (p.kind == Files) {
      if (!SetInputFiles(*entry, args.get(0))) return false;
    } else if (p.kind == Checked || p.kind == Indeterminate) {
      if (!state.Check(state.model.SetInputChecked(entry->key, present, p.kind == Indeterminate))) return false;
    } else if (p.kind == DefaultChecked) {
      if (!state.Check(present ? state.model.SetAttributeNS(entry->key, {}, p.attr, {})
                              : state.model.RemoveAttributeNS(entry->key, {}, p.attr))) return false;
    } else if (p.kind == Number) {
      if (args.get(0).isNumber() && !state.Check(state.model.SetInputNumber(entry->key, args[0].toNumber(), false))) return false;
    } else if (p.kind == Text && DomStringView(p.attr) == u"value") {
      if (!state.Check(state.model.SetInputValue(entry->key, text))) return false;
      if (text.empty() && !ResetInputFiles(*entry)) return false;
    } else if (apply && (p.kind == Text || p.kind == Boolean || p.kind == Numeric)) {
      DomStringView name(p.attr);
      if (name == u"defaultValue") name = u"value";
      if (name == u"type" && !ResetInputFiles(*entry)) return false;
      if (!state.Check(present ? state.model.SetAttribute(entry->key, name, text)
                              : state.model.RemoveAttribute(entry->key, name))) return false;
    }
  }
  args.rval().setUndefined(); return true;
}
}
bool InstallInputElement(State& state) {
  for (size_t i = 0; i < std::size(kProperties); ++i) {
    const auto& p = kProperties[i];
    if (!state.Accessor("HTMLInputElement", p.name, Get, int32_t(i), p.set ? Set : nullptr)) return false;
  }
  return InstallInputMethods(state);
}
}
