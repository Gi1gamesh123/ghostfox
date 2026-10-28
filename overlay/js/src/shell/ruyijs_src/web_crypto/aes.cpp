// AES-CBC and AES-GCM secret keys (generateKey, importKey/exportKey raw and
// jwk, encrypt/decrypt) over the CNG adapter; the key bytes are the material.
#include "web_crypto/ec.h"
#include "web_crypto/cng_backend.h"
#include "web_crypto/crypto_key.h"
#include "runtime/runtime.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
namespace ruyijs::web_crypto {
namespace {
enum { kEncrypt = 0, kDecrypt = 1, kGenerate = 5, kImport = 8, kExport = 9 };
bool IsAes(const std::string& name) { return name == "AES-CBC" || name == "AES-GCM"; }
bool ValidLength(size_t bytes) { return bytes == 16 || bytes == 24 || bytes == 32; }
JSObject* NewAesKey(JSContext* cx, JS::HandleObject subtle, const std::string& name, bool extractable,
                    const std::vector<uint8_t>& raw, JS::HandleObject usages) {
  JS::RootedObject algorithm(cx, JS_NewPlainObject(cx));
  if (!algorithm || !DefineString(cx, algorithm, "name", name) ||
      !JS_DefineProperty(cx, algorithm, "length", uint32_t(raw.size() * 8), JSPROP_ENUMERATE)) return nullptr;
  return NewCryptoKey(cx, subtle, "secret", extractable, algorithm, usages, raw);
}
std::string JwkAlg(const std::string& name, size_t bytes) {
  return "A" + std::to_string(bytes * 8) + (name == "AES-CBC" ? "CBC" : "GCM");
}
bool Buffer(JSContext* cx, JS::HandleObject dictionary, const char* member, std::vector<uint8_t>* out, bool* present) {
  JS::RootedValue value(cx);
  *present = false;
  if (!JS_GetProperty(cx, dictionary, member, &value)) return false;
  if (value.isUndefined()) return true;
  *present = true;
  return RsaBytes(cx, value, out);
}
bool Crypt(JSContext* cx, JS::HandleObject subtle, const JS::CallArgs& args, const std::string& name, bool encrypt,
           JS::MutableHandleValue result) {
  if (!args.get(0).isObject()) return CryptoTypeError(cx, "AES parameters must be a dictionary");
  if (!args.get(1).isObject() || JS::GetClass(&args[1].toObject()) != &kCryptoKeyClass)
    return CryptoTypeError(cx, "WebCrypto requires a CryptoKey");
  JS::RootedObject params(cx, &args[0].toObject()), key(cx, &args[1].toObject());
  std::string algorithm;
  bool present = false;
  if (!KeyAlgorithmName(cx, args[1], &algorithm) || !HasUsage(cx, key, encrypt ? "encrypt" : "decrypt", &present))
    return false;
  if (algorithm != name || !present) return SubtleError(cx, subtle, "InvalidAccessError");
  std::vector<uint8_t> raw, iv, aad, data, out;
  if (!Buffer(cx, params, "iv", &iv, &present)) return false;
  if (!present) return CryptoTypeError(cx, "AES parameters require an iv");
  if (!ReadKeyMaterial(cx, key, &raw) || !RsaBytes(cx, args.get(2), &data)) return false;
  bool ok = false;
  if (name == "AES-CBC") {
    if (iv.size() != 16) return SubtleError(cx, subtle, "OperationError");
    ok = AesCbc(raw, iv, data, encrypt, &out);
  } else {
    JS::RootedValue tag(cx);
    double tagBits = 128;
    if (!Buffer(cx, params, "additionalData", &aad, &present) || !JS_GetProperty(cx, params, "tagLength", &tag)) return false;
    if (!tag.isUndefined() && !JS::ToNumber(cx, tag, &tagBits)) return false;
    const int t = int(tagBits);
    if (tagBits != t || (t != 32 && t != 64 && t != 96 && t != 104 && t != 112 && t != 120 && t != 128))
      return SubtleError(cx, subtle, "OperationError");
    ok = AesGcm(raw, iv, aad, size_t(t / 8), data, encrypt, &out);
  }
  if (!ok) return SubtleError(cx, subtle, "OperationError");
  JS::RootedObject buffer(cx, RsaBuffer(cx, out));
  if (!buffer) return false;
  result.setObject(*buffer);
  return true;
}
bool Generate(JSContext* cx, JS::HandleObject subtle, const JS::CallArgs& args, const std::string& name,
              JS::MutableHandleValue result) {
  if (!args.get(0).isObject()) return CryptoTypeError(cx, "AesKeyGenParams requires length");
  JS::RootedObject params(cx, &args[0].toObject());
  JS::RootedValue lengthValue(cx);
  double bits = 0;
  if (!JS_GetProperty(cx, params, "length", &lengthValue)) return false;
  if (lengthValue.isUndefined()) return CryptoTypeError(cx, "AesKeyGenParams requires length");
  if (!JS::ToNumber(cx, lengthValue, &bits)) return false;
  JS::RootedObject usages(cx);
  uint32_t count = 0;
  if (!ReadKeyUsages(cx, subtle, args.get(2), {"encrypt", "decrypt", "wrapKey", "unwrapKey"},
                 {"encrypt", "decrypt", "wrapKey", "unwrapKey"}, &usages, &count)) return false;
  if (bits != 128 && bits != 192 && bits != 256) return SubtleError(cx, subtle, "OperationError");
  if (!count) return SubtleError(cx, subtle, "SyntaxError");
  std::vector<uint8_t> raw(size_t(bits) / 8);
  if (!RsaRandom(raw.data(), raw.size())) return SubtleError(cx, subtle, "OperationError");
  JS::RootedObject key(cx, NewAesKey(cx, subtle, name, JS::ToBoolean(args.get(1)), raw, usages));
  if (!key) return false;
  result.setObject(*key);
  return true;
}
bool Import(JSContext* cx, JS::HandleObject subtle, const JS::CallArgs& args, const std::string& name,
            JS::MutableHandleValue result) {
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
    if (kty != "oct" || !FromBase64Url(k, &raw) || (hasAlg && ValidLength(raw.size()) && alg != JwkAlg(name, raw.size())))
      return SubtleError(cx, subtle, "DataError");
  } else {
    return SubtleError(cx, subtle, "NotSupportedError");
  }
  return AesKeyFromRaw(cx, subtle, name, raw, JS::ToBoolean(args.get(3)), args.get(4), result);
}
bool Export(JSContext* cx, JS::HandleObject subtle, const JS::CallArgs& args, const std::string& name,
            JS::MutableHandleValue result) {
  std::string format;
  JS::RootedString text(cx, JS::ToString(cx, args.get(0)));
  JS::RootedObject key(cx, &args[1].toObject());
  std::vector<uint8_t> raw;
  if (!text || !ToUTF8(cx, text, &format)) return false;
  if (!JS::GetReservedSlot(key, KeyExtractable).toBoolean()) return SubtleError(cx, subtle, "InvalidAccessError");
  if (!ReadKeyMaterial(cx, key, &raw)) return SubtleError(cx, subtle, "OperationError");
  if (format == "raw") {
    JS::RootedObject buffer(cx, RsaBuffer(cx, raw));
    if (!buffer) return false;
    result.setObject(*buffer);
    return true;
  }
  if (format != "jwk") return SubtleError(cx, subtle, "NotSupportedError");
  JS::RootedObject jwk(cx, JS_NewPlainObject(cx)), ops(cx, KeyOps(cx, key));
  if (!jwk || !ops || !DefineString(cx, jwk, "alg", JwkAlg(name, raw.size())) ||
      !JS_DefineProperty(cx, jwk, "ext", JS::TrueHandleValue, JSPROP_ENUMERATE) || !DefineString(cx, jwk, "k", Base64Url(raw)) ||
      !JS_DefineProperty(cx, jwk, "key_ops", ops, JSPROP_ENUMERATE) || !DefineString(cx, jwk, "kty", "oct")) return false;
  result.setObject(*jwk);
  return true;
}
}  // namespace
bool AesKeyFromRaw(JSContext* cx, JS::HandleObject subtle, const std::string& name,
                   const std::vector<uint8_t>& raw, bool extractable, JS::HandleValue requested,
                   JS::MutableHandleValue result) {
  JS::RootedObject usages(cx);
  uint32_t count = 0;
  if (!ReadKeyUsages(cx, subtle, requested, {"encrypt", "decrypt", "wrapKey", "unwrapKey"},
                 {"encrypt", "decrypt", "wrapKey", "unwrapKey"}, &usages, &count)) return false;
  if (!ValidLength(raw.size())) return SubtleError(cx, subtle, "DataError");
  if (!count) return SubtleError(cx, subtle, "SyntaxError");
  JS::RootedObject key(cx, NewAesKey(cx, subtle, name, extractable, raw, usages));
  if (!key) return false;
  result.setObject(*key);
  return true;
}
SubtleStatus AesOperation(JSContext* cx, JS::HandleObject subtle, unsigned operation,
                          const JS::CallArgs& args, JS::MutableHandleValue result) {
  std::string name;
  bool ok = false;
  switch (operation) {
    case kEncrypt: case kDecrypt: case kGenerate: ok = CryptoAlgorithmName(cx, args.get(0), &name); break;
    case kImport: ok = CryptoAlgorithmName(cx, args.get(2), &name); break;
    case kExport: ok = KeyAlgorithmName(cx, args.get(1), &name); break;
    default: return SubtleStatus::kUnsupported;
  }
  if (!ok) return SubtleStatus::kFailed;
  if (!IsAes(name)) return SubtleStatus::kUnsupported;
  switch (operation) {
    case kEncrypt: case kDecrypt: ok = Crypt(cx, subtle, args, name, operation == kEncrypt, result); break;
    case kGenerate: ok = Generate(cx, subtle, args, name, result); break;
    case kImport: ok = Import(cx, subtle, args, name, result); break;
    case kExport: ok = Export(cx, subtle, args, name, result); break;
  }
  return ok ? SubtleStatus::kHandled : SubtleStatus::kFailed;
}
}  // namespace ruyijs::web_crypto
