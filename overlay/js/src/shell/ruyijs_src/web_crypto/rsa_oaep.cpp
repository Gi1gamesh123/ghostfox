#include "web_crypto/rsa.h"

#include "domain/crypto/rsa_oaep.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::web_crypto {
namespace {

namespace rsa = ruyijs::domain::crypto;

// The key's usages live in its own reserved slot; there is no C++ side table.
bool Usage(JSContext* cx, JS::HandleObject key, const char* name, bool* present) {
  JS::RootedValue usages(cx, JS::GetReservedSlot(key, KeyUsages));
  if (!usages.isObject())
    return CryptoTypeError(cx, "WebCrypto key has no usage list");
  JS::RootedObject list(cx, &usages.toObject());
  return RsaUsage(cx, list, name, present);
}

// encrypt/decrypt share everything except the key type, the usage and which
// direction of the RSAES-OAEP primitive runs.
bool OaepOperation(JSContext* cx, JS::HandleObject subtle, const JS::CallArgs& args,
                   bool encrypt, JS::MutableHandleValue result) {
  bool supported = false;
  if (!ReadRsaOaepName(cx, args.get(0), &supported)) return false;
  if (!supported) return false;  // declined: the caller rejects NotSupportedError
  if (!args.get(1).isObject() || JS::GetClass(&args[1].toObject()) != &kCryptoKeyClass)
    return CryptoTypeError(cx, "WebCrypto operation requires a CryptoKey");
  JS::RootedObject key(cx, &args[1].toObject());
  std::string type;
  bool extractable = false;
  if (!RsaKeyType(cx, key, &type, &extractable)) return false;
  bool usable = false;
  if (!Usage(cx, key, encrypt ? "encrypt" : "decrypt", &usable)) return false;
  if (type != (encrypt ? "public" : "private") || !usable)
    return SubtleError(cx, subtle, "InvalidAccessError");
  rsa::RsaKey material;
  if (!ReadRsaKey(cx, key, &material)) return SubtleError(cx, subtle, "OperationError");
  std::vector<uint8_t> input;
  if (!RsaBytes(cx, args.get(2), &input)) return false;
  // The key's own hash (CryptoKey.algorithm.hash) drives the label hash and MGF1.
  std::string hashName;
  if (!RsaKeyHash(cx, key, &hashName)) return SubtleError(cx, subtle, "OperationError");
  const rsa::OaepHash hash = OaepHashFor(hashName);
  if (!hash.size) return SubtleError(cx, subtle, "OperationError");
  std::vector<uint8_t> output;
  const bool ok = encrypt
      ? rsa::RsaOaepEncrypt(material, hash, input.data(), input.size(), RsaRandom, &output)
      : rsa::RsaOaepDecrypt(material, hash, input.data(), input.size(), &output);
  if (!ok) return SubtleError(cx, subtle, "OperationError");
  JS::RootedObject buffer(cx, RsaBuffer(cx, output));
  if (!buffer) return false;
  result.setObject(*buffer);
  return true;
}

}  // namespace

bool RsaEncryptOperation(JSContext* cx, JS::HandleObject subtle, const JS::CallArgs& args,
                         JS::MutableHandleValue result) {
  return OaepOperation(cx, subtle, args, true, result);
}

bool RsaDecryptOperation(JSContext* cx, JS::HandleObject subtle, const JS::CallArgs& args,
                         JS::MutableHandleValue result) {
  return OaepOperation(cx, subtle, args, false, result);
}

}  // namespace ruyijs::web_crypto
