#include "document_model/html_control.h"
#include "js/Conversions.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
namespace {
enum Kind { Text, Plain, Boolean, NodeText, Policy, Empty, Null, Dormant };
struct Member { const char* iface; const char* name; const char16_t* attr; Kind kind; bool set; };
const Member kMembers[] = {
  {"HTMLAnchorElement","target",u"target",Text,true},
  {"HTMLAnchorElement","download",u"download",Text,true},
  {"HTMLAnchorElement","ping",u"ping",Text,true},
  {"HTMLAnchorElement","rel",u"rel",Text,true},
  {"HTMLAnchorElement","hreflang",u"hreflang",Text,true},
  {"HTMLAnchorElement","type",u"type",Text,true},
  {"HTMLAnchorElement","referrerPolicy",u"referrerpolicy",Policy,true},
  {"HTMLAnchorElement","text",u"",NodeText,true},
  {"HTMLAnchorElement","coords",u"coords",Text,true},
  {"HTMLAnchorElement","charset",u"charset",Text,true},
  {"HTMLAnchorElement","name",u"name",Text,true},
  {"HTMLAnchorElement","rev",u"rev",Text,true},
  {"HTMLAnchorElement","shape",u"shape",Text,true},
  {"HTMLAreaElement","alt",u"alt",Plain,true},
  {"HTMLAreaElement","coords",u"coords",Plain,true},
  {"HTMLAreaElement","download",u"download",Plain,true},
  {"HTMLAreaElement","shape",u"shape",Plain,true},
  {"HTMLAreaElement","target",u"target",Plain,true},
  {"HTMLAreaElement","ping",u"ping",Plain,true},
  {"HTMLAreaElement","rel",u"rel",Plain,true},
  {"HTMLAreaElement","relList",u"",Null,true},
  {"HTMLAreaElement","referrerPolicy",u"referrerpolicy",Plain,true},
  {"HTMLAreaElement","noHref",u"nohref",Boolean,true},
  {"HTMLAreaElement","origin",u"",Empty,false},
  {"HTMLAreaElement","protocol",u"",Empty,true},
  {"HTMLAreaElement","username",u"",Empty,true},
  {"HTMLAreaElement","password",u"",Empty,true},
  {"HTMLAreaElement","host",u"",Empty,true},
  {"HTMLAreaElement","hostname",u"",Empty,true},
  {"HTMLAreaElement","port",u"",Empty,true},
  {"HTMLAreaElement","pathname",u"",Empty,true},
  {"HTMLAreaElement","search",u"",Empty,true},
  {"HTMLAreaElement","hash",u"",Empty,true},
  {"HTMLAreaElement","href",u"href",Plain,true},
  {"HTMLMapElement","name",u"",Dormant,true},
  {"HTMLMapElement","areas",u"",Dormant,false}
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
    if (m.kind == Dormant) return TypeError(cx, "Public map member has no initialized implementation");
    std::optional<DomString> attr; DomString text;
    if (m.kind == Null) result.setNull();
    else if (m.kind == Boolean) {
      if (!state.Check(state.model.GetAttribute(entry->key, m.attr, &attr))) return false;
      result.setBoolean(attr.has_value());
    } else {
      if (m.kind == Policy) { if (!state.Check(state.model.AnchorPolicy(entry->key, &text))) return false; }
      else if (m.kind != Empty) {
        auto status = m.kind == NodeText ? state.model.TextContent(entry->key, &attr)
                                        : state.model.GetAttribute(entry->key, m.attr, &attr);
        if (!state.Check(status)) return false;
        text = attr.value_or(u"");
      }
      if (!Value(cx, DomStringView(text), &result)) return false;
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
  else if (m.kind == Plain) { if (!String(cx, args.get(0), &text)) return false; }
  else if ((m.kind == Text || m.kind == NodeText || m.kind == Policy) && !ControlString(cx, args.get(0), &text)) return false;
  auto& state = *entry->owner;
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe(m.iface, m.name, int(ApiAccess::Write));
    if (m.kind == Dormant) return TypeError(cx, "Public map member has no initialized implementation");
    if (m.kind != Empty && m.kind != Null) {
      auto status = m.kind == NodeText ? state.model.SetTextContent(entry->key, text)
          : m.kind == Policy ? state.model.SetAnchorPolicy(entry->key, text)
          : present ? state.model.SetAttribute(entry->key, m.attr, text)
                    : state.model.RemoveAttribute(entry->key, m.attr);
      if (!state.Check(status)) return false;
    }
  }
  args.rval().setUndefined(); return true;
}
}
bool InstallHyperlinkMembers(State& state) {
  for (size_t i = 0; i < std::size(kMembers); ++i) {
    const auto& m = kMembers[i];
    if (!state.Accessor(m.iface, m.name, Read, int32_t(i), m.set ? Write : nullptr)) return false;
  }
  return true;
}
}
