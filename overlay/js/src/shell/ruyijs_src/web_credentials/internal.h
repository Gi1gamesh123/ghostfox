#ifndef RUYIJS_WEB_CREDENTIALS_INTERNAL_H
#define RUYIJS_WEB_CREDENTIALS_INTERNAL_H
#include "web_credentials/realm.h"
#include "runtime/secure_slot.h"
#include "runtime/web_kit.h"
#include <string>

namespace ruyijs::web_credentials {
// The five interfaces, in the donor's install order (WebAuthentication.webidl,
// CredentialManagement.webidl).
enum Interface { CredentialFace, PublicKeyFace, ResponseFace, AttestationFace, AssertionFace,
                 FaceCount };
struct State {
  State(JSContext* cx, JS::HandleObject page, PageHelpers& helpers)
      : cx(cx), kit(cx, page, helpers, this), exception(cx),
        faces{WebFace(cx), WebFace(cx), WebFace(cx), WebFace(cx), WebFace(cx)},
        container_face(cx), login_face(cx), container(cx), login(cx), secure(cx) {}
  JSContext* cx;
  WebKit kit;
  JS::PersistentRootedObject exception;
  WebFace faces[FaceCount];
  // navigator.credentials / navigator.login (container.cpp).
  WebFace container_face, login_face;
  JS::PersistentRootedObject container, login;
  // [SecureContext]: every interface of this owner and both Navigator members.
  SecureSlots secure;
};
bool InstallNavigatorCredentials(State& state, JS::HandleObject navigatorPrototype);

// Every prototype member: with no platform object in existence, each one
// reports the donor binding's receiver error for its table entry.
bool Incompatible(JSContext* cx, unsigned argc, JS::Value* vp);
bool IllegalConstructor(JSContext* cx, unsigned argc, JS::Value* vp);
// PublicKeyCredential statics (statics.cpp, options_json.cpp).
bool IsUserVerifyingPlatformAuthenticatorAvailable(JSContext* cx, unsigned argc, JS::Value* vp);
bool IsConditionalMediationAvailable(JSContext* cx, unsigned argc, JS::Value* vp);
bool GetClientCapabilities(JSContext* cx, unsigned argc, JS::Value* vp);
bool ParseCreationOptionsFromJSON(JSContext* cx, unsigned argc, JS::Value* vp);
bool ParseRequestOptionsFromJSON(JSContext* cx, unsigned argc, JS::Value* vp);

bool ThrowType(JSContext* cx, const std::u16string& message);
bool ThrowDomException(State& state, const std::u16string& message, const char* name);
}
#endif
