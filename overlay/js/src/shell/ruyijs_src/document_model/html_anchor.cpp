// DOM hyperlink reflection, with no JS URL object or shadow href state.
#include "document_model/state.h"
#include "document_model/html_control.h"
#include "document_model/model_hyperlink.h"
#include "browser_environment/environment.h"
#include "runtime/runtime.h"
#include "js/CharacterEncoding.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/String.h"
#include "js/Wrapper.h"
#include <cstring>

namespace ruyijs::document_model {
namespace {
const char* const kFields[] = {
    "href", "origin", "protocol", "host", "hostname", "port", "pathname", "search", "hash", "username", "password"};
Wrapper* AnchorReceiver(JSContext* cx, const JS::CallArgs& args) {
  JS::RootedObject object(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  Wrapper* entry = object && JS::GetClass(object) == &kNodeClass
      ? static_cast<Wrapper*>(JS::GetReservedSlot(object, 0).toPrivate()) : nullptr;
  if (!entry || entry->dead || !entry->owner) {
    TypeError(cx, "HTMLAnchorElement member called on incompatible receiver");
    return nullptr;
  }
  State& state = *entry->owner;
  JSAutoRealm realm(cx, state.page);
  NodeSnapshot node;
  if (!state.Check(state.model.Inspect(entry->key, &node))) return nullptr;
  if (std::strcmp(node.interface_name, "HTMLAnchorElement")) {
    TypeError(cx, "HTMLAnchorElement member called on incompatible receiver");
    return nullptr;
  }
  return entry;
}
bool Encode(JSContext* cx, DomStringView text, std::string* result) {
  JS::RootedValue value(cx);
  if (!Value(cx, text, &value)) return false;
  JS::RootedString string(cx, value.toString());
  return ToUTF8(cx, string, result);
}
bool Input(State& state, NodeKey node, std::optional<DomString>* raw,
           std::string* href, std::string* base) {
  if (!state.Check(state.model.GetAttribute(node, u"href", raw))) return false;
  if (!*raw) return true;
  NodeSnapshot element;
  DocumentSnapshot document;
  if (!state.Check(state.model.Inspect(node, &element)) ||
      !state.Check(state.model.InspectDocument(element.owner_document, &document))) return false;
  JS::RootedValue value(state.cx);
  if (document.url) {
    if (!Value(state.cx, DomStringView(*document.url), &value)) return false;
  } else if (!state.environment.URL(&value)) return false;
  JS::RootedString url(state.cx, value.toString());
  return ToUTF8(state.cx, url, base) && Encode(state.cx, **raw, href);
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Wrapper* entry = AnchorReceiver(cx, args);
  if (!entry) return false;
  State& state = *entry->owner;
  auto field = HyperlinkField(FunctionSlot(args));
  std::string result;
  {
    JSAutoRealm realm(cx, state.page);
    state.Observe("HTMLAnchorElement", kFields[int(field)], int(ApiAccess::Read));
    std::optional<DomString> raw;
    std::string href, base;
    if (!Input(state, entry->key, &raw, &href, &base)) return false;
    if (raw) {
      if (!state.Check(ReadHyperlink(href, base, field, &result))) return false;
    } else if (field == HyperlinkField::Protocol) result = ":";
  }
  JS::RootedString value(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(result.data(), result.size())));
  if (!value) return false;
  args.rval().setString(value);
  return true;
}
bool Set(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Wrapper* entry = AnchorReceiver(cx, args);
  if (!entry) return false;
  State& state = *entry->owner;
  auto field = HyperlinkField(FunctionSlot(args));
  DomString text;
  if (!(field == HyperlinkField::Username || field == HyperlinkField::Password
          ? ControlString(cx, args.get(0), &text) : String(cx, args.get(0), &text))) return false;
  JSAutoRealm realm(cx, state.page);
  state.Observe("HTMLAnchorElement", kFields[int(field)], int(ApiAccess::Write));
  if (field != HyperlinkField::Href) {
    std::optional<DomString> raw;
    std::string href, base, value, result;
    bool changed = false;
    if (!Input(state, entry->key, &raw, &href, &base) || !Encode(cx, text, &value)) return false;
    if (!raw) { args.rval().setUndefined(); return true; }
    if (!state.Check(WriteHyperlink(href, base, field, value, &result, &changed))) return false;
    if (!changed) { args.rval().setUndefined(); return true; }
    JS::RootedString encoded(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(result.data(), result.size())));
    JS::RootedValue decoded(cx, encoded ? JS::StringValue(encoded) : JS::UndefinedValue());
    if (!encoded || !String(cx, decoded, &text)) return false;
  }
  if (!state.Check(state.model.SetAttribute(entry->key, u"href", text))) return false;
  args.rval().setUndefined();
  return true;
}
}
bool InstallAnchorElement(State& state) {
  for (size_t index = 0; index < std::size(kFields); ++index)
    if (!state.Accessor("HTMLAnchorElement", kFields[index], Get, int32_t(index),
                        index == int(HyperlinkField::Origin) ? nullptr : Set)) return false;
  return state.Method("HTMLAnchorElement", "toString", Get, 0, int(HyperlinkField::Href)) &&
      InstallHyperlinkMembers(state);
}
}
