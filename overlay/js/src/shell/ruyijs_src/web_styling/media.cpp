// matchMedia and MediaQueryList (T17). The matches value is evaluated on every
// access from the original browsing context's viewport and screen sources,
// without a second display state or a change notification (D37).
#include "web_styling/condition_environment.h"
#include "js/Realm.h"
#include "web_styling/internal.h"

namespace ruyijs::web_styling {
namespace {
enum MediaSlot { kMediaQuery = 0, kMediaState = 1 };

State* MediaStateOf(JS::HandleObject self) {
  return static_cast<State*>(JS::GetReservedSlot(self, kMediaState).toPrivate());
}
bool MediaReceiver(JSContext* cx, const JS::CallArgs& args, JS::MutableHandleObject self) {
  return detail::CheckReceiver(cx, args.thisv(), &kMediaQueryClass, self,
                               "MediaQueryList member called on an incompatible receiver");
}
bool QueryOf(JSContext* cx, JS::HandleObject self, std::string* out) {
  JS::Value slot = JS::GetReservedSlot(self, kMediaQuery);
  if (!slot.isString()) { out->clear(); return true; }
  JS::RootedString text(cx, slot.toString());
  return detail::StringToAscii(cx, text, out);
}
bool Evaluate(State& state, const std::string& query, bool* out) {
  ruyijs::domain::styling::CssMediaEnvironment environment;
  if(!ReadConditionEnvironment(state,&environment)) return false;
  *out=ruyijs::domain::styling::CssMediaCondition(query,environment);
  return true;
}
bool MediaGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!MediaReceiver(cx, args, &self)) return false;
  std::string query;
  if (!QueryOf(cx, self, &query)) return false;
  JS::RootedString out(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(query.data(), query.size())));
  if (!out) return false;
  args.rval().setString(out);
  return true;
}
bool MatchesGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!MediaReceiver(cx, args, &self)) return false;
  State& state = *MediaStateOf(self);
  std::string query;
  bool matches = false;
  if (!QueryOf(cx, self, &query) || !Evaluate(state, query, &matches)) return false;
  args.rval().setBoolean(matches);
  return true;
}
bool OnChangeGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!MediaReceiver(cx, args, &self)) return false;
  args.rval().setNull();
  return true;
}
bool OnChangeSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!MediaReceiver(cx, args, &self)) return false;
  args.rval().setUndefined();
  return true;
}
bool Noop(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!MediaReceiver(cx, args, &self)) return false;
  args.rval().setUndefined();
  return true;
}
bool MatchMedia(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  if(!argc) return WebKit::Fail(cx,"matchMedia requires a query");
  State& state = *detail::Self(args);
  bool is_page = args.thisv().isUndefined() ||
                 (args.thisv().isObject() && &args.thisv().toObject() == state.page);
  if (!is_page) return WebKit::Fail(cx, "matchMedia called on an incompatible receiver");
  std::string query;
  if (!detail::ToAscii(cx, args.get(0), &query)) return false;
  ruyijs::domain::styling::MediaListValue media;media.set(query);query=media.text();
  JS::RootedString text(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(query.data(), query.size())));
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, &kMediaQueryClass,
                                                        state.mediaQuery.prototype));
  if (!text || !object) return false;
  JS::SetReservedSlot(object, kMediaQuery, JS::StringValue(text));
  JS::SetReservedSlot(object, kMediaState, JS::PrivateValue(&state));
  args.rval().setObject(*object);
  return true;
}
}  // namespace

const JSClass kMediaQueryClass = {"MediaQueryList", JSCLASS_HAS_RESERVED_SLOTS(2)};

bool InstallMedia(State& state) {
  WebKit& kit = state.kit;
  JS::RootedObject plain(kit.cx, JS::GetRealmObjectPrototype(kit.cx));
  if (!kit.Define("MediaQueryList", &kMediaQueryClass, plain, WebKit::Reject, &state.mediaQuery))
    return false;
  WebFace& face = state.mediaQuery;
  return detail::AccessorOn(kit, face.prototype, "MediaQueryList", "media", MediaGet,
                            nullptr, 0) &&
         detail::AccessorOn(kit, face.prototype, "MediaQueryList", "matches", MatchesGet,
                            nullptr, 0) &&
         detail::AccessorOn(kit, face.prototype, "MediaQueryList", "onchange", OnChangeGet,
                            OnChangeSet, 0) &&
         detail::MethodOn(kit, face.prototype, "MediaQueryList", "addEventListener", Noop,
                          2, 0) &&
         detail::MethodOn(kit, face.prototype, "MediaQueryList", "removeEventListener",
                          Noop, 2, 0) &&
         detail::MethodOn(kit, face.prototype, "MediaQueryList", "addListener", Noop, 1,
                          0) &&
         detail::MethodOn(kit, face.prototype, "MediaQueryList", "removeListener", Noop,
                          1, 0) &&
         detail::MethodOn(kit, state.page, "Window", "matchMedia", MatchMedia, 1, 0);
}
}  // namespace ruyijs::web_styling
