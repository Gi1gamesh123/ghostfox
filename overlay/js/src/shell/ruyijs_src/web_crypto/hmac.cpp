// HMAC secret keys (generateKey, importKey/exportKey raw and jwk, sign/verify)
// over the CNG adapter; the key bytes are the material, the hash lives in
// CryptoKey.algorithm.hash.
#include "web_crypto/ec.h"
#include "web_crypto/cng_backend.h"
#include "web_crypto/crypto_key.h"
#include "runtime/runtime.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
namespace ruyijs::web_crypto {
namespace {
enum { kSign = 2, kVerify = 3, kGenerate = 5, kImport = 8, kExport = 9 };
const char* HashName(HashAlgorithm hash) {
  return hash == HashAlgorithm::Sha1 ? "SHA-1" : hash == HashAlgorithm::Sha256 ? "SHA-256"
       : hash == HashAlgorithm::Sha384 ? "SHA-384" : "SHA-512";
}
const char* JwkAlg(HashAlgorithm hash) {
  return hash == HashAlgorithm::Sha1 ? "HS1" : hash == HashAlgorithm::Sha256 ? "HS256"
       : hash == HashAlgorithm::Sha384 ? "HS384" : "HS512";
}
uint32_t BlockBits(HashAlgorithm hash) {
  return hash == HashAlgorithm::Sha384 || hash == HashAlgorithm::Sha512 ? 1024 : 512;
}
// The optional `length` member in bits; 0 when absent.
bool Length(JSContext* cx, JS::HandleValue params, double* bits) {
  *bits = 0;
  if (!params.isObject()) return true;
  JS::RootedObject dictionary(cx, &params.toObject());
  JS::RootedValue value(cx);
  return JS_GetProperty(cx, dictionary, "length", &value) && (value.isUndefined() || JS::ToNumber(cx, value, bits));
}
bool Finish(JSContext* cx, JS::HandleObject subtle, HashAlgorithm hash, std::vector<uint8_t> raw, uint32_t bits,
            bool extractable, JS::HandleValue requested, JS::MutableHandleValue result) {
  JS::RootedObject usages(cx), algorithm(cx, JS_NewPlainObject(cx)), hashObject(cx, JS_NewPlainObject(cx));
  uint32_t count = 0;
  if (!ReadKeyUsages(cx, subtle, requested, {"sign", "verify"}, {"sign", "verify"}, &usages, &count)) return false;
  if (!bits || bits > raw.size() * 8 || bits + 8 <= raw.size() * 8) return SubtleError(cx, subtle, "DataError");
  if (!count) return SubtleError(cx, subtle, "SyntaxError");
  // HmacKeyAlgorithm: name (KeyAlgorithm), then hash and length.
  if (!algorithm || !hashObject || !DefineString(cx, algorithm, "name", "HMAC") ||
      !DefineString(cx, hashObject, "name", HashName(hash)) ||
      !JS_DefineProperty(cx, algorithm, "hash", hashObject, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, algorithm, "length", bits, JSPROP_ENUMERATE)) return false;
  JS::RootedObject key(cx, NewCryptoKey(cx, subtle, "secret", extractable, algorithm, usages, raw));
  if (!key) return false;
  result.setObject(*key);
  return true;
}
bool KeyHash(JSContext* cx, JS::HandleObject key, HashAlgorithm* hash) {
  std::string name;
  if (!RsaKeyHash(cx, key, &name)) return false;
  *hash = name == "SHA-1" ? HashAlgorithm::Sha1 : name == "SHA-256" ? HashAlgorithm::Sha256
        : name == "SHA-384" ? HashAlgorithm::Sha384 : HashAlgorithm::Sha512;
  return true;
}
bool Generate(JSContext* cx, JS::HandleObject subtle, const JS::CallArgs& args, JS::MutableHandleValue result) {
  HashAlgorithm hash = HashAlgorithm::Sha256;
  bool known = false;
  double bits = 0;
  if (!ReadHash(cx, args.get(0), &hash, &known) || !Length(cx, args.get(0), &bits)) return false;
  if (!known) return SubtleError(cx, subtle, "NotSupportedError");
  if (!bits) bits = BlockBits(hash);
  if (bits != double(uint32_t(bits)) || bits < 1 || bits > 65536) return SubtleError(cx, subtle, "OperationError");
  std::vector<uint8_t> raw((size_t(bits) + 7) / 8);
  if (!RsaRandom(raw.data(), raw.size())) return SubtleError(cx, subtle, "OperationError");
  if (uint32_t(bits) % 8) raw.back() &= uint8_t(0xff << (8 - uint32_t(bits) % 8));
  return Finish(cx, subtle, hash, raw, uint32_t(bits), JS::ToBoolean(args.get(1)), args.get(2), result);
}
bool Import(JSContext* cx, JS::HandleObject subtle, const JS::CallArgs& args, JS::MutableHandleValue result) {
  HashAlgorithm hash = HashAlgorithm::Sha256;
  bool known = false;
  double bits = 0;
  if (!ReadHash(cx, args.get(2), &hash, &known) || !Length(cx, args.get(2), &bits)) return false;
  if (!known) return SubtleError(cx, subtle, "NotSupportedError");
  std::string format;
  JS::RootedString text(cx, JS::ToString(cx, args.get(0)));
  if (!text || !ToUTF8(cx, text, &format)) return false;
  std::vector<uint8_t> raw;
  if (format == "raw") {
    if (!RsaBytes(cx, args.get(1), &raw)) return false;
  } else if (format == "jwk") {
    if (!args.get(1).isObject()) return CryptoTypeError(cx, "importKey jwk requires a JsonWebKey");
    std::string kty, k, alg;
    bool has = false, hasAlg = false;
    if (!StringMember(cx, args[1], "kty", &kty, &has) || !StringMember(cx, args[1], "k", &k, &has) ||
        !StringMember(cx, args[1], "alg", &alg, &hasAlg)) return false;
    if (kty != "oct" || !FromBase64Url(k, &raw) || (hasAlg && alg != JwkAlg(hash))) return SubtleError(cx, subtle, "DataError");
  } else {
    return SubtleError(cx, subtle, "NotSupportedError");
  }
  if (raw.empty()) return SubtleError(cx, subtle, "DataError");
  const uint32_t length = bits ? uint32_t(bits) : uint32_t(raw.size() * 8);
  return Finish(cx, subtle, hash, raw, length, JS::ToBoolean(args.get(3)), args.get(4), result);
}
bool Export(JSContext* cx, JS::HandleObject subtle, const JS::CallArgs& args, JS::MutableHandleValue result) {
  std::string format;
  JS::RootedString text(cx, JS::ToString(cx, args.get(0)));
  JS::RootedObject key(cx, &args[1].toObject());
  std::vector<uint8_t> raw;
  HashAlgorithm hash = HashAlgorithm::Sha256;
  if (!text || !ToUTF8(cx, text, &format)) return false;
  if (!JS::GetReservedSlot(key, KeyExtractable).toBoolean()) return SubtleError(cx, subtle, "InvalidAccessError");
  if (!ReadKeyMaterial(cx, key, &raw) || !KeyHash(cx, key, &hash)) return SubtleError(cx, subtle, "OperationError");
  if (format == "raw") {
    JS::RootedObject buffer(cx, RsaBuffer(cx, raw));
    if (!buffer) return false;
    result.setObject(*buffer);
    return true;
  }
  if (format != "jwk") return SubtleError(cx, subtle, "NotSupportedError");
  JS::RootedObject jwk(cx, JS_NewPlainObject(cx)), ops(cx, KeyOps(cx, key));
  if (!jwk || !ops || !DefineString(cx, jwk, "alg", JwkAlg(hash)) ||
      !JS_DefineProperty(cx, jwk, "ext", JS::TrueHandleValue, JSPROP_ENUMERATE) || !DefineString(cx, jwk, "k", Base64Url(raw)) ||
      !JS_DefineProperty(cx, jwk, "key_ops", ops, JSPROP_ENUMERATE) || !DefineString(cx, jwk, "kty", "oct")) return false;
  result.setObject(*jwk);
  return true;
}
bool Sign(JSContext* cx, JS::HandleObject subtle, const JS::CallArgs& args, bool verify, JS::MutableHandleValue result) {
  if (!args.get(1).isObject() || JS::GetClass(&args[1].toObject()) != &kCryptoKeyClass)
    return CryptoTypeError(cx, "WebCrypto requires a CryptoKey");
  JS::RootedObject key(cx, &args[1].toObject());
  std::string algorithm;
  bool present = false;
  if (!KeyAlgorithmName(cx, args[1], &algorithm) || !HasUsage(cx, key, verify ? "verify" : "sign", &present)) return false;
  if (algorithm != "HMAC" || !present) return SubtleError(cx, subtle, "InvalidAccessError");
  std::vector<uint8_t> raw, data, signature, mac;
  HashAlgorithm hash = HashAlgorithm::Sha256;
  if (!ReadKeyMaterial(cx, key, &raw) || !KeyHash(cx, key, &hash)) return SubtleError(cx, subtle, "OperationError");
  if (!RsaBytes(cx, args.get(verify ? 3 : 2), &data) || (verify && !RsaBytes(cx, args.get(2), &signature))) return false;
  if (!Hmac(hash, raw, data, &mac)) return SubtleError(cx, subtle, "OperationError");
  if (verify) {
    uint8_t diff = uint8_t(mac.size() ^ signature.size());
    for (size_t i = 0; i < mac.size() && i < signature.size(); ++i) diff |= uint8_t(mac[i] ^ signature[i]);
    result.setBoolean(diff == 0);
    return true;
  }
  JS::RootedObject buffer(cx, RsaBuffer(cx, mac));
  if (!buffer) return false;
  result.setObject(*buffer);
  return true;
}
}  // namespace
SubtleStatus HmacOperation(JSContext* cx, JS::HandleObject subtle, unsigned operation,
                           const JS::CallArgs& args, JS::MutableHandleValue result) {
  std::string name;
  bool ok = false;
  switch (operation) {
    case kSign: case kVerify: case kGenerate: ok = CryptoAlgorithmName(cx, args.get(0), &name); break;
    case kImport: ok = CryptoAlgorithmName(cx, args.get(2), &name); break;
    case kExport: ok = KeyAlgorithmName(cx, args.get(1), &name); break;
    default: return SubtleStatus::kUnsupported;
  }
  if (!ok) return SubtleStatus::kFailed;
  if (name != "HMAC") return SubtleStatus::kUnsupported;
  switch (operation) {
    case kSign: case kVerify: ok = Sign(cx, subtle, args, operation == kVerify, result); break;
    case kGenerate: ok = Generate(cx, subtle, args, result); break;
    case kImport: ok = Import(cx, subtle, args, result); break;
    case kExport: ok = Export(cx, subtle, args, result); break;
  }
  return ok ? SubtleStatus::kHandled : SubtleStatus::kFailed;
}
}  // namespace ruyijs::web_crypto
