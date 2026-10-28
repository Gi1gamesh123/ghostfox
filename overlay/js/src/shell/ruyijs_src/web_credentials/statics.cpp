// PublicKeyCredential's promise-returning statics. The answers are the donor
// build's on this Windows profile (tests/fixtures/webauthn_interfaces_firefox.json):
// no user-verifying platform authenticator reported by
// isUserVerifyingPlatformAuthenticatorAvailable, conditional mediation
// available, and the getClientCapabilities record below.
#include "web_credentials/internal.h"
#include "jsapi.h"
#include "js/CallAndConstruct.h"
#include "js/ColumnNumber.h"
#include "js/ErrorReport.h"
#include "js/Promise.h"
#include "js/PropertyAndElement.h"
#include "js/ValueArray.h"

namespace ruyijs::web_credentials {
namespace {
struct Capability {
  const char* name;
  bool value;
};
// PublicKeyCredentialClientCapabilities, in the donor's (sorted) key order.
constexpr Capability kCapabilities[] = {
    {"conditionalCreate", false}, {"conditionalGet", true},
    {"extension:appid", true}, {"extension:credProps", true},
    {"extension:credentialProtectionPolicy", true},
    {"extension:enforceCredentialProtectionPolicy", true},
    {"extension:hmacCreateSecret", true}, {"extension:largeBlob", true},
    {"extension:minPinLength", true}, {"extension:prf", true},
    {"hybridTransport", true}, {"passkeyPlatformAuthenticator", true},
    {"relatedOrigins", true}, {"signalAllAcceptedCredentials", false},
    {"signalCurrentUserDetails", false}, {"signalUnknownCredential", false},
    {"userVerifyingPlatformAuthenticator", true}};

bool Resolved(JSContext* cx, const JS::CallArgs& args, JS::HandleValue value) {
  State& state = *static_cast<State*>(WebKit::Owner(args));
  return state.kit.Resolve(value, args.rval());
}
}  // namespace

bool ThrowType(JSContext* cx, const std::u16string& message) {
  JS::RootedString text(cx, JS_NewUCStringCopyN(cx, message.data(), message.size()));
  JS::RootedString file(cx, JS_NewStringCopyZ(cx, "ruyijs:web"));
  JS::Rooted<mozilla::Maybe<JS::Value>> cause(cx, mozilla::Nothing());
  JS::RootedValue error(cx);
  if (!text || !file || !JS::CreateError(cx, JSEXN_TYPEERR, nullptr, file, 1,
                                         JS::ColumnNumberOneOrigin(), nullptr, text, cause,
                                         &error)) return false;
  JS_SetPendingException(cx, error);
  return false;
}

bool ThrowDomException(State& state, const std::u16string& message, const char* name) {
  JSContext* cx = state.cx;
  JS::RootedValueArray<2> values(cx);
  JS::RootedString text(cx, JS_NewUCStringCopyN(cx, message.data(), message.size()));
  JS::RootedString label(cx, JS_NewStringCopyZ(cx, name));
  if (!text || !label) return false;
  values[0].setString(text);
  values[1].setString(label);
  JS::RootedValue constructor(cx, JS::ObjectValue(*state.exception));
  JS::RootedObject error(cx);
  if (!JS::Construct(cx, constructor, values, &error)) return false;
  JS::RootedValue thrown(cx, JS::ObjectValue(*error));
  JS_SetPendingException(cx, thrown);
  return false;
}

bool IsUserVerifyingPlatformAuthenticatorAvailable(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedValue value(cx, JS::FalseValue());
  return Resolved(cx, args, value);
}

bool IsConditionalMediationAvailable(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedValue value(cx, JS::TrueValue());
  return Resolved(cx, args, value);
}

bool GetClientCapabilities(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject record(cx, JS_NewPlainObject(cx));
  if (!record) return false;
  for (const Capability& capability : kCapabilities) {
    JS::RootedValue value(cx, JS::BooleanValue(capability.value));
    if (!JS_DefineProperty(cx, record, capability.name, value, JSPROP_ENUMERATE)) return false;
  }
  JS::RootedValue value(cx, JS::ObjectValue(*record));
  return Resolved(cx, args, value);
}
}  // namespace ruyijs::web_credentials
