#include "document_model/html_textarea.h"
#include "js/Conversions.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
namespace {
enum Kind { Text, Boolean, Size, Length, Null, Type, Default, ValueText, Count, Will, Validity,
            Message, Labels, Start, End, Direction, Form };
struct Property { const char* name; const char16_t* attr; Kind kind; bool set; };
const Property kProperties[] = {
  {"autocomplete",u"autocomplete",Text,true},{"cols",u"cols",Size,true},
  {"dirName",u"dirname",Text,true},{"disabled",u"disabled",Boolean,true},{"form",u"",Form,false},
  {"maxLength",u"maxlength",Length,true},{"minLength",u"minlength",Length,true},
  {"name",u"name",Text,true},{"placeholder",u"placeholder",Text,true},{"readOnly",u"readonly",Boolean,true},
  {"required",u"required",Boolean,true},{"rows",u"rows",Size,true},{"wrap",u"wrap",Text,true},
  {"type",u"",Type,false},{"defaultValue",u"defaultValue",Default,true},{"value",u"value",ValueText,true},
  {"textLength",u"textLength",Count,false},{"willValidate",u"",Will,false},{"validity",u"",Validity,false},
  {"validationMessage",u"",Message,false},{"labels",u"",Labels,false},
  {"selectionStart",u"selectionStart",Start,true},{"selectionEnd",u"selectionEnd",End,true},
  {"selectionDirection",u"selectionDirection",Direction,true}
};
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto& p = kProperties[FunctionSlot(args)];
  auto* entry = ControlReceiver(cx, args.thisv(), "HTMLTextAreaElement");
  if (!entry) return false;
  auto& state = *entry->owner;
  JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe("HTMLTextAreaElement", p.name, int(ApiAccess::Read));
    DomString text; std::optional<DomString> attr;
    if (p.kind == Null) result.setNull();
    else if (p.kind == Form) { if (!ControlForm(*entry, &result)) return false; }
    else if (p.kind == Size || p.kind == Length || p.kind == Count || p.kind == Start || p.kind == End) {
      double number;
      if (!state.Check(state.model.TextareaNumber(entry->key, p.attr, &number))) return false;
      result.setNumber(number);
    } else if (p.kind == Boolean) {
      if (!state.Check(state.model.GetAttribute(entry->key, p.attr, &attr))) return false;
      result.setBoolean(attr.has_value());
    } else if (p.kind == Will) {
      bool will;
      if (!state.Check(state.model.TextareaWillValidate(entry->key, &will))) return false;
      result.setBoolean(will);
    } else if (p.kind == Validity) {
      if (!ControlValidity(*entry, &result)) return false;
    } else if (p.kind == Labels) {
      if (!TextareaLabels(state, &result)) return false;
    } else {
      if (p.kind == Type) text = u"textarea";
      else if (p.kind == Message) {
        if (!state.Check(state.model.CustomValidity(entry->key, &text))) return false;
      } else if (!state.Check(state.model.TextareaText(entry->key, p.attr, &text))) return false;
      if (!Value(cx, DomStringView(text), &result)) return false;
    }
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto& p = kProperties[FunctionSlot(args)];
  auto* entry = ControlReceiver(cx, args.thisv(), "HTMLTextAreaElement");
  if (!entry) return false;
  auto& state = *entry->owner;
  DomString text; bool present = true; uint32_t number = 0;
  if (p.kind == Boolean) present = JS::ToBoolean(args.get(0));
  else if (p.kind == Size || p.kind == Start || p.kind == End) {
    if (!JS::ToUint32(cx, args.get(0), &number)) return false;
    if (p.kind == Size) {
      JS::RootedValue numeric(cx, JS::NumberValue(number ? number : 1));
      if (!String(cx, numeric, &text)) return false;
    }
  } else if (p.kind == Length) {
    int32_t length;
    if (!JS::ToInt32(cx, args.get(0), &length)) return false;
    if (length < 0) return state.Check({ErrorCode::IndexSize,"Textarea length must be nonnegative"});
    JS::RootedValue numeric(cx, JS::Int32Value(length));
    if (!String(cx, numeric, &text)) return false;
  } else if (!(p.kind == ValueText && args.get(0).isNull()) && !ControlString(cx, args.get(0), &text)) return false;
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe("HTMLTextAreaElement", p.name, int(ApiAccess::Write));
    if (p.kind == ValueText) {
      if (!state.Check(state.model.SetTextareaValue(entry->key, text))) return false;
    } else if (p.kind == Default) {
      if (!state.Check(state.model.SetTextContent(entry->key, text))) return false;
    } else if (p.kind == Start || p.kind == End || p.kind == Direction) {
      if (!state.Check(state.model.SetTextareaSelection(entry->key, number, number, text,
          p.kind == Start ? 0 : p.kind == End ? 1 : 2))) return false;
    } else if (!state.Check(present ? state.model.SetAttribute(entry->key, p.attr, text)
                                   : state.model.RemoveAttribute(entry->key, p.attr))) return false;
  }
  args.rval().setUndefined(); return true;
}
}
bool InstallTextAreaElement(State& state) {
  for (size_t i = 0; i < std::size(kProperties); ++i) {
    const auto& p = kProperties[i];
    if (!state.Accessor("HTMLTextAreaElement", p.name, Get, int32_t(i), p.set ? Set : nullptr)) return false;
  }
  return InstallTextareaMethods(state);
}
}
