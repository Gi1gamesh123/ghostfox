// The one production installer of Credential, PublicKeyCredential,
// AuthenticatorResponse, AuthenticatorAttestationResponse and
// AuthenticatorAssertionResponse: interface objects with the donor's
// inheritance (prototype and constructor chains), prototype members in the
// donor's order (operations, then attributes) and the static operations.
#include "web_credentials/internal.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include <string>

namespace ruyijs::web_credentials {
namespace {
const JSClass kClasses[FaceCount] = {
    {"Credential", 0}, {"PublicKeyCredential", 0}, {"AuthenticatorResponse", 0},
    {"AuthenticatorAttestationResponse", 0}, {"AuthenticatorAssertionResponse", 0}};
struct MemberDef {
  Interface face;
  const char* name;
  bool attribute;
};
constexpr MemberDef kMembers[] = {
    {CredentialFace, "id", true}, {CredentialFace, "type", true},
    {PublicKeyFace, "getClientExtensionResults", false}, {PublicKeyFace, "toJSON", false},
    {PublicKeyFace, "rawId", true}, {PublicKeyFace, "response", true},
    {PublicKeyFace, "authenticatorAttachment", true},
    {ResponseFace, "clientDataJSON", true},
    {AttestationFace, "getTransports", false}, {AttestationFace, "getAuthenticatorData", false},
    {AttestationFace, "getPublicKey", false}, {AttestationFace, "getPublicKeyAlgorithm", false},
    {AttestationFace, "attestationObject", true},
    {AssertionFace, "authenticatorData", true}, {AssertionFace, "signature", true},
    {AssertionFace, "userHandle", true}};
struct StaticDef {
  const char* name;
  JSNative native;
  unsigned length;
};
constexpr StaticDef kStatics[] = {
    {"isConditionalMediationAvailable", IsConditionalMediationAvailable, 0},
    {"isUserVerifyingPlatformAuthenticatorAvailable", IsUserVerifyingPlatformAuthenticatorAvailable, 0},
    {"getClientCapabilities", GetClientCapabilities, 0},
    {"parseCreationOptionsFromJSON", ParseCreationOptionsFromJSON, 1},
    {"parseRequestOptionsFromJSON", ParseRequestOptionsFromJSON, 1}};
// Parent interface of each face, or FaceCount for none.
constexpr Interface kParents[FaceCount] = {FaceCount, CredentialFace, FaceCount, ResponseFace,
                                           ResponseFace};
}  // namespace

bool Incompatible(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const MemberDef& member = kMembers[WebKit::Slot(args)];
  return WebKit::Fail(cx, (std::string("'") + (member.attribute ? "get " : "") + member.name +
                           "' called on an object that does not implement interface " +
                           kClasses[member.face].name + ".").c_str());
}
bool IllegalConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  return trace.Finish(WebKit::Fail(cx, "Illegal constructor."));
}

Realm::Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers)
    : state_(std::make_unique<State>(cx, page, helpers)) {}
Realm::~Realm() = default;
bool Realm::InstallNavigator(JS::HandleObject navigatorPrototype) {
  JSAutoRealm realm(state_->cx, state_->kit.page);
  return InstallNavigatorCredentials(*state_, navigatorPrototype);
}
bool Realm::ConfigureExposure() {
  State& s = *state_;
  JSAutoRealm realm(s.cx, s.kit.page);
  JS::RootedValue secure(s.cx);
  return JS_GetProperty(s.cx, s.kit.page, "isSecureContext", &secure) && s.secure.Apply(secure.isTrue());
}

bool Realm::Install() {
  State& s = *state_;
  JSContext* cx = s.cx;
  JSAutoRealm realm(cx, s.kit.page);
  JS::RootedValue exception(cx);
  if (!JS_GetProperty(cx, s.kit.page, "DOMException", &exception) || !exception.isObject())
    return WebKit::Fail(cx, "Web Authentication requires the realm's DOMException");
  s.exception = &exception.toObject();
  JS::RootedObject objectPrototype(cx, JS::GetRealmObjectPrototype(cx));
  for (int face = 0; face < FaceCount; ++face) {
    WebFace& target = s.faces[face];
    const Interface parent = kParents[face];
    JS::RootedObject parentPrototype(cx, parent == FaceCount ? objectPrototype.get()
                                                             : s.faces[parent].prototype.get());
    if (!s.kit.Define(kClasses[face].name, &kClasses[face], parentPrototype, IllegalConstructor,
                      &target)) return false;
    if (parent != FaceCount) {
      JS::RootedObject parentConstructor(cx, s.faces[parent].constructor);
      if (!JS_SetPrototype(cx, target.constructor, parentConstructor)) return false;
    }
    for (size_t i = 0; i < std::size(kMembers); ++i) {
      const MemberDef& member = kMembers[i];
      if (member.face != face) continue;
      const char* iface = kClasses[face].name;
      if (!(member.attribute
                ? s.kit.Accessor(target, iface, member.name, Incompatible, nullptr, int32_t(i))
                : s.kit.Method(target, iface, member.name, Incompatible, 0, int32_t(i))))
        return false;
    }
    if (face == PublicKeyFace) {
      for (const StaticDef& operation : kStatics) {
        JS::RootedObject function(cx, s.kit.Function(operation.native, operation.name,
                                                     operation.length, 0));
        if (!function ||
            !JS_DefineProperty(cx, target.constructor, operation.name, function, JSPROP_ENUMERATE) ||
            !s.kit.helpers->Member(function, "PublicKeyCredential", operation.name, "call"))
          return false;
      }
    }
    JS::RootedValue constructor(cx, JS::ObjectValue(*target.constructor));
    if (!JS_DeleteProperty(cx, target.prototype, "constructor") ||
        !JS_DefineProperty(cx, target.prototype, "constructor", constructor, 0)) return false;
  }
  return true;
}
}  // namespace ruyijs::web_credentials
