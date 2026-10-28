// ServiceWorkerContainer (dom/webidl/ServiceWorkerContainer.webidl) and
// navigator.serviceWorker, following donor ServiceWorkerContainer.cpp. URL
// validation is domain/service_worker/registration.h; with no service-worker
// runtime, a register() that passes it rejects with the SecurityError Firefox
// reports when the script cannot be installed.
#include "web_service_worker/internal.h"
#include "web_service_worker/scope.h"
#include "domain/service_worker/registration.h"
#include "browser_environment/environment.h"
#include "interaction/realm.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "runtime/runtime.h"
#include "jsapi.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#include <algorithm>
#include <cstdio>
#include <random>
#include <string>
#include "domain/browser_environment/url_value.h"
#include "js/Array.h"

namespace ruyijs::web_service_worker {
namespace sw = ::ruyijs::domain::service_worker;
const JSClass kContainerClass = {"ServiceWorkerContainer", JSCLASS_HAS_RESERVED_SLOTS(SlotCount)};
const char* const kHandlers[SlotCount] = {"controllerchange", "message", "messageerror"};



JSObject* Container(JSContext* cx, const JS::CallArgs& args, const char* member) {
  JS::RootedObject self(cx);
  if (!WebKit::Check(cx, args.thisv(), &kContainerClass, &self, Incompatible(member, "ServiceWorkerContainer").c_str()))
    return nullptr;
  return self;
}
bool Reject(State& s, JS::MutableHandleValue rval) {
  JS::RootedValue error(s.cx);
  if (!JS_GetPendingException(s.cx, &error)) return false;
  JS_ClearPendingException(s.cx);
  return s.kit.RejectWith(error, rval);
}
namespace {
bool MakeDom(State& s, const std::string& message, const char* name, JS::MutableHandleValue error) {
  JSContext* cx = s.cx;
  JS::RootedValue constructor(cx, JS::ObjectValue(*s.exception));
  JS::RootedString text(cx, JS_NewStringCopyUTF8N(cx, JS::UTF8Chars(message.data(), message.size()))),
      kind(cx, JS_NewStringCopyZ(cx, name));
  JS::RootedValueArray<2> values(cx);
  JS::RootedObject created(cx);
  if (!text || !kind) return false;
  values[0].setString(text);
  values[1].setString(kind);
  if (!JS::Construct(cx, constructor, values, &created)) return false;
  error.setObject(*created);
  return JS_WrapValue(cx, error);
}
}  // namespace
bool RejectDom(State& s, const std::string& message, const char* name, JS::MutableHandleValue rval) {
  JS::RootedValue error(s.cx);
  return MakeDom(s, message, name, &error) && s.kit.RejectWith(error, rval);
}
bool ThrowDom(State& s, const std::string& message, const char* name) {
  JS::RootedValue error(s.cx);
  if (MakeDom(s, message, name, &error)) JS_SetPendingException(s.cx, error);
  return false;
}
namespace {
bool Illegal(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  return trace.Finish(WebKit::Fail(cx, "Illegal constructor."));
}
bool Register(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  if (!Container(cx, args, "register")) return Reject(s, args.rval());
  const std::string prefix = "ServiceWorkerContainer.register: ";
  if (!argc) {
    (void)WebKit::Fail(cx, (prefix + "At least 1 argument required, but only 0 passed").c_str());
    return Reject(s, args.rval());
  }
  JS::RootedString scriptText(cx, JS::ToString(cx, args[0]));
  std::string script, scope, document;
  if (!scriptText || !ToUTF8(cx, scriptText, &script)) return Reject(s, args.rval());
  bool hasScope = false;
  if (!args.get(1).isNullOrUndefined()) {
    if (!args[1].isObject()) {
      (void)WebKit::Fail(cx, (prefix + "Argument 2 can't be converted to a dictionary.").c_str());
      return Reject(s, args.rval());
    }
    JS::RootedObject options(cx, &args[1].toObject());
    JS::RootedValue value(cx);
    // RegistrationOptions members in WebIDL (lexicographic) order.
    if (!JS_GetProperty(cx, options, "scope", &value)) return Reject(s, args.rval());
    if (!value.isUndefined()) {
      JS::RootedString text(cx, JS::ToString(cx, value));
      if (!text || !ToUTF8(cx, text, &scope)) return Reject(s, args.rval());
      hasScope = true;
    }
    if (!JS_GetProperty(cx, options, "type", &value) || !JS_GetProperty(cx, options, "updateViaCache", &value))
      return Reject(s, args.rval());
  }
  JS::RootedValue href(cx);
  if (!s.environment.URL(&href) || !href.isString()) return false;
  JS::RootedString hrefText(cx, href.toString());
  if (!ToUTF8(cx, hrefText, &document)) return false;
  const sw::RegisterCheck check = sw::CheckRegistration(prefix, script, hasScope, scope, document);
  if (check.kind == sw::RegisterCheck::TypeError) {
    (void)WebKit::Fail(cx, check.message.c_str());
    return Reject(s, args.rval());
  }
  if (check.kind == sw::RegisterCheck::SecurityError) return RejectDom(s, check.message, "SecurityError", args.rval());
  if (!s.workers || !s.registry) return RejectDom(s, "The operation is insecure.", "SecurityError", args.rval());
  // The max scope is the script's directory (no Service-Worker-Allowed header is read).
  const std::string maxScope = check.script.substr(0, check.script.rfind('/') + 1);
  if (check.scope.compare(0, maxScope.size(), maxScope) != 0) {
    return RejectDom(s, "The operation is insecure.", "SecurityError", args.rval());
  }
  std::string cache = "imports";
  if (args.get(1).isObject()) {
    JS::RootedObject options(cx, &args[1].toObject());
    JS::RootedValue value(cx);
    if (!JS_GetProperty(cx, options, "updateViaCache", &value)) return Reject(s, args.rval());
    if (!value.isUndefined()) {
      JS::RootedString text(cx, JS::ToString(cx, value));
      if (!text || !ToUTF8(cx, text, &cache)) return Reject(s, args.rval());
      if (cache != "imports" && cache != "all" && cache != "none") {
        (void)WebKit::Fail(cx, (prefix + "'" + cache + "' (value of 'updateViaCache' member of RegistrationOptions) "
                                "is not a valid value for enumeration ServiceWorkerUpdateViaCache.").c_str());
        return Reject(s, args.rval());
      }
    }
  }
  return StartRegister(s, check.scope, check.script, cache, args.rval());
}
bool Query(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  const bool all = WebKit::Slot(args) == 1;
  if (!Container(cx, args, all ? "getRegistrations" : "getRegistration")) return Reject(s, args.rval());
  if (!all) {
    std::string url = s.registry ? ClientUrl(s) : std::string();
    if (argc) {
      JS::RootedString text(cx, JS::ToString(cx, args[0]));
      std::string input;
      if (!text || !ToUTF8(cx, text, &input)) return Reject(s, args.rval());
      domain::browser_environment::UrlValue resolved;
      if (!input.empty() && domain::browser_environment::UrlValue::Resolve(input, url, &resolved)) url = resolved.href();
    }
    Registration* r = s.registry ? Match(*s.registry, url, false) : nullptr;
    JS::RootedValue value(cx);
    if (r && !RegistrationObject(s, *r, &value)) return false;
    return s.kit.Resolve(value, args.rval());
  }
  JS::RootedObject list(cx, JS::NewArrayObject(cx, 0));
  if (!list) return false;
  uint32_t index = 0;
  if (s.registry)
    for (auto& r : s.registry->list) {
      JS::RootedValue value(cx);
      if (r->removed) continue;
      if (!RegistrationObject(s, *r, &value) || !JS_SetElement(cx, list, index++, value)) return false;
    }
  JS::RootedValue value(cx, JS::ObjectValue(*list));
  return s.kit.Resolve(value, args.rval());
}
bool NavigatorServiceWorker(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedValue navigator(cx);
  JSObject* self = args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr;
  {
    JSAutoRealm realm(cx, s.kit.page);
    if (!JS_GetProperty(cx, s.kit.page, "navigator", &navigator)) return false;
  }
  if (!self || !navigator.isObject() ||
      JS::GetClass(self) != JS::GetClass(js::UncheckedUnwrap(&navigator.toObject())))
    return WebKit::Fail(cx, Incompatible("get serviceWorker", "Navigator").c_str());
  JS::RootedValue value(cx, JS::ObjectValue(*s.instance));
  if (!JS_WrapValue(cx, &value)) return false;
  args.rval().set(value);
  return true;
}
}  // namespace

Realm::Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers, interaction::Realm& events,
             browser_environment::Environment& environment, Realm* origin)
    : state_(std::make_unique<State>(cx, page, helpers, events, environment)) {
  state_->registry = origin ? origin->state_->registry : std::make_shared<Registry>();
}
Realm::~Realm() {
  State& s = *state_;
  delete s.scope;
  if (!s.registry) return;
  std::erase(s.registry->realms, &s);
  // Processes run by this realm's Web Workers owner end with it.
  for (auto& r : s.registry->list)
    if (r->home == &s) { r->process = 0; Remove(*s.registry, *r); r->home = nullptr; }
}
void Realm::SetClient(web_workers::Realm* workers, bool nested, bool (*parsed)(void*), void* context) {
  State& s = *state_;
  s.workers = workers;
  s.nested = nested;
  s.parsed = parsed;
  s.parsedContext = context;
  if (!workers) { std::erase(s.registry->realms, &s); return; }
  if (s.clientId.empty()) {
    std::random_device random;
    char text[37];
    uint32_t words[4] = {random(), random(), random(), random()};
    std::snprintf(text, sizeof(text), "%08x-%04x-4%03x-%04x-%04x%08x", words[0], words[1] >> 16,
                  words[1] & 0xfff, (words[2] >> 16 & 0x3fff) | 0x8000, words[2] & 0xffff, words[3]);
    s.clientId = text;
  }
  if (std::find(s.registry->realms.begin(), s.registry->realms.end(), &s) == s.registry->realms.end())
    s.registry->realms.push_back(&s);
}
void Realm::SetTaskPort(TaskPort port, void* context) {
  state_->task = port;
  state_->taskContext = context;
}

// A Worker realm (any kind) defines WorkerGlobalScope on its global.
bool WorkerRealm(State& s) {
  bool found = false;
  JS::RootedObject page(s.cx, s.kit.page);
  return JS_HasOwnProperty(s.cx, page, "WorkerGlobalScope", &found) && found;
}

bool Realm::Install() {
  State& s = *state_;
  JSContext* cx = s.cx;
  JSAutoRealm realm(cx, s.kit.page);
  JS::RootedValue exception(cx);
  if (!JS_GetProperty(cx, s.kit.page, "DOMException", &exception) || !exception.isObject())
    return WebKit::Fail(cx, "ServiceWorkerContainer requires the realm's DOMException");
  s.exception = &exception.toObject();
  JS::RootedObject target(cx, s.events.prototype());
  if (!target || !s.kit.Define("ServiceWorkerContainer", &kContainerClass, target, Illegal, &s.face) ||
      !s.kit.Method(s.face, "ServiceWorkerContainer", "register", Register, 1, 0) ||
      !s.kit.Method(s.face, "ServiceWorkerContainer", "getRegistration", Query, 0, 0) ||
      !s.kit.Method(s.face, "ServiceWorkerContainer", "getRegistrations", Query, 0, 1) ||
      !s.kit.Method(s.face, "ServiceWorkerContainer", "startMessages", StartMessages, 0, 0) ||
      !s.kit.Accessor(s.face, "ServiceWorkerContainer", "controller", Controller, nullptr, 0) ||
      !s.kit.Accessor(s.face, "ServiceWorkerContainer", "ready", Ready, nullptr, 0)) return false;
  for (int which = 0; which < SlotCount; ++which) {
    const std::string name = std::string("on") + kHandlers[which];
    if (!s.kit.Accessor(s.face, "ServiceWorkerContainer", name.c_str(), HandlerGet, HandlerSet, which)) return false;
  }
  s.instance = s.kit.New(&kContainerClass, s.face.prototype);
  s.ready = JS::NewPromiseObject(cx, nullptr);
  JS::RootedObject instance(cx, s.instance);
  if (!instance || !s.ready || !s.events.Attach(instance)) return false;
  for (int which = 0; which < SlotCount; ++which) JS::SetReservedSlot(instance, which, JS::NullValue());
  JS::RootedObject navigatorPrototype(cx, s.environment.navigatorPrototype());
  JS::RootedObject getter(cx, s.kit.Function(NavigatorServiceWorker, "get serviceWorker", 0, 0)), setter(cx);
  return navigatorPrototype && getter &&
         JS_DefineProperty(cx, navigatorPrototype, "serviceWorker", getter, setter, JSPROP_ENUMERATE) &&
         s.kit.helpers->Accessor("Navigator", navigatorPrototype, "serviceWorker") &&
         s.kit.helpers->Object("ServiceWorkerContainer", instance) &&
         s.secure.Add(navigatorPrototype, "serviceWorker") && s.secure.Add(s.kit.page, "ServiceWorkerContainer") &&
         InstallRegistration(s) && InstallPush(s) && (WorkerRealm(s) || InstallCookieManager(s));
}

bool Realm::ConfigureExposure() {
  State& s = *state_;
  JSAutoRealm realm(s.cx, s.kit.page);
  JS::RootedValue secure(s.cx);
  return JS_GetProperty(s.cx, s.kit.page, "isSecureContext", &secure) && s.secure.Apply(secure.isTrue());
}
}  // namespace ruyijs::web_service_worker
