// The nested dictionaries of the JSON options (WebAuthentication.webidl):
// descriptors, extensions, authenticator selection, credential parameters,
// relying party and user. Each converts its members in lexical order.
#include "web_credentials/options_convert.h"
#include "jsapi.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::web_credentials {
namespace {
std::u16string Of(const char16_t* member, const char16_t* owner) {
  return std::u16string(u"'") + member + u"' member of " + owner;
}
}  // namespace

bool Converter::Descriptors(JS::HandleValue value, const char16_t* member, const char16_t* owner,
                            DescriptorList* list) {
  constexpr const char16_t* kName = u"PublicKeyCredentialDescriptorJSON";
  JS::RootedValueVector items(cx);
  if (value.isUndefined()) return true;
  if (!Sequence(value, Of(member, owner), &items)) return false;
  for (size_t i = 0; i < items.length(); ++i) {
    JS::RootedObject entry(cx);
    JS::RootedValue id(cx), transports(cx), type(cx);
    if (!Dictionary(items[i], u"Element of " + Of(member, owner), &entry) ||
        !Member(entry, "id", &id) || !Required(id, u"id", kName) || !Text(&id) ||
        !Member(entry, "transports", &transports) ||
        (!transports.isUndefined() && !TextSequence(&transports, Of(u"transports", kName))) ||
        !Member(entry, "type", &type) || !Required(type, u"type", kName) || !Text(&type) ||
        !list->ids.append(id) || !list->transports.append(transports) || !list->types.append(type))
      return false;
  }
  return true;
}

bool Converter::DecodeDescriptors(const DescriptorList& list, const char16_t* what,
                                  JS::MutableHandleValue out) {
  JS::RootedObject result(cx, Array());
  if (!result) return false;
  for (size_t i = 0; i < list.ids.length(); ++i) {
    JS::RootedObject entry(cx, JS_NewPlainObject(cx));
    JS::RootedValue buffer(cx), entryValue(cx);
    if (!entry || !Decoded(list.ids[i], what, &buffer) || !Set(entry, "id", buffer) ||
        (!list.transports[i].isUndefined() && !Set(entry, "transports", list.transports[i])) ||
        !Set(entry, "type", list.types[i])) return false;
    entryValue.setObject(*entry);
    if (!Push(result, entryValue)) return false;
  }
  out.setObject(*result);
  return true;
}

// AuthenticationExtensionsClientInputsJSON: the scalar members, copied into
// AuthenticationExtensionsClientInputs as the donor copies them for a
// creation or a request. largeBlob and prf are not converted (DEVIATIONS.md).
bool Converter::Extensions(JS::HandleValue value, const char16_t* owner, bool creation,
                           JS::MutableHandleValue out) {
  constexpr const char16_t* kName = u"AuthenticationExtensionsClientInputsJSON";
  struct Field { const char* name; const char16_t* label; int kind; bool creation, request; };
  constexpr Field kFields[] = {
      {"appid", u"appid", 0, true, true}, {"credProps", u"credProps", 1, true, true},
      {"credentialProtectionPolicy", u"credentialProtectionPolicy", 2, true, false},
      {"enforceCredentialProtectionPolicy", u"enforceCredentialProtectionPolicy", 1, true, false},
      {"hmacCreateSecret", u"hmacCreateSecret", 1, true, false},
      {"minPinLength", u"minPinLength", 1, true, true}};
  JS::RootedObject json(cx), result(cx, JS_NewPlainObject(cx));
  if (!result || !Dictionary(value, Of(u"extensions", owner), &json)) return false;
  for (const Field& field : kFields) {
    JS::RootedValue member(cx);
    if (!Member(json, field.name, &member)) return false;
    if (member.isUndefined()) continue;
    if (field.kind == 1) member.setBoolean(JS::ToBoolean(member));
    else if (!Text(&member)) return false;
    if (field.kind == 2 &&
        !Enumeration(member, field.label, kName, u"CredentialProtectionPolicy",
                     {u"userVerificationOptional", u"userVerificationOptionalWithCredentialIDList",
                      u"userVerificationRequired"})) return false;
    if ((creation ? field.creation : field.request) && !Set(result, field.name, member)) return false;
  }
  out.setObject(*result);
  return true;
}

// AuthenticatorSelectionCriteria; the result member defaults to {}, so its
// defaults appear even when the JSON omits it.
bool Converter::Selection(JS::HandleValue value, const char16_t* owner, JS::MutableHandleValue out) {
  JS::RootedObject json(cx), result(cx, JS_NewPlainObject(cx));
  JS::RootedValue attachment(cx), require(cx), resident(cx), verification(cx);
  if (!result || !Dictionary(value, Of(u"authenticatorSelection", owner), &json) ||
      !Member(json, "authenticatorAttachment", &attachment) ||
      (!attachment.isUndefined() && !Text(&attachment)) ||
      !Member(json, "requireResidentKey", &require) ||
      !Member(json, "residentKey", &resident) || (!resident.isUndefined() && !Text(&resident)) ||
      !Member(json, "userVerification", &verification) ||
      (!verification.isUndefined() && !Text(&verification))) return false;
  require.setBoolean(!require.isUndefined() && JS::ToBoolean(require));
  if (verification.isUndefined() && !Literal(u"preferred", &verification)) return false;
  if ((!attachment.isUndefined() && !Set(result, "authenticatorAttachment", attachment)) ||
      !Set(result, "requireResidentKey", require) ||
      (!resident.isUndefined() && !Set(result, "residentKey", resident)) ||
      !Set(result, "userVerification", verification)) return false;
  out.setObject(*result);
  return true;
}

bool Converter::Parameters(JS::HandleValue value, const char16_t* owner, JS::MutableHandleValue out) {
  constexpr const char16_t* kName = u"PublicKeyCredentialParameters";
  JS::RootedValueVector items(cx);
  JS::RootedObject result(cx, Array());
  if (!result || !Sequence(value, Of(u"pubKeyCredParams", owner), &items)) return false;
  for (size_t i = 0; i < items.length(); ++i) {
    JS::RootedObject json(cx), entry(cx, JS_NewPlainObject(cx));
    JS::RootedValue alg(cx), type(cx), entryValue(cx);
    int32_t algorithm = 0;
    if (!entry || !Dictionary(items[i], u"Element of " + Of(u"pubKeyCredParams", owner), &json) ||
        !Member(json, "alg", &alg) || !Required(alg, u"alg", kName) ||
        !JS::ToInt32(cx, alg, &algorithm) || !Member(json, "type", &type) ||
        !Required(type, u"type", kName) || !Text(&type)) return false;
    alg.setInt32(algorithm);
    entryValue.setObject(*entry);
    if (!Set(entry, "alg", alg) || !Set(entry, "type", type) || !Push(result, entryValue))
      return false;
  }
  out.setObject(*result);
  return true;
}

// PublicKeyCredentialRpEntity : PublicKeyCredentialEntity: the inherited
// required name is converted (and reported) before the own id.
bool Converter::RelyingParty(JS::HandleValue value, const char16_t* owner, JS::MutableHandleValue out) {
  JS::RootedObject json(cx), result(cx, JS_NewPlainObject(cx));
  JS::RootedValue name(cx), id(cx);
  if (!result || !Dictionary(value, Of(u"rp", owner), &json) || !Member(json, "name", &name) ||
      !Required(name, u"name", u"PublicKeyCredentialEntity") || !Text(&name) ||
      !Member(json, "id", &id) || (!id.isUndefined() && !Text(&id)) ||
      !Set(result, "name", name) || (!id.isUndefined() && !Set(result, "id", id))) return false;
  out.setObject(*result);
  return true;
}

bool Converter::User(JS::HandleValue value, const char16_t* owner, JS::MutableHandleValue display,
                     JS::MutableHandleValue id, JS::MutableHandleValue name) {
  constexpr const char16_t* kName = u"PublicKeyCredentialUserEntityJSON";
  JS::RootedObject json(cx);
  return Dictionary(value, Of(u"user", owner), &json) &&
         Member(json, "displayName", display) && Required(display, u"displayName", kName) &&
         Text(display) && Member(json, "id", id) && Required(id, u"id", kName) && Text(id) &&
         Member(json, "name", name) && Required(name, u"name", kName) && Text(name);
}
}  // namespace ruyijs::web_credentials
