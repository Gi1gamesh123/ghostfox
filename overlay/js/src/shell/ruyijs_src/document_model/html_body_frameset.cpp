#include "document_model/html_control.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#include <iterator>
#include <string>

namespace ruyijs::document_model {
namespace {
// Reflected string attributes of HTMLBodyElement / HTMLFrameSetElement. The
// five body color/text attributes are [LegacyNullToEmptyString] (null writes
// the empty string, undefined is stringified); the others are plain DOMString.
struct Reflected { const char* iface; const char* name; const char16_t* attr; bool null_empty; };
constexpr Reflected kReflected[] = {
  {"HTMLBodyElement", "text", u"text", true},
  {"HTMLBodyElement", "link", u"link", true},
  {"HTMLBodyElement", "vLink", u"vlink", true},
  {"HTMLBodyElement", "aLink", u"alink", true},
  {"HTMLBodyElement", "bgColor", u"bgcolor", true},
  {"HTMLBodyElement", "background", u"background", false},
  {"HTMLFrameSetElement", "cols", u"cols", false},
  {"HTMLFrameSetElement", "rows", u"rows", false},
};
// The WindowEventHandlers mixin members Firefox exposes on both interfaces,
// in binding order (onpagereveal is pref-disabled and therefore absent).
constexpr const char* kHandlerNames[] = {
  "onafterprint", "onbeforeprint", "onbeforeunload", "onhashchange",
  "onlanguagechange", "onmessage", "onmessageerror", "onoffline", "ononline",
  "onpagehide", "onpageshow", "onpopstate", "onrejectionhandled", "onstorage",
  "onunhandledrejection", "onunload", "ongamepadconnected", "ongamepaddisconnected"
};
bool ReflectedGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto& m = kReflected[FunctionSlot(args)];
  auto* entry = ControlReceiver(cx, args.thisv(), m.iface, ("get " + std::string(m.name)).c_str());
  if (!entry) return false;
  auto& state = *entry->owner;
  JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe(m.iface, m.name, int(ApiAccess::Read));
    std::optional<DomString> attr;
    if (!state.Check(state.model.GetAttribute(entry->key, m.attr, &attr))) return false;
    if (!Value(cx, DomStringView(attr.value_or(u"")), &result)) return false;
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}
bool ReflectedSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto& m = kReflected[FunctionSlot(args)];
  auto* entry = ControlReceiver(cx, args.thisv(), m.iface, ("set " + std::string(m.name)).c_str());
  if (!entry) return false;
  auto& state = *entry->owner;
  DomString text;
  if (m.null_empty && args.get(0).isNull()) text.clear();
  else if (!ControlString(cx, args.get(0), &text)) return false;
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe(m.iface, m.name, int(ApiAccess::Write));
    if (!state.Check(state.model.SetAttribute(entry->key, m.attr, text))) return false;
  }
  args.rval().setUndefined(); return true;
}
// body/frameset on* members are window-reflecting: they read and write the
// Window's existing handler of the same name (the browser_environment owner's
// accessor on the window global), never a per-element store. A document with no
// browsing context reads null and ignores writes, matching Firefox.
bool HandlerGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const unsigned slot = unsigned(FunctionSlot(args));
  const char* iface = slot < std::size(kHandlerNames) ? "HTMLBodyElement" : "HTMLFrameSetElement";
  const char* name = kHandlerNames[slot % std::size(kHandlerNames)];
  auto* entry = ControlReceiver(cx, args.thisv(), iface, ("get " + std::string(name)).c_str());
  if (!entry) return false;
  auto& state = *entry->owner;
  state.Observe(iface, name, int(ApiAccess::Read));
  if (entry->key.document != state.main.document) { args.rval().setNull(); return true; }
  {
    JSAutoRealm realm(cx, state.page);
    if (!JS_GetProperty(cx, state.page, name, args.rval())) return false;
  }
  return JS_WrapValue(cx, args.rval());
}
bool HandlerSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const unsigned slot = unsigned(FunctionSlot(args));
  const char* iface = slot < std::size(kHandlerNames) ? "HTMLBodyElement" : "HTMLFrameSetElement";
  const char* name = kHandlerNames[slot % std::size(kHandlerNames)];
  auto* entry = ControlReceiver(cx, args.thisv(), iface, ("set " + std::string(name)).c_str());
  if (!entry) return false;
  auto& state = *entry->owner;
  state.Observe(iface, name, int(ApiAccess::Write));
  if (entry->key.document != state.main.document) { args.rval().setUndefined(); return true; }
  JS::RootedValue value(cx, args.get(0));
  {
    JSAutoRealm realm(cx, state.page);
    if (!JS_WrapValue(cx, &value) || !JS_SetProperty(cx, state.page, name, value)) return false;
  }
  args.rval().setUndefined(); return true;
}
}
bool InstallBodyFrameset(State& state) {
  for (size_t i = 0; i < std::size(kReflected); ++i)
    if (!state.Accessor(kReflected[i].iface, kReflected[i].name, ReflectedGet, int32_t(i),
                        ReflectedSet)) return false;
  for (size_t i = 0; i < std::size(kHandlerNames) * 2; ++i) {
    const char* iface = i < std::size(kHandlerNames) ? "HTMLBodyElement" : "HTMLFrameSetElement";
    if (!state.Accessor(iface, kHandlerNames[i % std::size(kHandlerNames)], HandlerGet,
                        int32_t(i), HandlerSet)) return false;
  }
  return true;
}
}
