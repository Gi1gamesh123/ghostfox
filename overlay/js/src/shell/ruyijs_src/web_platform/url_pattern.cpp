// URLPattern (docs/owners/WEB_PLATFORM.md, 2026-10-21). Parsing and matching are
// the Firefox urlpattern 0.6.0 crate behind vendor/urlpattern-native; this file
// is the WebIDL binding of dom/webidl/URLPattern.webidl with Firefox's
// overloads, conversion order, messages and exec() result shape.
#include "web_platform/state.h"
#include "web_platform/url_pattern_ffi.h"
#include "runtime/page_helpers.h"
#include "runtime/runtime.h"
#include "jsapi.h"
#include "js/CharacterEncoding.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"

namespace ruyijs::web_platform {
namespace {
void Finalize(JS::GCContext*, JSObject* object) {
  const JS::Value& slot = JS::GetReservedSlot(object, 0);
  if (!slot.isUndefined()) ruyijs_urlpattern_free(slot.toPrivate());
}
const JSClassOps kOps = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, Finalize};
const JSClass kUrlPatternClass = {"URLPattern", JSCLASS_HAS_RESERVED_SLOTS(1) | JSCLASS_FOREGROUND_FINALIZE, &kOps};
// WebIDL dictionary member order (lexicographic) and the RyInit field index.
const char* const kInitMembers[] = {"baseURL", "hash", "hostname", "password", "pathname", "port",
                                    "protocol", "search", "username"};
const unsigned kInitField[] = {8, 7, 3, 2, 5, 4, 0, 6, 1};
const char* const kComponents[] = {"protocol", "username", "password", "hostname", "port", "pathname",
                                   "search", "hash"};
struct Input {
  bool init = false;
  std::string text;
  std::optional<std::string> fields[9];
  RyInit Ry() const {
    RyInit out;
    for (unsigned i = 0; i < 9; ++i) out.fields[i] = fields[i] ? RyOf(*fields[i]) : RyNone();
    return out;
  }
};
bool Utf8(JSContext* cx, JS::HandleValue value, std::string* out) {
  JS::RootedString text(cx, JS::ToString(cx, value));
  return text && ToUTF8(cx, text, out);
}
// (UTF8String or URLPatternInit): null, undefined and objects are the dictionary.
bool ReadInput(JSContext* cx, JS::HandleValue value, Input* out) {
  if (!value.isNullOrUndefined() && !value.isObject()) return Utf8(cx, value, &out->text);
  out->init = true;
  if (value.isNullOrUndefined()) return true;
  JS::RootedObject object(cx, &value.toObject());
  JS::RootedValue member(cx);
  for (unsigned i = 0; i < 9; ++i) {
    if (!JS_GetProperty(cx, object, kInitMembers[i], &member)) return false;
    if (member.isUndefined()) continue;
    std::string text;
    if (!Utf8(cx, member, &text)) return false;
    out->fields[kInitField[i]] = std::move(text);
  }
  return true;
}
bool ReadIgnoreCase(JSContext* cx, JS::HandleValue value, unsigned position, bool* out) {
  *out = false;
  if (value.isNullOrUndefined()) return true;
  if (!value.isObject())
    return WebKit::Fail(cx, ("URLPattern constructor: Argument " + std::to_string(position) +
                             " can't be converted to a dictionary.").c_str());
  JS::RootedObject object(cx, &value.toObject());
  JS::RootedValue member(cx);
  if (!JS_GetProperty(cx, object, "ignoreCase", &member)) return false;
  *out = JS::ToBoolean(member);
  return true;
}
bool Construct(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  if (!args.isConstructing()) return WebKit::Fail(cx, "URLPattern constructor: 'new' is required");
  // Overload resolution: a non-dictionary second argument (or a third) selects
  // constructor(input, baseURL, options).
  const bool withBase = argc >= 3 || (argc == 2 && !args[1].isNullOrUndefined() && !args[1].isObject());
  Input input;
  std::string base;
  bool ignoreCase = false;
  if (!ReadInput(cx, args.get(0), &input) || (withBase && !Utf8(cx, args[1], &base)) ||
      !ReadIgnoreCase(cx, args.get(withBase ? 2 : 1), withBase ? 3 : 2, &ignoreCase)) return false;
  void* pattern = nullptr;
  {
    UrlPatternRegExpScope scope(cx, s.kit.page);
    if (!input.init) {
      pattern = ruyijs_urlpattern_parse_string(RyOf(input.text), withBase ? RyOf(base) : RyNone(), ignoreCase);
      if (!pattern)
        return WebKit::Fail(cx, withBase ? "URLPattern constructor: Failed to create URLPattern with base url (from string)"
                                         : "URLPattern constructor: Failed to create URLPattern (from string)");
    } else {
      if (withBase && !base.empty())
        return WebKit::Fail(cx, "URLPattern constructor: Should not provide base url with init");
      if (!withBase && input.fields[8] && input.fields[8]->empty())
        return WebKit::Fail(cx, "URLPattern constructor: Should not provide empty base url with init");
      const RyInit init = input.Ry();
      pattern = ruyijs_urlpattern_parse_init(&init, ignoreCase);
      if (!pattern)
        return WebKit::Fail(cx, withBase ? "URLPattern constructor: Failed to create URLPattern with base url (from init)"
                                         : "URLPattern constructor: Failed to create URLPattern (from init)");
    }
  }
  JS::RootedObject proto(cx), fallback(cx, s.urlPattern.prototype);
  if (!s.kit.ConstructProto(args, fallback, &proto)) { ruyijs_urlpattern_free(pattern); return false; }
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, &kUrlPatternClass, proto));
  if (!object) { ruyijs_urlpattern_free(pattern); return false; }
  JS::SetReservedSlot(object, 0, JS::PrivateValue(pattern));
  args.rval().setObject(*object);
  return true;
}
void* Receiver(JSContext* cx, const JS::CallArgs& args, const char* member) {
  JSObject* self = args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr;
  if (self && JS::GetClass(self) == &kUrlPatternClass && !JS::GetReservedSlot(self, 0).isUndefined())
    return JS::GetReservedSlot(self, 0).toPrivate();
  WebKit::Fail(cx, (std::string("'") + member + "' called on an object that does not implement interface URLPattern.").c_str());
  return nullptr;
}
bool Str(JSContext* cx, const std::string& text, JS::MutableHandleValue out) {
  JSString* s = JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(text.data(), text.size()));
  if (!s) return false;
  out.setString(s);
  return true;
}
bool Define(JSContext* cx, JS::HandleObject object, const char* name, JS::HandleValue value) {
  return JS_DefineProperty(cx, object, name, value, JSPROP_ENUMERATE);
}
// One component's {groups, input}; false in *matched when the component rejects.
bool Component(JSContext* cx, const void* component, const std::string& input, bool matchOnly, bool* matched,
               JS::MutableHandleValue out) {
  *matched = false;
  UrlPatternValues captures;
  if (ruyijs_urlpattern_component_regexp_empty(component)) {
    if (!input.empty()) return true;
  } else if (!ruyijs_urlpattern_component_matches(component, RyOf(input), matchOnly, &captures)) {
    return true;
  }
  *matched = true;
  if (matchOnly) return true;
  JS::RootedObject result(cx, JS_NewPlainObject(cx)), groups(cx, JS_NewPlainObject(cx));
  if (!result || !groups) return false;
  std::vector<std::string> names;
  const size_t count = ruyijs_urlpattern_component_group_count(component);
  for (size_t i = 0; i < count && i < captures.size(); ++i)
    names.push_back(RyString(ruyijs_urlpattern_component_group_name(component, i)));
  JS::RootedValue v(cx);
  for (size_t index : UrlPatternGroupOrder(names)) {
    v.setUndefined();
    if (captures[index] && !Str(cx, *captures[index], &v)) return false;
    if (!JS_DefineProperty(cx, groups, names[index].c_str(), v, JSPROP_ENUMERATE)) return false;
  }
  v.setObject(*groups);
  if (!Define(cx, result, "groups", v) || !Str(cx, input, &v) || !Define(cx, result, "input", v)) return false;
  out.setObject(*result);
  return true;
}
bool Match(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  const bool exec = WebKit::Slot(args) != 0;
  const char* method = exec ? "exec" : "test";
  void* pattern = Receiver(cx, args, method);
  if (!pattern) return false;
  Input input;
  std::string base;
  const bool basePassed = argc >= 2 && !args[1].isUndefined();
  if (!ReadInput(cx, args.get(0), &input) || (basePassed && !Utf8(cx, args[1], &base))) return false;
  if (input.init && basePassed)
    return WebKit::Fail(cx, (std::string("URLPattern.") + method +
                             ": Do not pass baseUrl separately with init, use init's baseURL property").c_str());
  const auto fail = [&] { args.rval().set(exec ? JS::NullValue() : JS::FalseValue()); return true; };
  UrlPatternRegExpScope scope(cx, s.kit.page);
  UrlPatternValues values;
  const RyInit init = input.Ry();
  const bool processed = input.init ? ruyijs_urlpattern_match_input_init(&init, &values)
                                    : ruyijs_urlpattern_match_input_string(RyOf(input.text),
                                                                           basePassed ? RyOf(base) : RyNone(), &values);
  if (!processed || values.size() != 8) return fail();
  JS::RootedValue parts[8] = {JS::RootedValue(cx), JS::RootedValue(cx), JS::RootedValue(cx), JS::RootedValue(cx),
                              JS::RootedValue(cx), JS::RootedValue(cx), JS::RootedValue(cx), JS::RootedValue(cx)};
  for (uint32_t i = 0; i < 8; ++i) {
    bool matched = false;
    if (!Component(cx, ruyijs_urlpattern_component(pattern, i), values[i].value_or(""), !exec, &matched, &parts[i]))
      return false;
    if (!matched) return fail();
  }
  if (!exec) { args.rval().setBoolean(true); return true; }
  // URLPatternResult: dictionary members in lexicographic order.
  JS::RootedObject result(cx, JS_NewPlainObject(cx)), inputs(cx, JS::NewArrayObject(cx, 0));
  if (!result || !inputs) return false;
  JS::RootedValue v(cx);
  if (input.init) {
    JS::RootedObject echo(cx, JS_NewPlainObject(cx));
    if (!echo) return false;
    for (unsigned i = 0; i < 9; ++i) {
      const auto& field = input.fields[kInitField[i]];
      if (field && (!Str(cx, *field, &v) || !Define(cx, echo, kInitMembers[i], v))) return false;
    }
    v.setObject(*echo);
    if (!JS_DefineElement(cx, inputs, 0, v, JSPROP_ENUMERATE)) return false;
  } else {
    if (!Str(cx, input.text, &v) || !JS_DefineElement(cx, inputs, 0, v, JSPROP_ENUMERATE)) return false;
    if (basePassed && (!Str(cx, base, &v) || !JS_DefineElement(cx, inputs, 1, v, JSPROP_ENUMERATE))) return false;
  }
  const unsigned order[] = {7, 3, 99, 2, 5, 4, 0, 6, 1};  // hash, hostname, inputs, password, ...
  for (unsigned index : order) {
    if (index == 99) { v.setObject(*inputs); if (!Define(cx, result, "inputs", v)) return false; continue; }
    if (!Define(cx, result, kComponents[index], parts[index])) return false;
  }
  args.rval().setObject(*result);
  return true;
}
bool Get(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int32_t slot = WebKit::Slot(args);
  const std::string member = std::string("get ") + (slot < 8 ? kComponents[slot] : "hasRegExpGroups");
  void* pattern = Receiver(cx, args, member.c_str());
  if (!pattern) return false;
  if (slot == 8) { args.rval().setBoolean(ruyijs_urlpattern_has_regexp_groups(pattern)); return true; }
  return Str(cx, RyString(ruyijs_urlpattern_component_pattern(ruyijs_urlpattern_component(pattern, uint32_t(slot)))),
             args.rval());
}
}  // namespace

bool InstallUrlPattern(State& state) {
  JSContext* cx = state.cx;
  WebKit& kit = state.kit;
  JS::RootedObject objectProto(cx, JS::GetRealmObjectPrototype(cx));
  if (!objectProto || !kit.Define("URLPattern", &kUrlPatternClass, objectProto, Construct, &state.urlPattern) ||
      !JS_DeleteProperty(cx, state.urlPattern.prototype, "constructor") ||
      !kit.Method(state.urlPattern, "URLPattern", "test", Match, 0, 0) ||
      !kit.Method(state.urlPattern, "URLPattern", "exec", Match, 0, 1)) return false;
  for (int32_t i = 0; i < 8; ++i)
    if (!kit.Accessor(state.urlPattern, "URLPattern", kComponents[i], Get, nullptr, i)) return false;
  return kit.Accessor(state.urlPattern, "URLPattern", "hasRegExpGroups", Get, nullptr, 8) &&
         JS_DefineProperty(cx, state.urlPattern.prototype, "constructor", state.urlPattern.constructor, 0);
}
}  // namespace ruyijs::web_platform
