#include "web_crypto/rsa.h"
#include "web_crypto/crypto_key.h"

#include "mozilla/RandomNum.h"
#include "runtime/runtime.h"
#include "web_crypto/digest_backend.h"
#include "js/ArrayBuffer.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "js/experimental/TypedData.h"
#include "jsfriendapi.h"
#include <cstring>

namespace ruyijs::web_crypto {
namespace {

namespace rsa = ruyijs::domain::crypto;

const char* const kOaep = "RSA-OAEP";

bool Field(std::vector<uint8_t>* out, const std::vector<uint8_t>& value) {
  if (value.size() > 0xffff) return false;
  out->push_back(uint8_t(value.size() >> 8));
  out->push_back(uint8_t(value.size()));
  out->insert(out->end(), value.begin(), value.end());
  return true;
}

bool TakeField(const uint8_t* data, size_t size, size_t* at, std::vector<uint8_t>* out) {
  if (*at + 2 > size) return false;
  const size_t length = (size_t(data[*at]) << 8) | data[*at + 1];
  *at += 2;
  if (*at + length > size) return false;
  out->assign(data + *at, data + *at + length);
  *at += length;
  return true;
}

// The material blob: five length-prefixed big-endian fields (n, e, d, p, q).
bool Serialize(const rsa::RsaKey& key, std::vector<uint8_t>* out) {
  out->clear();
  return Field(out, key.modulus) && Field(out, key.exponent) &&
         Field(out, key.private_exponent) && Field(out, key.prime1) &&
         Field(out, key.prime2);
}

bool Parse(const uint8_t* data, size_t size, rsa::RsaKey* out) {
  size_t at = 0;
  return TakeField(data, size, &at, &out->modulus) &&
         TakeField(data, size, &at, &out->exponent) &&
         TakeField(data, size, &at, &out->private_exponent) &&
         TakeField(data, size, &at, &out->prime1) &&
         TakeField(data, size, &at, &out->prime2) && !out->modulus.empty();
}

bool SetString(JSContext* cx, JS::HandleObject target, const char* name, const char* value) {
  JS::RootedString text(cx, JS_NewStringCopyZ(cx, value));
  return text && JS_DefineProperty(cx, target, name, text, JSPROP_ENUMERATE);
}

// CryptoKey.algorithm for RSA-OAEP is an RsaHashedKeyAlgorithm dictionary:
// name, modulusLength, publicExponent, hash.
JSObject* AlgorithmObject(JSContext* cx, const rsa::RsaKey& key, const std::string& hashName) {
  JS::RootedObject algorithm(cx, JS_NewPlainObject(cx));
  JS::RootedObject hash(cx, JS_NewPlainObject(cx));
  JS::RootedObject exponent(cx, JS_NewUint8Array(cx, key.exponent.size()));
  if (!algorithm || !hash || !exponent || !SetString(cx, algorithm, "name", kOaep) ||
      !SetString(cx, hash, "name", hashName.c_str()) ||
      !JS_DefineProperty(cx, algorithm, "modulusLength",
                         uint32_t(key.modulus.size() * 8), JSPROP_ENUMERATE))
    return nullptr;
  if (!key.exponent.empty()) {
    bool shared = false;
    JS::AutoCheckCannotGC nogc;
    void* data = JS_GetArrayBufferViewData(exponent, &shared, nogc);
    if (!data) return nullptr;
    std::memcpy(data, key.exponent.data(), key.exponent.size());
  }
  if (!JS_DefineProperty(cx, algorithm, "publicExponent", exponent, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(cx, algorithm, "hash", hash, JSPROP_ENUMERATE))
    return nullptr;
  return algorithm;
}

}  // namespace



bool RsaKeyType(JSContext* cx, JS::HandleObject key, std::string* type, bool* extractable) {
  if (JS::GetClass(key) != &kCryptoKeyClass)
    return CryptoTypeError(cx, "WebCrypto requires a CryptoKey");
  JS::RootedValue value(cx, JS::GetReservedSlot(key, KeyType));
  JS::RootedString text(cx, value.isString() ? value.toString() : nullptr);
  if (!text || !ToUTF8(cx, text, type)) return false;
  *extractable = JS::GetReservedSlot(key, KeyExtractable).toBoolean();
  return true;
}



bool RsaUsage(JSContext* cx, JS::HandleObject usages, const char* name, bool* present) {
  uint32_t length = 0;
  *present = false;
  if (!JS::GetArrayLength(cx, usages, &length)) return false;
  for (uint32_t i = 0; i < length; ++i) {
    JS::RootedValue element(cx);
    if (!JS_GetElement(cx, usages, i, &element)) return false;
    JS::RootedString text(cx, JS::ToString(cx, element));
    bool equal = false;
    if (!text || !JS_StringEqualsAscii(cx, text, name, &equal)) return false;
    if (equal) { *present = true; return true; }
  }
  return true;
}

JSObject* NewRsaKey(JSContext* cx, JS::HandleObject subtle, const char* type,
                    bool extractable, const rsa::RsaKey& key, JS::HandleObject usages,
                    const std::string& hash) {
  JS::RootedObject algorithm(cx, AlgorithmObject(cx, key, hash));
  std::vector<uint8_t> material;
  if (!algorithm || !Serialize(key, &material)) return nullptr;
  return NewCryptoKey(cx, subtle, type, extractable, algorithm, usages, material);
}

bool RsaKeyHash(JSContext* cx, JS::HandleObject key, std::string* hash) {
  JS::RootedValue algorithm(cx, JS::GetReservedSlot(key, KeyAlgorithm)), value(cx);
  if (!algorithm.isObject()) return false;
  JS::RootedObject dictionary(cx, &algorithm.toObject());
  if (!JS_GetProperty(cx, dictionary, "hash", &value) || !value.isObject()) return false;
  dictionary = &value.toObject();
  if (!JS_GetProperty(cx, dictionary, "name", &value) || !value.isString()) return false;
  JS::RootedString text(cx, value.toString());
  return ToUTF8(cx, text, hash);
}

bool ReadRsaKey(JSContext* cx, JS::HandleObject key, rsa::RsaKey* out) {
  if (JS::GetClass(key) != &kCryptoKeyClass) return false;
  JS::RootedValue material(cx, JS::GetReservedSlot(key, KeyMaterial));
  if (!material.isObject() || !JS::IsArrayBufferObject(&material.toObject())) return false;
  bool shared = false;
  uint8_t* data = nullptr;
  size_t length = 0;
  {
    JS::AutoCheckCannotGC nogc;
    JS::GetArrayBufferLengthAndData(&material.toObject(), &length, &shared, &data);
  }
  return data && Parse(data, length, out);
}

SubtleStatus RsaOperation(JSContext* cx, JS::HandleObject subtle, unsigned operation,
                          const JS::CallArgs& args, JS::MutableHandleValue result) {
  bool handled = false;
  switch (operation) {
    case 0: handled = RsaEncryptOperation(cx, subtle, args, result); break;
    case 1: handled = RsaDecryptOperation(cx, subtle, args, result); break;
    case 5: handled = RsaGenerateOperation(cx, subtle, args, result); break;
    case 8: handled = RsaImportOperation(cx, subtle, args, result); break;
    case 9: handled = RsaExportOperation(cx, subtle, args, result); break;
    default: return SubtleStatus::kUnsupported;
  }
  if (handled) return SubtleStatus::kHandled;
  return JS_IsExceptionPending(cx) ? SubtleStatus::kFailed : SubtleStatus::kUnsupported;
}

}  // namespace ruyijs::web_crypto
