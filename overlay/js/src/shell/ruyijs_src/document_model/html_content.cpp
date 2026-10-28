#include "document_model/html_control.h"
#include "js/Conversions.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
namespace {
enum Kind { Text, Plain, Boolean, Integer, Null, Dormant, StyleDisabled, StyleSheet };
struct Member { const char* iface; const char* name; const char16_t* attr; Kind kind; };
const Member kMembers[] = {
  {"HTMLTitleElement","text",u"",Dormant},
  {"HTMLStyleElement","disabled",u"",StyleDisabled},
  {"HTMLStyleElement","media",u"media",Text},
  {"HTMLStyleElement","type",u"type",Text},
  {"HTMLStyleElement","sheet",u"",StyleSheet},
  {"HTMLBRElement","clear",u"clear",Plain},
  {"HTMLDivElement","align",u"align",Plain},
  {"HTMLParagraphElement","align",u"align",Text},
  {"HTMLHRElement","align",u"",Dormant},
  {"HTMLHRElement","color",u"",Dormant},
  {"HTMLHRElement","noShade",u"",Dormant},
  {"HTMLHRElement","size",u"",Dormant},
  {"HTMLHRElement","width",u"",Dormant},
  {"HTMLHeadingElement","align",u"align",Text},
  {"HTMLPreElement","width",u"",Dormant},
  {"HTMLQuoteElement","cite",u"",Dormant},
  {"HTMLModElement","cite",u"",Dormant},
  {"HTMLModElement","dateTime",u"",Dormant},
  {"HTMLTimeElement","dateTime",u"",Dormant},
  {"HTMLDataElement","value",u"value",Plain},
  {"HTMLDListElement","compact",u"compact",Boolean},
  {"HTMLDirectoryElement","compact",u"compact",Boolean},
  {"HTMLMenuElement","compact",u"",Dormant},
  {"HTMLOListElement","reversed",u"reversed",Boolean},
  {"HTMLOListElement","start",u"start",Integer},
  {"HTMLOListElement","type",u"type",Text},
  {"HTMLOListElement","compact",u"compact",Boolean},
  {"HTMLUListElement","compact",u"compact",Boolean},
  {"HTMLUListElement","type",u"type",Plain},
  {"HTMLLIElement","value",u"value",Integer},
  {"HTMLFontElement","color",u"",Dormant},
  {"HTMLFontElement","face",u"",Dormant},
  {"HTMLFontElement","size",u"",Dormant},
  {"HTMLHtmlElement","version",u"version",Text},
  {"HTMLParamElement","name",u"",Dormant},
  {"HTMLParamElement","value",u"",Dormant},
  {"HTMLParamElement","type",u"",Dormant},
  {"HTMLParamElement","valueType",u"",Dormant}
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
    state.Observe(m.iface, m.name, int(ApiAccess::Read));
    if (m.kind == Dormant) return TypeError(cx, "Public content member has no initialized implementation");
    if (m.kind == StyleSheet || m.kind == StyleDisabled) {
      if (!StyleElementAccess(*entry, m.kind == StyleSheet ? StyleElementAction::Sheet :
            StyleElementAction::ReadDisabled, false, &result)) return false;
    } else if (m.kind == Null) result.setNull();
    else if (m.kind == Boolean) {
      std::optional<DomString> attr;
      if (!state.Check(state.model.GetAttribute(entry->key, m.attr, &attr))) return false;
      result.setBoolean(attr.has_value());
    } else if (m.kind == Integer) {
      int32_t value;
      if (!state.Check(state.model.ContentInteger(entry->key, m.attr, &value))) return false;
      result.setInt32(value);
    } else {
      DomString text;
      if (!state.Check(state.model.ContentAttribute(entry->key, m.attr, &text)) ||
          !Value(cx, DomStringView(text), &result)) return false;
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
  DomString text; bool present = true; int32_t number = 0;
  if (m.kind == Boolean || m.kind == StyleDisabled) present = JS::ToBoolean(args.get(0));
  else if (m.kind == Integer) { if (!JS::ToInt32(cx, args.get(0), &number)) return false; }
  else if (m.kind == Plain) { if (!String(cx, args.get(0), &text)) return false; }
  else if (m.kind == Text && !ControlString(cx, args.get(0), &text)) return false;
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe(m.iface, m.name, int(ApiAccess::Write));
    if (m.kind == Dormant) return TypeError(cx, "Public content member has no initialized implementation");
    if (m.kind == StyleDisabled) {
      JS::RootedValue result(cx);
      if (!StyleElementAccess(*entry, StyleElementAction::WriteDisabled, present, &result)) return false;
      args.rval().setUndefined();return true;
    }
    auto status = m.kind == Integer ? state.model.SetContentInteger(entry->key, m.attr, number)
        : !present ? state.model.RemoveAttribute(entry->key, m.attr)
                   : state.model.SetContentAttribute(entry->key, m.attr, text);
    if (!state.Check(status)) return false;
  }
  args.rval().setUndefined(); return true;
}
}
bool InstallContentElements(State& state) {
  for (size_t i = 0; i < std::size(kMembers); ++i) {
    const auto& m = kMembers[i];
    if (!state.Accessor(m.iface, m.name, Read, int32_t(i),
                        m.kind == Null || m.kind == StyleSheet ? nullptr : Write)) return false;
  }
  return true;
}
}
