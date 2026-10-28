#include "web_crypto/ec.h"
#include "web_crypto/cng_backend.h"
#include "web_crypto/crypto_key.h"
#include "runtime/runtime.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
namespace ruyijs::web_crypto {
namespace ec = ruyijs::domain::crypto;
namespace {
enum { kSign = 2, kVerify = 3, kGenerate = 5, kDeriveKey = 6, kDeriveBits = 7, kImport = 8, kExport = 9 };
bool IsEc(const std::string& name) { return name == "ECDSA" || name == "ECDH"; }
// The key argument as a CryptoKey of `name` with `usage`; InvalidAccessError otherwise.
bool UsableKey(JSContext* cx, JS::HandleObject subtle, JS::HandleValue value, const char* name, const char* usage,
               JS::MutableHandleObject key, ec::EcKey* material) {
  if (!value.isObject() || JS::GetClass(&value.toObject()) != &kCryptoKeyClass)
    return CryptoTypeError(cx, "WebCrypto requires a CryptoKey");
  key.set(&value.toObject());
  std::string algorithm;
  bool present = false;
  if (!KeyAlgorithmName(cx, value, &algorithm) || !HasUsage(cx, key, usage, &present)) return false;
  if (algorithm != name || !present) return SubtleError(cx, subtle, "InvalidAccessError");
  return ReadEcKey(cx, key, material) || SubtleError(cx, subtle, "OperationError");
}
bool Generate(JSContext* cx, JS::HandleObject subtle, const JS::CallArgs& args, bool ecdh, JS::MutableHandleValue result) {
  std::string curveName;
  bool present = false;
  if (!StringMember(cx, args.get(0), "namedCurve", &curveName, &present)) return false;
  if (!present) return CryptoTypeError(cx, "EcKeyGenParams requires namedCurve");
  JS::RootedObject publicUsages(cx), privateUsages(cx);
  uint32_t publicCount = 0, privateCount = 0;
  if (ecdh) {
    if (!ReadKeyUsages(cx, subtle, args.get(2), {"deriveKey", "deriveBits"}, {}, &publicUsages, &publicCount) ||
        !ReadKeyUsages(cx, subtle, args.get(2), {"deriveKey", "deriveBits"}, {"deriveKey", "deriveBits"}, &privateUsages, &privateCount))
      return false;
  } else if (!ReadKeyUsages(cx, subtle, args.get(2), {"sign", "verify"}, {"verify"}, &publicUsages, &publicCount) ||
             !ReadKeyUsages(cx, subtle, args.get(2), {"sign", "verify"}, {"sign"}, &privateUsages, &privateCount)) {
    return false;
  }
  const ec::EcCurve* curve = ec::EcCurveNamed(curveName);
  if (!curve) return SubtleError(cx, subtle, "NotSupportedError");
  if (!privateCount) return SubtleError(cx, subtle, "SyntaxError");
  ec::EcKey key;
  if (!EcGenerate(*curve, ecdh, &key)) return SubtleError(cx, subtle, "OperationError");
  ec::EcKey publicPart{key.curve, key.x, key.y, {}};
  const bool extractable = JS::ToBoolean(args.get(1));
  JS::RootedObject publicKey(cx, NewEcKey(cx, subtle, ecdh, extractable, publicPart, publicUsages));
  JS::RootedObject privateKey(cx, NewEcKey(cx, subtle, ecdh, extractable, key, privateUsages));
  JS::RootedObject pair(cx, JS_NewPlainObject(cx));
  // CryptoKeyPair is a dictionary: members materialize in lexicographic order.
  if (!publicKey || !privateKey || !pair || !JS_DefineProperty(cx, pair, "privateKey", privateKey, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, pair, "publicKey", publicKey, JSPROP_ENUMERATE)) return false;
  result.setObject(*pair);
  return true;
}
bool Sign(JSContext* cx, JS::HandleObject subtle, const JS::CallArgs& args, bool verify, JS::MutableHandleValue result) {
  HashAlgorithm hash = HashAlgorithm::Sha256;
  bool known = false;
  if (!ReadHash(cx, args.get(0), &hash, &known)) return false;
  if (!known) return SubtleError(cx, subtle, "NotSupportedError");
  JS::RootedObject key(cx);
  ec::EcKey material;
  if (!UsableKey(cx, subtle, args.get(1), "ECDSA", verify ? "verify" : "sign", &key, &material)) return false;
  std::vector<uint8_t> data, signature;
  if (!RsaBytes(cx, args.get(verify ? 3 : 2), &data) || (verify && !RsaBytes(cx, args.get(2), &signature))) return false;
  if (verify) {
    bool valid = false;
    if (!EcdsaVerify(material, hash, data, signature, &valid)) return SubtleError(cx, subtle, "OperationError");
    result.setBoolean(valid);
    return true;
  }
  std::vector<uint8_t> out;
  if (!EcdsaSign(material, hash, data, &out)) return SubtleError(cx, subtle, "OperationError");
  JS::RootedObject buffer(cx, RsaBuffer(cx, out));
  if (!buffer) return false;
  result.setObject(*buffer);
  return true;
}
// ECDH: the shared secret of the base key and algorithm.public, truncated to `bits`.
bool Secret(JSContext* cx, JS::HandleObject subtle, const JS::CallArgs& args, const char* usage,
            std::vector<uint8_t>* secret) {
  JS::RootedObject base(cx), peer(cx);
  ec::EcKey mine, theirs;
  if (!UsableKey(cx, subtle, args.get(1), "ECDH", usage, &base, &mine)) return false;
  if (!args.get(0).isObject()) return CryptoTypeError(cx, "EcdhKeyDeriveParams requires public");
  JS::RootedObject params(cx, &args[0].toObject());
  JS::RootedValue publicValue(cx);
  std::string algorithm, type;
  bool extractable = false;
  if (!JS_GetProperty(cx, params, "public", &publicValue)) return false;
  if (!publicValue.isObject() || JS::GetClass(&publicValue.toObject()) != &kCryptoKeyClass)
    return CryptoTypeError(cx, "EcdhKeyDeriveParams.public must be a CryptoKey");
  peer = &publicValue.toObject();
  if (!KeyAlgorithmName(cx, publicValue, &algorithm) || !RsaKeyType(cx, peer, &type, &extractable)) return false;
  if (algorithm != "ECDH" || type != "public" || !ReadEcKey(cx, peer, &theirs) || theirs.curve != mine.curve)
    return SubtleError(cx, subtle, "InvalidAccessError");
  return EcdhSecret(mine, theirs, secret) || SubtleError(cx, subtle, "OperationError");
}
bool DeriveBits(JSContext* cx, JS::HandleObject subtle, const JS::CallArgs& args, JS::MutableHandleValue result) {
  std::vector<uint8_t> secret;
  if (!Secret(cx, subtle, args, "deriveBits", &secret)) return false;
  if (!args.get(2).isNullOrUndefined()) {
    double bits = 0;
    if (!JS::ToNumber(cx, args[2], &bits)) return false;
    if (bits < 0 || bits > secret.size() * 8.0 || bits != double(uint32_t(bits))) return SubtleError(cx, subtle, "OperationError");
    const uint32_t length = uint32_t(bits);
    secret.resize((length + 7) / 8);
    if (length % 8) secret.back() &= uint8_t(0xff << (8 - length % 8));
  }
  JS::RootedObject buffer(cx, RsaBuffer(cx, secret));
  if (!buffer) return false;
  result.setObject(*buffer);
  return true;
}
bool DeriveKey(JSContext* cx, JS::HandleObject subtle, const JS::CallArgs& args, JS::MutableHandleValue result) {
  std::string target;
  if (!CryptoAlgorithmName(cx, args.get(2), &target)) return false;
  if (target != "AES-CBC" && target != "AES-GCM") return SubtleError(cx, subtle, "NotSupportedError");
  JS::RootedObject type(cx, args.get(2).isObject() ? &args[2].toObject() : nullptr);
  JS::RootedValue lengthValue(cx);
  double bits = 0;
  if (!type || !JS_GetProperty(cx, type, "length", &lengthValue) || !JS::ToNumber(cx, lengthValue, &bits)) return false;
  if (bits != 128 && bits != 192 && bits != 256) return SubtleError(cx, subtle, "OperationError");
  std::vector<uint8_t> secret;
  if (!Secret(cx, subtle, args, "deriveKey", &secret)) return false;
  if (secret.size() * 8 < bits) return SubtleError(cx, subtle, "OperationError");
  secret.resize(size_t(bits) / 8);
  return AesKeyFromRaw(cx, subtle, target, secret, JS::ToBoolean(args.get(3)), args.get(4), result);
}
}  // namespace
SubtleStatus EcOperation(JSContext* cx, JS::HandleObject subtle, unsigned operation,
                         const JS::CallArgs& args, JS::MutableHandleValue result) {
  std::string name;
  bool ok = true;
  switch (operation) {
    case kGenerate: case kSign: case kVerify: case kDeriveBits: case kDeriveKey:
      if (!CryptoAlgorithmName(cx, args.get(0), &name)) return SubtleStatus::kFailed;
      break;
    case kImport:
      if (!CryptoAlgorithmName(cx, args.get(2), &name)) return SubtleStatus::kFailed;
      break;
    case kExport:
      if (!KeyAlgorithmName(cx, args.get(1), &name)) return SubtleStatus::kFailed;
      break;
    default: return SubtleStatus::kUnsupported;
  }
  if (!IsEc(name)) return SubtleStatus::kUnsupported;
  const bool ecdh = name == "ECDH";
  if ((operation == kSign || operation == kVerify) && ecdh) return SubtleStatus::kUnsupported;
  if ((operation == kDeriveBits || operation == kDeriveKey) && !ecdh) return SubtleStatus::kUnsupported;
  switch (operation) {
    case kGenerate: ok = Generate(cx, subtle, args, ecdh, result); break;
    case kSign: case kVerify: ok = Sign(cx, subtle, args, operation == kVerify, result); break;
    case kDeriveBits: ok = DeriveBits(cx, subtle, args, result); break;
    case kDeriveKey: ok = DeriveKey(cx, subtle, args, result); break;
    case kImport: ok = EcImport(cx, subtle, args, ecdh, result); break;
    case kExport: {
      std::string format;
      JS::RootedString text(cx, JS::ToString(cx, args.get(0)));
      JS::RootedObject key(cx, &args[1].toObject());
      ok = text && ToUTF8(cx, text, &format) && EcExport(cx, subtle, format, key, result);
      break;
    }
  }
  return ok ? SubtleStatus::kHandled : SubtleStatus::kFailed;
}
}  // namespace ruyijs::web_crypto
