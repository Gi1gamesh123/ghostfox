#include "document_model/html_control.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
namespace {
enum Kind { Text, Url, Boolean, Null, ScriptText, BaseString, LinkCors, LinkSheet };
struct Member { const char* iface; const char* name; const char16_t* attr; Kind kind; };
const Member kMembers[] = {
  {"HTMLLinkElement","disabled",u"disabled",Boolean},
  {"HTMLLinkElement","href",u"href",Url},
  {"HTMLLinkElement","crossOrigin",u"crossorigin",LinkCors},
  {"HTMLLinkElement","rel",u"rel",Text},
  {"HTMLLinkElement","relList",u"",Null},
  {"HTMLLinkElement","media",u"media",Text},
  {"HTMLLinkElement","hreflang",u"hreflang",Text},
  {"HTMLLinkElement","type",u"type",Text},
  {"HTMLLinkElement","as",u"as",Text},
  {"HTMLLinkElement","referrerPolicy",u"referrerpolicy",Text},
  {"HTMLLinkElement","sizes",u"",Null},
  {"HTMLLinkElement","fetchPriority",u"fetchpriority",Text},
  {"HTMLLinkElement","imageSrcset",u"imagesrcset",Text},
  {"HTMLLinkElement","imageSizes",u"imagesizes",Text},
  {"HTMLLinkElement","charset",u"charset",Text},
  {"HTMLLinkElement","rev",u"rev",Text},
  {"HTMLLinkElement","target",u"target",Text},
  {"HTMLLinkElement","sheet",u"",LinkSheet},
  {"HTMLLinkElement","integrity",u"integrity",Text},
  {"HTMLBaseElement","href",u"href",BaseString},
  {"HTMLBaseElement","target",u"target",BaseString},
  {"HTMLScriptElement","src",u"src",Url},
  {"HTMLScriptElement","type",u"type",Text},
  {"HTMLScriptElement","noModule",u"nomodule",Boolean},
  {"HTMLScriptElement","charset",u"charset",Text},
  {"HTMLScriptElement","async",u"async",Boolean},
  {"HTMLScriptElement","defer",u"defer",Boolean},
  {"HTMLScriptElement","crossOrigin",u"crossorigin",Text},
  {"HTMLScriptElement","text",u"",ScriptText},
  {"HTMLScriptElement","textContent",u"",ScriptText},
  {"HTMLScriptElement","referrerPolicy",u"referrerpolicy",Text},
  {"HTMLScriptElement","fetchPriority",u"fetchpriority",Text},
  {"HTMLScriptElement","event",u"event",Text},
  {"HTMLScriptElement","htmlFor",u"for",Text},
  {"HTMLScriptElement","integrity",u"integrity",Text}
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
    std::optional<DomString> text;
    if (m.kind == LinkSheet) {
      if (!StyleElementAccess(*entry, StyleElementAction::Sheet, false, &result)) return false;
    } else if (m.kind == Null) result.setNull();
    else if (m.kind == Boolean) {
      if (DomStringView(m.attr)==u"async") {
        bool value=false;
        if(!state.Check(state.model.ScriptAsync(entry->key,&value)))return false;
        result.setBoolean(value);
      } else {
      if (!state.Check(state.model.GetAttribute(entry->key, m.attr, &text))) return false;
      result.setBoolean(text.has_value());
      }
    } else if (m.kind == Url) {
      if (!state.Check(state.model.GetAttribute(entry->key, m.attr, &text)) ||
          !ReflectUrl(state, entry->key, text, &result)) return false;
    } else {
      auto status = m.kind == ScriptText ? state.model.TextContent(entry->key, &text)
                                        : state.model.HeadAttribute(entry->key, m.attr, &text);
      if (!state.Check(status) || !Value(cx, text, &result)) return false;
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
  DomString text; bool present = true;
  if (m.kind == Boolean) present = JS::ToBoolean(args.get(0));
  else if (m.kind == BaseString) { if (!String(cx, args.get(0), &text)) return false; }
  else if (!(m.kind == LinkCors && args.get(0).isNull()) && !ControlString(cx, args.get(0), &text)) return false;
  auto& state = *entry->owner;
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe(m.iface, m.name, int(ApiAccess::Write));
    auto status = DomStringView(m.attr)==u"async" ? state.model.SetScriptAsync(entry->key,present)
        : m.kind == ScriptText ? state.model.SetTextContent(entry->key, text)
        : present ? state.model.SetAttribute(entry->key, m.attr, text)
                  : state.model.RemoveAttribute(entry->key, m.attr);
    if (!state.Check(status)) return false;
  }
  args.rval().setUndefined(); return true;
}
bool Supports(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  state.Observe("HTMLScriptElement", "supports", int(ApiAccess::Call));
  DomString type; bool supported = false;
  if (!ControlString(cx, args.get(0), &type) || !state.Check(state.model.SupportsScriptType(type, &supported))) return false;
  args.rval().setBoolean(supported); return true;
}
}
bool InstallHeadElements(State& state) {
  for (size_t i = 0; i < std::size(kMembers); ++i) {
    const auto& m = kMembers[i];
    if (!state.Accessor(m.iface, m.name, Read, int32_t(i),
                        m.kind == Null || m.kind == LinkSheet ? nullptr : Write)) return false;
  }
  JS::RootedObject target(state.cx, state.Find("HTMLScriptElement")->constructor);
  JS::RootedObject supports(state.cx, state.Function(Supports, "supports", 1, 0));
  return supports && JS_DefineProperty(state.cx, target, "supports", supports, JSPROP_ENUMERATE);
}
}
