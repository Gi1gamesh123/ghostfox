#include "web_crypto/rsa.h"

#include "runtime/runtime.h"

#include "domain/crypto/rsa_spki.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"

namespace ruyijs::web_crypto {
namespace {

namespace rsa = ruyijs::domain::crypto;

bool Format(JSContext* cx, JS::HandleValue value, std::string* format) {
  JS::RootedString text(cx, JS::ToString(cx, value));
  return text && ToUTF8(cx, text, format);
}

// RSA-OAEP usages: a public key encrypts and wraps, a private key decrypts and
// unwraps. Any other usage is a SyntaxError for this algorithm.
bool ReadUsages(JSContext* cx, JS::HandleObject subtle, JS::HandleValue value,
                bool public_key, JS::MutableHandleObject out) {
  if (!value.isObject())
    return CryptoTypeError(cx, "WebCrypto key usages must be a sequence");
  JS::RootedObject source(cx, &value.toObject());
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, source, &length)) return false;
  JS::RootedObject usages(cx, JS::NewArrayObject(cx, 0));
  if (!usages) return false;
  uint32_t used = 0;
  for (uint32_t i = 0; i < length; ++i) {
    JS::RootedValue element(cx);
    if (!JS_GetElement(cx, source, i, &element)) return false;
    JS::RootedString text(cx, JS::ToString(cx, element));
    std::string name;
    if (!text || !ToUTF8(cx, text, &name)) return false;
    const bool encrypt = name == "encrypt" || name == "wrapKey";
    const bool decrypt = name == "decrypt" || name == "unwrapKey";
    if (!encrypt && !decrypt) return SubtleError(cx, subtle, "SyntaxError");
    if (encrypt != public_key) continue;
    JS::RootedValue entry(cx, JS::StringValue(text));
    if (!JS_DefineElement(cx, usages, used++, entry, JSPROP_ENUMERATE)) return false;
  }
  out.set(usages);
  return true;
}

}  // namespace

bool RsaGenerateOperation(JSContext* cx, JS::HandleObject subtle, const JS::CallArgs& args,
                          JS::MutableHandleValue result) {
  std::string hash;
  if (!ReadRsaOaepHash(cx, args.get(0), &hash)) return false;
  if (hash.empty()) return false;  // declined: the caller rejects NotSupportedError
  JS::RootedObject dictionary(cx, &args[0].toObject());
  JS::RootedValue modulus(cx), exponent(cx);
  if (!JS_GetProperty(cx, dictionary, "modulusLength", &modulus) ||
      !JS_GetProperty(cx, dictionary, "publicExponent", &exponent))
    return false;
  double bits = 0;
  if (!JS::ToNumber(cx, modulus, &bits)) return false;
  std::vector<uint8_t> exponentBytes;
  if (!RsaBytes(cx, exponent, &exponentBytes)) return false;
  const bool extractable = JS::ToBoolean(args.get(1));
  JS::RootedObject publicUsages(cx), privateUsages(cx);
  if (!ReadUsages(cx, subtle, args.get(2), true, &publicUsages) ||
      !ReadUsages(cx, subtle, args.get(2), false, &privateUsages))
    return false;
  if (bits != double(uint32_t(bits)) || uint32_t(bits) % 8 != 0 ||
      uint32_t(bits) < rsa::kMinRsaBits || uint32_t(bits) > rsa::kMaxRsaBits)
    return SubtleError(cx, subtle, "OperationError");
  rsa::RsaKey key;
  if (!rsa::GenerateRsaKey(uint32_t(bits), exponentBytes, RsaRandom, &key))
    return SubtleError(cx, subtle, "OperationError");
  JS::RootedObject publicKey(cx, NewRsaKey(cx, subtle, "public", extractable, key, publicUsages, hash));
  JS::RootedObject privateKey(cx,
                              NewRsaKey(cx, subtle, "private", extractable, key, privateUsages, hash));
  JS::RootedObject pair(cx, JS_NewPlainObject(cx));
  if (!publicKey || !privateKey || !pair) return false;
  // CryptoKeyPair is a dictionary: members materialize in lexicographic order.
  if (!JS_DefineProperty(cx, pair, "privateKey", privateKey, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, pair, "publicKey", publicKey, JSPROP_ENUMERATE))
    return false;
  result.setObject(*pair);
  return true;
}

bool RsaImportOperation(JSContext* cx, JS::HandleObject subtle, const JS::CallArgs& args,
                        JS::MutableHandleValue result) {
  std::string format;
  if (!Format(cx, args.get(0), &format) || format != "spki") return false;
  std::string hash;
  if (!ReadRsaOaepHash(cx, args.get(2), &hash) || hash.empty()) return false;
  std::vector<uint8_t> der;
  if (!RsaBytes(cx, args.get(1), &der)) return false;
  rsa::RsaKey key;
  if (!rsa::DecodeSpki(der.data(), der.size(), &key)) return SubtleError(cx, subtle, "DataError");
  const bool extractable = JS::ToBoolean(args.get(3));
  JS::RootedObject usages(cx);
  if (!ReadUsages(cx, subtle, args.get(4), true, &usages)) return false;
  JS::RootedObject imported(cx, NewRsaKey(cx, subtle, "public", extractable, key, usages, hash));
  if (!imported) return false;
  result.setObject(*imported);
  return true;
}

bool RsaExportOperation(JSContext* cx, JS::HandleObject subtle, const JS::CallArgs& args,
                        JS::MutableHandleValue result) {
  std::string format;
  if (!Format(cx, args.get(0), &format) || format != "spki") return false;
  if (!args.get(1).isObject() || JS::GetClass(&args[1].toObject()) != &kCryptoKeyClass)
    return CryptoTypeError(cx, "exportKey requires a CryptoKey");
  JS::RootedObject key(cx, &args[1].toObject());
  std::string type;
  bool extractable = false;
  if (!RsaKeyType(cx, key, &type, &extractable)) return false;
  if (type != "public" || !extractable) return SubtleError(cx, subtle, "InvalidAccessError");
  rsa::RsaKey material;
  if (!ReadRsaKey(cx, key, &material)) return SubtleError(cx, subtle, "OperationError");
  std::vector<uint8_t> der;
  if (!rsa::EncodeSpki(material, &der)) return SubtleError(cx, subtle, "OperationError");
  JS::RootedObject buffer(cx, RsaBuffer(cx, der));
  if (!buffer) return false;
  result.setObject(*buffer);
  return true;
}

}  // namespace ruyijs::web_crypto
