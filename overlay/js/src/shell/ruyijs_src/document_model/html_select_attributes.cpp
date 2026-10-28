#include <cstring>
#include "js/PropertyAndElement.h"
#include "document_model/html_select.h"
#include "js/Conversions.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
namespace {
enum Kind { StringAttr, BoolAttr, Null, Zero, MinusOne, Empty, Size, Autocomplete,
            Type, Selected, Text, TextFallback, Options, Validity, Message, WillValidate, Form };
struct Property { const char* iface; const char* name; const char16_t* attr; Kind kind; bool set; };
const Property kProperties[] = {
  {"HTMLSelectElement","autocomplete",u"autocomplete",Autocomplete,true},
  {"HTMLSelectElement","disabled",u"disabled",BoolAttr,true},
  {"HTMLSelectElement","length",u"",Zero,true},
  {"HTMLSelectElement","multiple",u"multiple",BoolAttr,true},
  {"HTMLSelectElement","name",u"name",StringAttr,true},
  {"HTMLSelectElement","required",u"required",BoolAttr,true},
  {"HTMLSelectElement","selectedIndex",u"",MinusOne,true},
  {"HTMLSelectElement","size",u"size",Size,true},
  {"HTMLSelectElement","value",u"",Empty,true},
  {"HTMLSelectElement","form",u"",Form,false},
  {"HTMLSelectElement","labels",u"",Null,false},
  {"HTMLSelectElement","options",u"",Options,false},
  {"HTMLSelectElement","selectedOptions",u"",Null,false},
  {"HTMLSelectElement","type",u"multiple",Type,false},
  {"HTMLSelectElement","validationMessage",u"",Message,false},
  {"HTMLSelectElement","validity",u"",Validity,false},
  {"HTMLSelectElement","willValidate",u"",WillValidate,false},
  {"HTMLOptionElement","defaultSelected",u"selected",BoolAttr,true},
  {"HTMLOptionElement","disabled",u"disabled",BoolAttr,true},
  {"HTMLOptionElement","label",u"label",TextFallback,true},
  {"HTMLOptionElement","selected",u"",Selected,true},
  {"HTMLOptionElement","text",u"",Text,true},
  {"HTMLOptionElement","value",u"value",TextFallback,true},
  {"HTMLOptionElement","form",u"",Form,false},
  {"HTMLOptionElement","index",u"",Zero,false},
  {"HTMLOptGroupElement","disabled",u"disabled",BoolAttr,true},
  {"HTMLOptGroupElement","label",u"label",StringAttr,true}
};
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto& p = kProperties[FunctionSlot(args)];
  auto* entry = ControlReceiver(cx, args.thisv(), p.iface);
  if (!entry) return false;
  auto& state = *entry->owner;
  JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe(p.iface, p.name, int(ApiAccess::Read));
    DomString text; std::optional<DomString> attr;
    bool flag = false; uint32_t number = 0;
    const bool select = !std::strcmp(p.iface, "HTMLSelectElement");
    if (select && (p.kind == Zero || p.kind == MinusOne || p.kind == Empty)) {
      JS::RootedValue options(cx), length(cx), item(cx), field(cx);
      if (!SelectOptions(*entry, &options) || !options.isObject()) return false;
      JS::RootedObject list(cx, &options.toObject());
      if (!JS_GetProperty(cx, list, "length", &length)) return false;
      const int32_t count = length.isInt32() ? length.toInt32() : 0;
      if (p.kind == Zero) { args.rval().setInt32(count); return true; }
      std::optional<DomString> multiple;
      uint32_t size = 0;
      if (!state.Check(state.model.GetAttribute(entry->key, u"multiple", &multiple)) ||
          !state.Check(state.model.SelectSize(entry->key, &size))) return false;
      int32_t chosen = -1, fallback = -1;
      for (int32_t i = 0; i < count && chosen < 0; ++i) {
        if (!JS_GetElement(cx, list, i, &item) || !item.isObject()) continue;
        JS::RootedObject option(cx, &item.toObject());
        if (!JS_GetProperty(cx, option, "selected", &field)) return false;
        if (field.isTrue()) { chosen = i; break; }
        if (fallback < 0) {
          if (!JS_GetProperty(cx, option, "disabled", &field)) return false;
          if (!field.isTrue()) fallback = i;
        }
      }
      // Selectedness: a single-line select-one shows its first enabled option.
      if (chosen < 0 && !multiple && size <= 1) chosen = fallback;
      if (p.kind == MinusOne) { args.rval().setInt32(chosen); return true; }
      if (chosen < 0) return Value(cx, DomStringView(), args.rval());
      if (!JS_GetElement(cx, list, chosen, &item) || !item.isObject()) return false;
      JS::RootedObject option(cx, &item.toObject());
      if (!JS_GetProperty(cx, option, "value", args.rval())) return false;
      return JS_WrapValue(cx, args.rval());
    }
    switch (p.kind) {
      case Null: result.setNull(); break;
      case Form: if (!ControlForm(*entry, &result)) return false; break;
      case Zero: result.setInt32(0); break;
      case MinusOne: result.setInt32(-1); break;
      case Empty: if (!Value(cx, DomStringView(), &result)) return false; break;
      case Options: if (!SelectOptions(*entry, &result)) return false; break;
      case Validity: if (!ControlValidity(*entry, &result)) return false; break;
      case Size:
        if (!state.Check(state.model.SelectSize(entry->key, &number))) return false;
        result.setNumber(number); break;
      case Selected:
        if (!state.Check(state.model.OptionSelected(entry->key, &flag))) return false;
        result.setBoolean(flag); break;
      case WillValidate:
        if (!SelectWillValidate(*entry, &flag)) return false;
        result.setBoolean(flag); break;
      case Message:
        if (!state.Check(state.model.CustomValidity(entry->key, &text)) ||
            !Value(cx, DomStringView(text), &result)) return false;
        break;
      case Text:
        if (!state.Check(state.model.OptionText(entry->key, &text)) ||
            !Value(cx, DomStringView(text), &result)) return false;
        break;
      default:
        if (!state.Check(state.model.GetAttribute(entry->key, p.attr, &attr))) return false;
        if (p.kind == BoolAttr) { result.setBoolean(attr.has_value()); break; }
        if (p.kind == Type) text = attr ? u"select-multiple" : u"select-one";
        else if (p.kind == TextFallback && !attr) {
          if (!state.Check(state.model.OptionText(entry->key, &text))) return false;
        } else text = attr.value_or(u"");
        if (!Value(cx, DomStringView(text), &result)) return false;
    }
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto& p = kProperties[FunctionSlot(args)];
  auto* entry = ControlReceiver(cx, args.thisv(), p.iface);
  if (!entry) return false;
  DomString text; bool flag = false; uint32_t number = 0; int32_t index = 0;
  if (p.kind == BoolAttr || p.kind == Selected) flag = JS::ToBoolean(args.get(0));
  else if (p.kind == Zero || p.kind == Size) {
    if (!JS::ToUint32(cx, args.get(0), &number)) return false;
  } else if (p.kind == MinusOne) {
    if (!JS::ToInt32(cx, args.get(0), &index)) return false;
  } else if (!String(cx, args.get(0), &text)) return false;
  auto& state = *entry->owner;
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe(p.iface, p.name, int(ApiAccess::Write));
    Status status;
    switch (p.kind) {
      case Zero: case MinusOne: case Empty: break;
      case BoolAttr: status = flag ? state.model.SetAttribute(entry->key, p.attr, u"")
                                  : state.model.RemoveAttribute(entry->key, p.attr); break;
      case Selected: status = state.model.SetOptionSelected(entry->key, flag); break;
      case Text: status = state.model.SetTextContent(entry->key, text); break;
      case Size: status = state.model.SetSelectSize(entry->key, number); break;
      case Autocomplete: status = state.model.SetSelectAutocomplete(entry->key, text); break;
      default: status = state.model.SetAttribute(entry->key, p.attr, text); break;
    }
    if (!state.Check(status)) return false;
  }
  args.rval().setUndefined(); return true;
}
}
bool InstallSelectAttributes(State& state) {
  for (size_t i = 0; i < std::size(kProperties); ++i) {
    const auto& p = kProperties[i];
    if (!state.Accessor(p.iface, p.name, Get, int32_t(i), p.set ? Set : nullptr)) return false;
  }
  return true;
}
}
