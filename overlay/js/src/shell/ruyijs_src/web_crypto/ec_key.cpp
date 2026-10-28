#include "web_crypto/ec.h"
#include "web_crypto/cng_backend.h"
#include "web_crypto/crypto_key.h"
#include "runtime/runtime.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
namespace ruyijs::web_crypto {
namespace ec = ruyijs::domain::crypto;
namespace {
// Material: curve size byte, private flag, x, y, d.
std::vector<uint8_t> Material(const ec::EcKey& key) {
  std::vector<uint8_t> out{uint8_t(key.curve->size), uint8_t(!key.d.empty())};
  out.insert(out.end(), key.x.begin(), key.x.end());
  out.insert(out.end(), key.y.begin(), key.y.end());
  out.insert(out.end(), key.d.begin(), key.d.end());
  return out;
}
const char* CurveForSize(size_t size) { return size == 32 ? "P-256" : size == 48 ? "P-384" : "P-521"; }
}  // namespace
JSObject* KeyOps(JSContext* cx, JS::HandleObject key) {
  JS::RootedObject usages(cx, &JS::GetReservedSlot(key, KeyUsages).toObject());
  JS::RootedObject ops(cx, JS::NewArrayObject(cx, 0));
  uint32_t length = 0;
  if (!ops || !JS::GetArrayLength(cx, usages, &length)) return nullptr;
  JS::RootedValue item(cx);
  for (uint32_t i = 0; i < length; ++i)
    if (!JS_GetElement(cx, usages, i, &item) || !JS_DefineElement(cx, ops, i, item, JSPROP_ENUMERATE)) return nullptr;
  return ops;
}
JSObject* NewEcKey(JSContext* cx, JS::HandleObject subtle, bool ecdh, bool extractable,
                   const ec::EcKey& key, JS::HandleObject usages) {
  JS::RootedObject algorithm(cx, JS_NewPlainObject(cx));
  if (!algorithm || !DefineString(cx, algorithm, "name", ecdh ? "ECDH" : "ECDSA") ||
      !DefineString(cx, algorithm, "namedCurve", key.curve->name)) return nullptr;
  const bool priv = !key.d.empty();
  // A public key is always extractable (WebCrypto generateKey/importKey).
  return NewCryptoKey(cx, subtle, priv ? "private" : "public", priv ? extractable : true, algorithm,
                      usages, Material(key));
}
bool ReadEcKey(JSContext* cx, JS::HandleObject key, ec::EcKey* out) {
  std::vector<uint8_t> m;
  if (!ReadKeyMaterial(cx, key, &m) || m.size() < 2) return false;
  const size_t n = m[0];
  out->curve = ec::EcCurveNamed(CurveForSize(n));
  if (!out->curve || out->curve->size != n || m.size() != 2 + (m[1] ? 3 : 2) * n) return false;
  out->x.assign(m.begin() + 2, m.begin() + 2 + n);
  out->y.assign(m.begin() + 2 + n, m.begin() + 2 + 2 * n);
  if (m[1]) out->d.assign(m.begin() + 2 + 2 * n, m.end());
  return true;
}
bool EcImport(JSContext* cx, JS::HandleObject subtle, const JS::CallArgs& args, bool ecdh,
              JS::MutableHandleValue result) {
  std::string format, curveName;
  bool present = false;
  JS::RootedString text(cx, JS::ToString(cx, args.get(0)));
  if (!text || !ToUTF8(cx, text, &format) || !StringMember(cx, args.get(2), "namedCurve", &curveName, &present)) return false;
  if (!present) return CryptoTypeError(cx, "EcKeyImportParams requires namedCurve");
  const ec::EcCurve* curve = ec::EcCurveNamed(curveName);
  if (!curve) return SubtleError(cx, subtle, "NotSupportedError");
  ec::EcKey key;
  bool ok = false;
  if (format == "jwk") {
    if (!args.get(1).isObject()) return CryptoTypeError(cx, "importKey jwk requires a JsonWebKey");
    std::string kty, crv, x, y, d;
    bool has = false, hasD = false;
    if (!StringMember(cx, args[1], "kty", &kty, &has) || !StringMember(cx, args[1], "crv", &crv, &has) ||
        !StringMember(cx, args[1], "x", &x, &has) || !StringMember(cx, args[1], "y", &y, &has) ||
        !StringMember(cx, args[1], "d", &d, &hasD)) return false;
    key.curve = curve;
    ok = kty == "EC" && crv == curve->name && FromBase64Url(x, &key.x) && FromBase64Url(y, &key.y) &&
         key.x.size() == curve->size && key.y.size() == curve->size &&
         (!hasD || (FromBase64Url(d, &key.d) && key.d.size() == curve->size));
    JS::RootedObject jwk(cx, &args[1].toObject());
    JS::RootedValue ext(cx);
    if (!JS_GetProperty(cx, jwk, "ext", &ext)) return false;
    if (ok && ext.isBoolean() && !ext.toBoolean() && JS::ToBoolean(args.get(3))) ok = false;
  } else {
    std::vector<uint8_t> bytes;
    if (!RsaBytes(cx, args.get(1), &bytes)) return false;
    bool missingPublic = false;
    if (format == "spki") {
      // As Firefox: an spki/pkcs8 key keeps the curve its DER names.
      ok = ec::DecodeEcSpki(bytes.data(), bytes.size(), &key);
    } else if (format == "pkcs8") {
      ok = ec::DecodeEcPkcs8(bytes.data(), bytes.size(), &key, &missingPublic);
      if (missingPublic) return SubtleError(cx, subtle, "NotSupportedError");
    } else if (format == "raw") {
      ok = ec::DecodeEcPoint(bytes.data(), bytes.size(), *curve, &key);
    } else {
      return SubtleError(cx, subtle, "NotSupportedError");
    }
  }
  if (!ok || !EcValidate(key, ecdh)) return SubtleError(cx, subtle, "DataError");
  const bool priv = !key.d.empty();
  JS::RootedObject usages(cx);
  uint32_t count = 0;
  if (ecdh) {
    if (!ReadKeyUsages(cx, subtle, args.get(4), {"deriveKey", "deriveBits"}, {"deriveKey", "deriveBits"}, &usages, &count))
      return false;
    if (!priv && count) return SubtleError(cx, subtle, "SyntaxError");
  } else if (!ReadKeyUsages(cx, subtle, args.get(4), {priv ? "sign" : "verify"}, {"sign", "verify"}, &usages, &count)) {
    return false;
  }
  if (priv && !count) return SubtleError(cx, subtle, "SyntaxError");
  JS::RootedObject imported(cx, NewEcKey(cx, subtle, ecdh, JS::ToBoolean(args.get(3)), key, usages));
  if (!imported) return false;
  result.setObject(*imported);
  return true;
}
bool EcExport(JSContext* cx, JS::HandleObject subtle, const std::string& format, JS::HandleObject key,
              JS::MutableHandleValue result) {
  ec::EcKey material;
  if (!ReadEcKey(cx, key, &material)) return SubtleError(cx, subtle, "OperationError");
  const bool priv = !material.d.empty();
  if (!JS::GetReservedSlot(key, KeyExtractable).toBoolean()) return SubtleError(cx, subtle, "InvalidAccessError");
  if (format == "jwk") {
    // JsonWebKey is a dictionary: members appear in lexicographic order.
    JS::RootedObject jwk(cx, JS_NewPlainObject(cx)), ops(cx, KeyOps(cx, key));
    std::string algorithm;
    JS::RootedValue keyValue(cx, JS::ObjectValue(*key));
    if (!KeyAlgorithmName(cx, keyValue, &algorithm)) return false;
    // ECDSA keys name their JWS algorithm; ECDH keys have none.
    const char* alg = material.curve->size == 32 ? "ES256" : material.curve->size == 48 ? "ES384" : "ES512";
    if (!jwk || !ops || (algorithm == "ECDSA" && !DefineString(cx, jwk, "alg", alg)) ||
        !DefineString(cx, jwk, "crv", material.curve->name) ||
        (priv && !DefineString(cx, jwk, "d", Base64Url(material.d))) ||
        !JS_DefineProperty(cx, jwk, "ext", JS::TrueHandleValue, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, jwk, "key_ops", ops, JSPROP_ENUMERATE) || !DefineString(cx, jwk, "kty", "EC") ||
        !DefineString(cx, jwk, "x", Base64Url(material.x)) || !DefineString(cx, jwk, "y", Base64Url(material.y)))
      return false;
    result.setObject(*jwk);
    return true;
  }
  std::vector<uint8_t> bytes;
  if (format == "spki" && !priv) bytes = ec::EncodeEcSpki(material);
  else if (format == "raw" && !priv) bytes = ec::EcPoint(material);
  else if (format == "pkcs8" && priv) bytes = ec::EncodeEcPkcs8(material);
  else return SubtleError(cx, subtle, "NotSupportedError");  // also a format of the other key type
  JS::RootedObject buffer(cx, RsaBuffer(cx, bytes));
  if (!buffer) return false;
  result.setObject(*buffer);
  return true;
}
}  // namespace ruyijs::web_crypto
