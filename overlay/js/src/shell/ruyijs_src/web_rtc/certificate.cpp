#include "web_rtc/internal.h"

#include "domain/rtc/dtls_certificate.h"
#include "mozilla/RandomNum.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/String.h"
#include "jsapi.h"

namespace ruyijs::web_rtc {
namespace {

namespace rtc = ruyijs::domain::rtc;


// WebIDL converts the AlgorithmIdentifier union member to a DOMString and the
// WebCrypto normalization upper-cases the algorithm name.
bool Name(JSContext* cx, JS::MutableHandleValue value, std::string* name) {
  JS::RootedString text(cx, JS::ToString(cx, value));
  if (!text || !ToUTF8(cx, text, name)) return false;
  for (char& c : *name)
    if (c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
  return true;
}

bool ReadAlgorithm(JSContext* cx, JS::HandleValue value, std::string* name,
                   std::string* curve, bool* hasRsaParameters) {
  JS::RootedValue algorithm(cx, value);
  if (!algorithm.isObject()) return Name(cx, &algorithm, name);
  JS::RootedObject dictionary(cx, &algorithm.toObject());
  JS::RootedValue member(cx);
  if (!JS_GetProperty(cx, dictionary, "name", &member)) return false;
  // Algorithm dictionaries with no name are normalized by Firefox and then
  // rejected through the returned promise, rather than throwing while the
  // native method is entered.
  if (member.isUndefined() || member.isNull()) {
    name->clear();
    return true;
  }
  if (!Name(cx, &member, name)) return false;
  if (*name == "ECDSA") {
    if (!JS_GetProperty(cx, dictionary, "namedCurve", &member)) return false;
    if (member.isUndefined() || member.isNull()) return true;
    return Name(cx, &member, curve);
  }
  if (*name == "RSASSA-PKCS1-V1_5") {
    JS::RootedValue modulusLength(cx), publicExponent(cx), hash(cx);
    if (!JS_GetProperty(cx, dictionary, "modulusLength", &modulusLength) ||
        !JS_GetProperty(cx, dictionary, "publicExponent", &publicExponent) ||
        !JS_GetProperty(cx, dictionary, "hash", &hash))
      return false;
    *hasRsaParameters = !modulusLength.isUndefined() && !modulusLength.isNull() &&
                        publicExponent.isObject() && !hash.isUndefined() &&
                        !hash.isNull();
  }
  return true;
}

}  // namespace

JSObject* NewCertificate(State& state, double expires, const std::string& fingerprint,
                        JS::HandleObject proto) {
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, state.kit.page);
  JS::RootedObject certificate(cx, state.kit.New(&kCertificateClass, proto));
  JS::RootedValue text(cx);
  if (!certificate || !StringValue(cx, fingerprint, &text)) return nullptr;
  JS::SetReservedSlot(certificate, kCertificateExpires, JS::DoubleValue(expires));
  JS::SetReservedSlot(certificate, kCertificateFingerprint, text);
  return certificate.get();
}

bool CertificateGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Brand(cx, args, &kCertificateClass, &self))
    return false;
  args.rval().set(JS::GetReservedSlot(self, WebKit::Slot(args)));
  return true;
}

// RTCDtlsFingerprint is a dictionary, so its two members are materialized in
// lexicographic order on a fresh plain object per call.
bool CertificateMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Brand(cx, args, &kCertificateClass, &self))
    return false;
  JS::RootedObject array(cx, JS::NewArrayObject(cx, 0));
  JS::RootedObject fingerprint(cx, JS_NewPlainObject(cx));
  JS::RootedString algorithm(
      cx, JS_NewStringCopyZ(cx, rtc::kCertificateFingerprintAlgorithm));
  JS::RootedValue value(cx, JS::GetReservedSlot(self, kCertificateFingerprint));
  JS::RootedValue entry(cx);
  if (!array || !fingerprint || !algorithm ||
      !JS_DefineProperty(cx, fingerprint, "algorithm", algorithm, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, fingerprint, "value", value, JSPROP_ENUMERATE))
    return false;
  entry.setObject(*fingerprint);
  if (!JS_DefineElement(cx, array, 0, entry, JSPROP_ENUMERATE)) return false;
  args.rval().setObject(*array);
  return true;
}

// The certificate value: one SHA-256 DTLS fingerprint over 32 random bytes and
// a 30-day expiry. No key pair and no X.509 certificate are created yet.
bool GenerateCertificate(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = static_cast<State*>(WebKit::Owner(args));
  if (!argc)
    return RejectError(*state, "TypeError", nullptr,
                       "RTCPeerConnection.generateCertificate: At least 1 "
                       "argument required, but only 0 passed", args.rval());
  JSAutoRealm realm(cx, state->kit.page);
  std::string name, curve;
  bool hasRsaParameters = false;
  if (!ReadAlgorithm(cx, args.get(0), &name, &curve, &hasRsaParameters)) return false;
  if (!rtc::SupportsCertificateAlgorithm(name, curve, hasRsaParameters))
    return RejectError(*state, "DOMException", "NotSupportedError",
                       "Operation is not supported", args.rval());
  uint8_t digest[rtc::kDtlsFingerprintBytes];
  if (!mozilla::GenerateRandomBytesFromOS(digest, sizeof(digest)))
    return RejectError(*state, "DOMException", "OperationError",
                       "The operating system random generator failed", args.rval());
  const double expires = double(JS_Now() / 1000) + double(rtc::kCertificateLifetimeMs);
  JS::RootedObject certificate(
      cx, NewCertificate(*state, expires,
                         rtc::FormatCertificateFingerprint(digest, sizeof(digest)),
                         state->certificate.prototype));
  if (!certificate) return false;
  JS::RootedValue value(cx, JS::ObjectValue(*certificate));
  return state->kit.Resolve(value, args.rval());
}

}  // namespace ruyijs::web_rtc
