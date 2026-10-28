// PublicKeyCredential.parseCreationOptionsFromJSON / parseRequestOptionsFromJSON
// (donor dom/webauthn/PublicKeyCredential.cpp). The binding converts the whole
// JSON dictionary first (members in lexical order); only then does the donor
// decode the Base64URLString members, in its own order, and the result
// dictionary is built with its defaults in lexical member order.
#include "web_credentials/options_convert.h"
#include "jsapi.h"

namespace ruyijs::web_credentials {
bool ParseRequestOptionsFromJSON(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Converter c(*static_cast<State*>(WebKit::Owner(args)),
              u"PublicKeyCredential.parseRequestOptionsFromJSON: ");
  if (!argc) return c.MissingArgument();
  constexpr const char16_t* kOwner = u"PublicKeyCredentialRequestOptionsJSON";
  JS::RootedObject json(cx), result(cx, JS_NewPlainObject(cx));
  JS::RootedValue allow(cx), challenge(cx), extensions(cx), hints(cx), rpId(cx), timeout(cx),
      verification(cx);
  DescriptorList allowed(cx);
  if (!result || !c.Dictionary(args[0], u"Argument 1", &json) ||
      !c.Member(json, "allowCredentials", &allow) ||
      !c.Descriptors(allow, u"allowCredentials", kOwner, &allowed) ||
      !c.Member(json, "challenge", &challenge) || !c.Required(challenge, u"challenge", kOwner) ||
      !c.Text(&challenge) || !c.Member(json, "extensions", &extensions) ||
      !c.Extensions(extensions, kOwner, false, &extensions) || !c.Member(json, "hints", &hints) ||
      (!hints.isUndefined() && !c.TextSequence(&hints, u"'hints' member of " + std::u16string(kOwner))) ||
      !c.Member(json, "rpId", &rpId) || (!rpId.isUndefined() && !c.Text(&rpId)) ||
      !c.Member(json, "timeout", &timeout) || (!timeout.isUndefined() && !c.UnsignedLong(&timeout)) ||
      !c.Member(json, "userVerification", &verification) ||
      (!verification.isUndefined() && !c.Text(&verification))) return false;
  JS::RootedValue buffer(cx), credentials(cx), empty(cx);
  if (!c.Decoded(challenge, u"challenge", &buffer) ||
      !c.DecodeDescriptors(allowed, u"allowed credential ID", &credentials)) return false;
  if (hints.isUndefined()) hints.setObject(*c.Array());
  if (verification.isUndefined() && !c.Literal(u"preferred", &verification)) return false;
  if (!c.Set(result, "allowCredentials", credentials) || !c.Set(result, "challenge", buffer) ||
      !c.Set(result, "extensions", extensions) || !c.Set(result, "hints", hints) ||
      (!rpId.isUndefined() && !c.Set(result, "rpId", rpId)) ||
      (!timeout.isUndefined() && !c.Set(result, "timeout", timeout)) ||
      !c.Set(result, "userVerification", verification)) return false;
  args.rval().setObject(*result);
  return true;
}

bool ParseCreationOptionsFromJSON(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Converter c(*static_cast<State*>(WebKit::Owner(args)),
              u"PublicKeyCredential.parseCreationOptionsFromJSON: ");
  if (!argc) return c.MissingArgument();
  constexpr const char16_t* kOwner = u"PublicKeyCredentialCreationOptionsJSON";
  JS::RootedObject json(cx), result(cx, JS_NewPlainObject(cx)), user(cx, JS_NewPlainObject(cx));
  JS::RootedValue attestation(cx), selection(cx), challenge(cx), exclude(cx), extensions(cx),
      hints(cx), params(cx), rp(cx), timeout(cx), userJson(cx), display(cx), id(cx), name(cx);
  DescriptorList excluded(cx);
  if (!result || !user || !c.Dictionary(args[0], u"Argument 1", &json) ||
      !c.Member(json, "attestation", &attestation) ||
      (!attestation.isUndefined() && !c.Text(&attestation)) ||
      !c.Member(json, "authenticatorSelection", &selection) ||
      !c.Selection(selection, kOwner, &selection) ||
      !c.Member(json, "challenge", &challenge) || !c.Required(challenge, u"challenge", kOwner) ||
      !c.Text(&challenge) || !c.Member(json, "excludeCredentials", &exclude) ||
      !c.Descriptors(exclude, u"excludeCredentials", kOwner, &excluded) ||
      !c.Member(json, "extensions", &extensions) ||
      !c.Extensions(extensions, kOwner, true, &extensions) || !c.Member(json, "hints", &hints) ||
      (!hints.isUndefined() && !c.TextSequence(&hints, u"'hints' member of " + std::u16string(kOwner))) ||
      !c.Member(json, "pubKeyCredParams", &params) ||
      !c.Required(params, u"pubKeyCredParams", kOwner) || !c.Parameters(params, kOwner, &params) ||
      !c.Member(json, "rp", &rp) || !c.Required(rp, u"rp", kOwner) ||
      !c.RelyingParty(rp, kOwner, &rp) || !c.Member(json, "timeout", &timeout) ||
      (!timeout.isUndefined() && !c.UnsignedLong(&timeout)) || !c.Member(json, "user", &userJson) ||
      !c.Required(userJson, u"user", kOwner) || !c.User(userJson, kOwner, &display, &id, &name))
    return false;
  // The donor decodes the user ID, then the challenge, then excluded IDs.
  JS::RootedValue userId(cx), buffer(cx), credentials(cx), userValue(cx, JS::ObjectValue(*user));
  if (!c.Decoded(id, u"user ID", &userId) || !c.Decoded(challenge, u"challenge", &buffer) ||
      !c.DecodeDescriptors(excluded, u"excluded credential ID", &credentials)) return false;
  if (hints.isUndefined()) hints.setObject(*c.Array());
  if (attestation.isUndefined() && !c.Literal(u"none", &attestation)) return false;
  // PublicKeyCredentialUserEntity : PublicKeyCredentialEntity: name, then own members.
  if (!c.Set(user, "name", name) || !c.Set(user, "displayName", display) ||
      !c.Set(user, "id", userId) || !c.Set(result, "attestation", attestation) ||
      !c.Set(result, "authenticatorSelection", selection) || !c.Set(result, "challenge", buffer) ||
      !c.Set(result, "excludeCredentials", credentials) || !c.Set(result, "extensions", extensions) ||
      !c.Set(result, "hints", hints) || !c.Set(result, "pubKeyCredParams", params) ||
      !c.Set(result, "rp", rp) || (!timeout.isUndefined() && !c.Set(result, "timeout", timeout)) ||
      !c.Set(result, "user", userValue)) return false;
  args.rval().setObject(*result);
  return true;
}
}  // namespace ruyijs::web_credentials
