// navigator.credentials (CredentialsContainer) and navigator.login
// (NavigatorLogin), following donor dom/credentialmanagement/
// CredentialsContainer.cpp (Get/Create/Store/PreventSilentAccess branches and
// messages) and LoginStatus.webidl. There is no authenticator, identity
// provider or digital-credential wallet: a publicKey request ends in the
// NotAllowedError Firefox reports when no authenticator answers, and other
// requests take the donor's NotSupportedError branch.
#include "web_credentials/internal.h"
#include "runtime/page_helpers.h"
#include "runtime/runtime.h"
#include "jsapi.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#include <cstring>
#include <string>

namespace ruyijs::web_credentials {
const JSClass kContainerClass = {"CredentialsContainer", 0};
const JSClass kLoginClass = {"NavigatorLogin", 0};
namespace {
enum Op { kGet = 0, kCreate, kStore, kPrevent, kSetStatus };
std::string Incompatible(const char* member, const char* iface) {
  return std::string("'") + member + "' called on an object that does not implement interface " + iface + ".";
}
bool Reject(State& s, JS::MutableHandleValue rval) {
  JS::RootedValue error(s.cx);
  if (!JS_GetPendingException(s.cx, &error)) return false;
  JS_ClearPendingException(s.cx);
  return s.kit.RejectWith(error, rval);
}
bool RejectDom(State& s, const char16_t* message, const char* name, JS::MutableHandleValue rval) {
  return ThrowDomException(s, message, name) || Reject(s, rval);
}
// Which of the request dictionary's credential members were passed.
bool Members(JSContext* cx, JS::HandleValue options, const char* prefix, bool creation, int* count,
             bool* publicKey) {
  *count = 0;
  *publicKey = false;
  if (options.isNullOrUndefined()) return true;
  if (!options.isObject())
    return WebKit::Fail(cx, (std::string(prefix) + "Argument 1 can't be converted to a dictionary.").c_str());
  JS::RootedObject dictionary(cx, &options.toObject());
  JS::RootedValue value(cx);
  for (const char* name : {"digital", "identity", "publicKey"}) {
    if (creation && !std::strcmp(name, "identity")) continue;
    if (!JS_GetProperty(cx, dictionary, name, &value)) return false;
    if (value.isUndefined()) continue;
    ++*count;
    if (!std::strcmp(name, "publicKey")) *publicKey = true;
  }
  return true;
}
bool Container(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  const int op = WebKit::Slot(args);
  static const char* const names[] = {"get", "create", "store", "preventSilentAccess"};
  JS::RootedObject self(cx);
  if (!WebKit::Check(cx, args.thisv(), &kContainerClass, &self, Incompatible(names[op], "CredentialsContainer").c_str()))
    return Reject(s, args.rval());
  if (op == kPrevent) return s.kit.Resolve(JS::UndefinedHandleValue, args.rval());
  if (op == kStore) {
    if (!argc) {
      (void)WebKit::Fail(cx, "CredentialsContainer.store: At least 1 argument required, but only 0 passed");
      return Reject(s, args.rval());
    }
    JSObject* object = args[0].isObject() ? js::CheckedUnwrapStatic(&args[0].toObject()) : nullptr;
    if (!object || JS::GetClass(object)->name != std::string("Credential")) {
      (void)WebKit::Fail(cx, "CredentialsContainer.store: Argument 1 does not implement interface Credential.");
      return Reject(s, args.rval());
    }
    return RejectDom(s, u"CredentialsContainer request is not supported.", "NotSupportedError", args.rval());
  }
  int count = 0;
  bool publicKey = false;
  const char* prefix = op == kGet ? "CredentialsContainer.get: " : "CredentialsContainer.create: ";
  if (!Members(cx, args.get(0), prefix, op == kCreate, &count, &publicKey)) return Reject(s, args.rval());
  if (count == 1 && publicKey)
    return RejectDom(s, u"The request is not allowed by the user agent or the platform in the current context, "
                        u"possibly because the user denied permission.", "NotAllowedError", args.rval());
  return RejectDom(s, u"CredentialsContainer request is not supported.", "NotSupportedError", args.rval());
}
bool SetStatus(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject self(cx);
  if (!WebKit::Check(cx, args.thisv(), &kLoginClass, &self, Incompatible("setStatus", "NavigatorLogin").c_str()))
    return Reject(s, args.rval());
  if (!argc) {
    (void)WebKit::Fail(cx, "NavigatorLogin.setStatus: At least 1 argument required, but only 0 passed");
    return Reject(s, args.rval());
  }
  JS::RootedString text(cx, JS::ToString(cx, args[0]));
  std::string status;
  if (!text || !ToUTF8(cx, text, &status)) return Reject(s, args.rval());
  if (status != "logged-in" && status != "logged-out") {
    (void)WebKit::Fail(cx, ("NavigatorLogin.setStatus: '" + status +
        "' (value of argument 1) is not a valid value for enumeration LoginStatus.").c_str());
    return Reject(s, args.rval());
  }
  return s.kit.Resolve(JS::UndefinedHandleValue, args.rval());
}
bool NavigatorGetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *static_cast<State*>(WebKit::Owner(args));
  const bool login = WebKit::Slot(args) == 1;
  JS::RootedValue navigator(cx);
  JSObject* self = args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr;
  {
    JSAutoRealm realm(cx, s.kit.page);
    if (!JS_GetProperty(cx, s.kit.page, "navigator", &navigator)) return false;
  }
  if (!self || !navigator.isObject() ||
      JS::GetClass(self) != JS::GetClass(js::UncheckedUnwrap(&navigator.toObject())))
    return WebKit::Fail(cx, Incompatible(login ? "get login" : "get credentials", "Navigator").c_str());
  JS::RootedValue value(cx, JS::ObjectValue(*(login ? s.login : s.container)));
  if (!JS_WrapValue(cx, &value)) return false;
  args.rval().set(value);
  return true;
}
}  // namespace

bool InstallNavigatorCredentials(State& s, JS::HandleObject navigatorPrototype) {
  JSContext* cx = s.cx;
  JS::RootedObject objectPrototype(cx, JS::GetRealmObjectPrototype(cx));
  if (!s.kit.Define("CredentialsContainer", &kContainerClass, objectPrototype, IllegalConstructor, &s.container_face) ||
      !s.kit.Method(s.container_face, "CredentialsContainer", "get", Container, 0, kGet) ||
      !s.kit.Method(s.container_face, "CredentialsContainer", "create", Container, 0, kCreate) ||
      !s.kit.Method(s.container_face, "CredentialsContainer", "store", Container, 1, kStore) ||
      !s.kit.Method(s.container_face, "CredentialsContainer", "preventSilentAccess", Container, 0, kPrevent) ||
      !s.kit.Define("NavigatorLogin", &kLoginClass, objectPrototype, IllegalConstructor, &s.login_face) ||
      !s.kit.Method(s.login_face, "NavigatorLogin", "setStatus", SetStatus, 1, 0)) return false;
  s.container = s.kit.New(&kContainerClass, s.container_face.prototype);
  s.login = s.kit.New(&kLoginClass, s.login_face.prototype);
  if (!s.container || !s.login) return false;
  for (int which = 0; which < 2; ++which) {
    const char* name = which ? "login" : "credentials";
    JS::RootedObject getter(cx, s.kit.Function(NavigatorGetter, which ? "get login" : "get credentials", 0, which)),
        setter(cx);
    if (!getter || !JS_DefineProperty(cx, navigatorPrototype, name, getter, setter, JSPROP_ENUMERATE) ||
        !s.kit.helpers->Accessor("Navigator", navigatorPrototype, name) ||
        !s.secure.Add(navigatorPrototype, name)) return false;
  }
  for (const char* name : {"CredentialsContainer", "NavigatorLogin", "Credential", "PublicKeyCredential",
                           "AuthenticatorResponse", "AuthenticatorAttestationResponse",
                           "AuthenticatorAssertionResponse"})
    if (!s.secure.Add(s.kit.page, name)) return false;
  return true;
}
}  // namespace ruyijs::web_credentials
