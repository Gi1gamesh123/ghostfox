// ServiceWorkerGlobalScope (dom/webidl/ServiceWorkerGlobalScope.webidl) in a
// service-kind worker process, and the registry frames that drive it.
#include "web_service_worker/scope.h"
#include "web_workers/child.h"
#include "web_workers/clone.h"
#include "interaction/realm.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/Object.h"
#include "js/Array.h"
#include "js/CallAndConstruct.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"

namespace ruyijs::web_service_worker {
namespace pt = protocol;
namespace {
// Firefox prototype order (trace 20261022-181949, worker 23924).
const char* const kHandlerNames[] = {"install", "activate", "fetch", "message", "messageerror", "push",
                                     "pushsubscriptionchange", "notificationclick", "notificationclose"};
ScopeState& Self(const JS::CallArgs& args) { return *static_cast<ScopeState*>(WebKit::Owner(args)); }
bool Illegal(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  return trace.Finish(WebKit::Fail(cx, "Illegal constructor."));
}
bool Receiver(ScopeState& s, const JS::CallArgs& args, const char* member) {
  if (args.thisv().isNullOrUndefined() || (args.thisv().isObject() && &args.thisv().toObject() == s.owner.kit.page))
    return true;
  return WebKit::Fail(s.owner.cx, Incompatible(member, "ServiceWorkerGlobalScope").c_str());
}
bool Member(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  ScopeState& s = Self(args);
  const int slot = WebKit::Slot(args);
  const char* names[] = {"skipWaiting", "get clients", "get registration", "get cookieStore"};
  if (!Receiver(s, args, names[slot])) return false;
  if (slot == 0) return s.owner.kit.Resolve(JS::UndefinedHandleValue, args.rval());
  if (slot == 1) args.rval().setObject(*s.clientsObject);
  else if (slot == 2) args.rval().setObject(*s.registration);
  else args.rval().setNull();  // CookieStore is not installed in this realm
  return true;
}
// on<type> attributes: one listener per handler, added and removed through the
// global's own EventTarget, as Window EventHandler attributes do here.
bool Handler(JSContext* cx, const JS::CallArgs& args, bool setter) {
  ScopeState& s = Self(args);
  const char* type = kHandlerNames[WebKit::Slot(args)];
  const std::string member = std::string(setter ? "set on" : "get on") + type;
  if (!Receiver(s, args, member.c_str())) return false;
  JS::RootedObject store(cx, s.handlers);
  JS::RootedValue previous(cx);
  if (!JS_GetProperty(cx, store, type, &previous)) return false;
  if (!setter) { args.rval().set(previous.isUndefined() ? JS::NullValue() : previous); return true; }
  JS::RootedValue desired(cx, args.get(0));
  if (!desired.isObject() || !JS::IsCallable(&desired.toObject())) desired.setNull();
  JS::RootedObject target(cx, s.owner.events.prototype());
  JS::RootedValue function(cx), receiver(cx, JS::ObjectValue(*s.owner.kit.page)), ignored(cx);
  JS::RootedValueArray<2> parameters(cx);
  JS::RootedString name(cx, JS_NewStringCopyZ(cx, type));
  if (!name) return false;
  parameters[0].setString(name);
  for (int operation = 0; operation < 2; ++operation) {
    parameters[1].set(operation ? desired : previous);
    if (!parameters[1].isObject()) continue;
    if (!JS_GetProperty(cx, target, operation ? "addEventListener" : "removeEventListener", &function) ||
        !JS::Call(cx, receiver, function, parameters, &ignored)) return false;
  }
  args.rval().setUndefined();
  return JS_SetProperty(cx, store, type, desired);
}
bool ScopeHandlerGet(JSContext* cx, unsigned argc, JS::Value* vp) { return Handler(cx, JS::CallArgsFromVp(argc, vp), false); }
bool ScopeHandlerSet(JSContext* cx, unsigned argc, JS::Value* vp) { return Handler(cx, JS::CallArgsFromVp(argc, vp), true); }
// The SW realm's registration object mirrors the registry's last snapshot.
bool ApplyRegistration(ScopeState& s, pt::Reader& reader) {
  std::string scope, script, cache, states[3];
  if (!reader.Field(&scope) || !reader.Field(&script) || !reader.Field(&cache) ||
      !reader.Field(&states[0]) || !reader.Field(&states[1]) || !reader.Field(&states[2])) return true;
  JSContext* cx = s.owner.cx;
  JS::RootedObject registration(cx, s.registration), self(cx, s.self);
  auto text = [&](JS::HandleObject object, uint32_t slot, const std::string& value) {
    JSString* string = JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(value.data(), value.size()));
    if (string) JS::SetReservedSlot(object, slot, JS::StringValue(string));
    return string != nullptr;
  };
  bool ok = text(registration, 8, scope) && text(registration, 9, cache) && text(self, 1, script);
  for (int i = 0; i < 3 && ok; ++i) {
    JS::SetReservedSlot(registration, 4 + i, states[i].empty() ? JS::NullValue() : JS::ObjectValue(*self));
    if (!states[i].empty()) ok = text(self, 2, states[i]);
  }
  return ok;
}
bool Frame(void* context, std::string_view frame) {
  ScopeState& s = *static_cast<ScopeState*>(context);
  JSContext* cx = s.owner.cx;
  JSAutoRealm realm(cx, s.owner.kit.page);
  JS::RootedObject global(cx, s.owner.kit.page), lifetimes(cx);
  pt::Reader reader(frame);
  const char tag = frame.empty() ? '\0' : frame[0];
  if (tag == pt::kLifecycle) {
    std::string name;
    JS::RootedObject init(cx, JS_NewPlainObject(cx));
    if (!reader.Field(&name) || !init) return !init ? false : true;
    if (!s.owner.events.FireExtendable(global, name.c_str(), init, false, &lifetimes)) return false;
    return ReplyWhenSettled(s, lifetimes, name);
  }
  if (tag == pt::kMessage) {
    pt::Client client;
    std::string origin;
    if (!reader.Record(&client) || !reader.Field(&origin)) return true;
    JS::RootedValue data(cx), source(cx);
    JS::RootedObject init(cx, JS_NewPlainObject(cx)), ports(cx, JS::NewArrayObject(cx, 0));
    JS::RootedString originText(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(origin.data(), origin.size())));
    if (!init || !ports || !originText || !web_workers::DecodeMessage(cx, reader.Rest(), &data) ||
        !NewClient(s, client, &source) ||
        !JS_DefineProperty(cx, init, "data", data, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, init, "origin", originText, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, init, "source", source, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, init, "ports", ports, JSPROP_ENUMERATE)) return false;
    return s.owner.events.FireExtendable(global, "message", init, true, &lifetimes);
  }
  if (tag == pt::kClients) {
    std::string count;
    if (!reader.Field(&count)) return true;
    s.snapshot.assign(std::stoul(count), pt::Client{});
    for (auto& client : s.snapshot) if (!reader.Record(&client)) break;
    return true;
  }
  if (tag == pt::kRegistration) return ApplyRegistration(s, reader);
  return true;
}
}  // namespace

bool Realm::InstallServiceScope(web_workers::Child& child) {
  State& owner = *state_;
  JSContext* cx = owner.cx;
  JSAutoRealm realm(cx, owner.kit.page);
  auto* s = new ScopeState(owner, child);
  owner.scope = s;
  JS::RootedObject parent(cx, child.globalScopePrototype()), store(cx, JS_NewPlainObject(cx));
  if (!parent || !store || !s->kit.Define("ServiceWorkerGlobalScope", nullptr, parent, Illegal, &s->scope)) return false;
  s->handlers = store;
  JS::RootedValue parentCtor(cx);
  JS::RootedObject ctor(cx, s->scope.constructor);
  if (!JS_GetProperty(cx, parent, "constructor", &parentCtor) || !parentCtor.isObject()) return false;
  JS::RootedObject parentConstructor(cx, &parentCtor.toObject());
  bool immutable = false;
  if (!JS_SetPrototype(cx, ctor, parentConstructor) ||
      !JS_SetPrototype(cx, owner.kit.page, s->scope.prototype) ||
      !JS_SetImmutablePrototype(cx, owner.kit.page, &immutable) || !immutable) return false;
  // [Global]: the members are own properties of the global, its prototype holds only `constructor`.
  WebFace global(cx);
  global.prototype = owner.kit.page;
  if (!s->kit.Method(global, "ServiceWorkerGlobalScope", "skipWaiting", Member, 0, 0) ||
      !s->kit.Accessor(global, "ServiceWorkerGlobalScope", "clients", Member, nullptr, 1) ||
      !s->kit.Accessor(global, "ServiceWorkerGlobalScope", "registration", Member, nullptr, 2)) return false;
  for (int i = 0; i < 9; ++i) {
    const std::string name = std::string("on") + kHandlerNames[i];
    if (!s->kit.Accessor(global, "ServiceWorkerGlobalScope", name.c_str(), ScopeHandlerGet, ScopeHandlerSet, i)) return false;
  }
  if (!InstallCookieManager(owner) ||
      !s->kit.Accessor(global, "ServiceWorkerGlobalScope", "cookieStore", Member, nullptr, 3) ||
      !InstallClients(*s)) return false;
  s->registration = NewObject(owner, kRegistrationObject);
  s->self = NewObject(owner, kWorkerObject);
  if (!s->registration || !s->self) return false;
  JS::RootedObject preload(cx, NewObject(owner, kPreloadObject));
  if (!preload) return false;
  JS::SetReservedSlot(s->registration, 7, JS::ObjectValue(*preload));
  child.SetFramePort(&Frame, s);
  return true;
}
}  // namespace ruyijs::web_service_worker
