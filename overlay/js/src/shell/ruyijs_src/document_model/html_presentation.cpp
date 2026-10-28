#include "document_model/html_control.h"
#include "document_model/html_element_internal.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
namespace {
enum Kind { Translate, Boolean, Text, Popover, OuterText };
struct Property { const char* name; const char16_t* attribute; Kind kind; };
constexpr Property properties[] = {
  {"translate", u"translate", Translate}, {"inert", u"inert", Boolean},
  {"autofocus", u"autofocus", Boolean}, {"nonce", u"nonce", Text},
  {"inputMode", u"inputmode", Text}, {"enterKeyHint", u"enterkeyhint", Text},
  {"autocapitalize", u"autocapitalize", Text}, {"popover", u"popover", Popover},
  {"outerText", u"", OuterText}
};
constexpr const char* methods[] = {"showPopover", "hidePopover", "togglePopover"};
bool Read(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto& property = properties[FunctionSlot(args)];
  State& function = *FunctionState(args);
  auto* entry = HTMLReceiver(cx, args.thisv(), function, &args);
  if (!entry) return false;
  State& owner = *entry->owner;
  JS::RootedValue result(cx);
  {
    JSAutoRealm realm(cx, owner.page);
    owner.Observe("HTMLElement", property.name, int(ApiAccess::Read));
    std::optional<DomString> text;
    if (!owner.Check(property.kind == OuterText ? owner.model.TextContent(entry->key, &text)
        : owner.model.GetAttribute(entry->key, property.attribute, &text))) return false;
    if (property.kind == Translate) result.setBoolean(!text || *text != u"no");
    else if (property.kind == Boolean) result.setBoolean(text.has_value());
    else if (property.kind == Popover && (!text || text->empty())) result.setNull();
    else if (!Value(cx, text ? DomStringView(*text) : DomStringView(), &result)) return false;
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result); return true;
}
// Preserve public conversion side effects: an ordinary conversion error commits
// a fixed fallback before the original exception is returned to the caller.
// Product string limits and engine termination are not conversion fallbacks.
bool Convert(JSContext* cx, JS::HandleValue value, Kind kind, DomString* text, bool* failed) {
  if (value.isBigInt() || (kind == Popover && value.isNullOrUndefined()) ||
      (kind == OuterText && value.isNull())) { text->clear(); return true; }
  if (value.isNumber()) return ControlString(cx, value, text);
  JS::RootedString converted(cx, JS::ToString(cx, value));
  if (!converted) {
    if (!JS_IsExceptionPending(cx) || JS_IsThrowingOutOfMemory(cx)) return false;
    *text = value.isObject() ? u"[object Object]" : u"";
    *failed = true; return true;
  }
  JS::RootedValue string(cx, JS::StringValue(converted));
  return String(cx, string, text);
}
bool Write(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const auto& property = properties[FunctionSlot(args)];
  State& function = *FunctionState(args);
  auto* entry = HTMLReceiver(cx, args.thisv(), function, &args);
  if (!entry) return false;
  if (!argc) return TypeError(cx, "HTMLElement setter requires a value");
  DomString text;
  bool failed = false;
  const bool present = JS::ToBoolean(args[0]);
  if (property.kind == Translate) text = present ? u"yes" : u"no";
  else if (property.kind != Boolean && !Convert(cx, args[0], property.kind, &text, &failed)) return false;
  JS::AutoSaveExceptionState pending(cx);
  State& owner = *entry->owner;
  {
    JSAutoRealm realm(cx, owner.page);
    owner.Observe("HTMLElement", property.name, int(ApiAccess::Write));
    const auto status = property.kind == OuterText ? owner.model.SetTextContent(entry->key, text)
        : property.kind == Boolean && !present ? owner.model.RemoveAttribute(entry->key, property.attribute)
        : owner.model.SetAttribute(entry->key, property.attribute, text);
    if (!owner.Check(status)) return false;
  }
  args.rval().setUndefined(); return !failed;
}
bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int method = FunctionSlot(args);
  State& function = *FunctionState(args);
  auto* entry = HTMLReceiver(cx, args.thisv(), function, &args);
  if (!entry) return false;
  entry->owner->Observe("HTMLElement", methods[method], int(ApiAccess::Call));
  if (method == 2) args.rval().setBoolean(false);
  else args.rval().setUndefined();
  return true;
}
}
bool InstallHTMLPresentation(State& state) {
  for (int i = 0; i < int(std::size(properties)); ++i)
    if (!state.Accessor("HTMLElement", properties[i].name, Read, i, Write)) return false;
  for (int i = 0; i < int(std::size(methods)); ++i)
    if (!state.Method("HTMLElement", methods[i], Call, 0, i)) return false;
  return true;
}
}
