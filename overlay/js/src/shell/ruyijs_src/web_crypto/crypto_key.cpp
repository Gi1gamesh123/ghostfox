#include "web_crypto/crypto_key.h"
#include <cstring>
#include "runtime/runtime.h"
#include "web_crypto/rsa.h"
#include "js/Array.h"
#include "js/ArrayBuffer.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
namespace ruyijs::web_crypto {
JSObject* NewCryptoKey(JSContext* cx, JS::HandleObject subtle, const char* type, bool extractable,
                       JS::HandleObject algorithm, JS::HandleObject usages,
                       const std::vector<uint8_t>& material) {
  JS::RootedValue constructor(cx, JS::GetReservedSlot(subtle, SubtleKeyConstructor)), prototype(cx);
  JS::RootedObject target(cx, constructor.isObject() ? &constructor.toObject() : nullptr);
  if (!target || !JS_GetProperty(cx, target, "prototype", &prototype) || !prototype.isObject()) {
    CryptoTypeError(cx, "WebCrypto requires the realm CryptoKey interface");
    return nullptr;
  }
  JS::RootedObject proto(cx, &prototype.toObject());
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, &kCryptoKeyClass, proto));
  JS::RootedObject buffer(cx, RsaBuffer(cx, material));
  JS::RootedString text(cx, JS_NewStringCopyZ(cx, type));
  if (!object || !buffer || !text) return nullptr;
  JS::SetReservedSlot(object, KeyType, JS::StringValue(text));
  JS::SetReservedSlot(object, KeyExtractable, JS::BooleanValue(extractable));
  JS::SetReservedSlot(object, KeyAlgorithm, JS::ObjectValue(*algorithm));
  JS::SetReservedSlot(object, KeyUsages, JS::ObjectValue(*usages));
  JS::SetReservedSlot(object, KeyMaterial, JS::ObjectValue(*buffer));
  return object;
}
bool ReadKeyMaterial(JSContext* cx, JS::HandleObject key, std::vector<uint8_t>* out) {
  if (JS::GetClass(key) != &kCryptoKeyClass) return false;
  JS::RootedValue material(cx, JS::GetReservedSlot(key, KeyMaterial));
  if (!material.isObject() || !JS::IsArrayBufferObject(&material.toObject())) return false;
  bool shared = false; uint8_t* data = nullptr; size_t length = 0;
  JS::AutoCheckCannotGC nogc;
  JS::GetArrayBufferLengthAndData(&material.toObject(), &length, &shared, &data);
  if (!data && length) return false;
  out->assign(data, data + length);
  return true;
}
bool KeyAlgorithmName(JSContext* cx, JS::HandleValue key, std::string* name) {
  name->clear();
  if (!key.isObject() || JS::GetClass(&key.toObject()) != &kCryptoKeyClass) return true;
  JS::RootedValue algorithm(cx, JS::GetReservedSlot(&key.toObject(), KeyAlgorithm));
  return CryptoAlgorithmName(cx, algorithm, name);
}
bool ReadKeyUsages(JSContext* cx, JS::HandleObject subtle, JS::HandleValue requested,
               std::initializer_list<const char*> allowed, std::initializer_list<const char*> keep,
               JS::MutableHandleObject out, uint32_t* count) {
  if (!requested.isObject()) return CryptoTypeError(cx, "WebCrypto key usages must be a sequence");
  JS::RootedObject source(cx, &requested.toObject());
  uint32_t length = 0;
  if (!JS::GetArrayLength(cx, source, &length)) return false;
  std::vector<std::string> names;
  JS::RootedValue element(cx);
  for (uint32_t i = 0; i < length; ++i) {
    std::string name;
    JS::RootedString text(cx);
    if (!JS_GetElement(cx, source, i, &element) || !(text = JS::ToString(cx, element)) || !ToUTF8(cx, text, &name))
      return false;
    bool known = false;
    for (const char* a : allowed) known |= name == a;
    if (!known) return SubtleError(cx, subtle, "SyntaxError");
    names.push_back(std::move(name));
  }
  JS::RootedObject usages(cx, JS::NewArrayObject(cx, 0));
  if (!usages) return false;
  *count = 0;
  for (const char* a : allowed) {
    bool wanted = false, kept = false;
    for (const auto& name : names) wanted |= name == a;
    for (const char* k : keep) kept |= !std::strcmp(k, a);
    if (!wanted || !kept) continue;
    JS::RootedString text(cx, JS_NewStringCopyZ(cx, a));
    if (!text || !JS_DefineElement(cx, usages, (*count)++, text, JSPROP_ENUMERATE)) return false;
  }
  out.set(usages);
  return true;
}
bool HasUsage(JSContext* cx, JS::HandleObject key, const char* name, bool* present) {
  JS::RootedObject usages(cx, &JS::GetReservedSlot(key, KeyUsages).toObject());
  return RsaUsage(cx, usages, name, present);
}
bool StringMember(JSContext* cx, JS::HandleValue dictionary, const char* name, std::string* out, bool* present) {
  *present = false;
  if (!dictionary.isObject()) return true;
  JS::RootedObject object(cx, &dictionary.toObject());
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, object, name, &value)) return false;
  if (value.isUndefined()) return true;
  JS::RootedString text(cx, JS::ToString(cx, value));
  *present = true;
  return text && ToUTF8(cx, text, out);
}
bool DefineString(JSContext* cx, JS::HandleObject target, const char* name, const std::string& value) {
  JS::RootedString text(cx, JS_NewStringCopyN(cx, value.data(), value.size()));
  return text && JS_DefineProperty(cx, target, name, text, JSPROP_ENUMERATE);
}
std::string Base64Url(const std::vector<uint8_t>& bytes) {
  static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  std::string out;
  for (size_t i = 0; i < bytes.size(); i += 3) {
    uint32_t v = uint32_t(bytes[i]) << 16;
    if (i + 1 < bytes.size()) v |= uint32_t(bytes[i + 1]) << 8;
    if (i + 2 < bytes.size()) v |= bytes[i + 2];
    out += table[(v >> 18) & 63]; out += table[(v >> 12) & 63];
    if (i + 1 < bytes.size()) out += table[(v >> 6) & 63];
    if (i + 2 < bytes.size()) out += table[v & 63];
  }
  return out;
}
bool FromBase64Url(const std::string& text, std::vector<uint8_t>* out) {
  out->clear();
  uint32_t buffer = 0; int bits = 0;
  for (char c : text) {
    int v = c >= 'A' && c <= 'Z' ? c - 'A' : c >= 'a' && c <= 'z' ? c - 'a' + 26 :
            c >= '0' && c <= '9' ? c - '0' + 52 : c == '-' ? 62 : c == '_' ? 63 : -1;
    if (v < 0) return false;
    buffer = (buffer << 6) | uint32_t(v); bits += 6;
    if (bits >= 8) { bits -= 8; out->push_back(uint8_t(buffer >> bits)); }
  }
  return bits < 6;
}
bool ReadHash(JSContext* cx, JS::HandleValue algorithm, HashAlgorithm* hash, bool* known) {
  *known = false;
  if (!algorithm.isObject()) return true;
  JS::RootedObject dictionary(cx, &algorithm.toObject());
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, dictionary, "hash", &value)) return false;
  if (value.isUndefined()) return CryptoTypeError(cx, "WebCrypto algorithm requires a hash");
  std::string name;
  if (!CryptoAlgorithmName(cx, value, &name)) return false;
  *known = true;
  if (name == "SHA-1") *hash = HashAlgorithm::Sha1;
  else if (name == "SHA-256") *hash = HashAlgorithm::Sha256;
  else if (name == "SHA-384") *hash = HashAlgorithm::Sha384;
  else if (name == "SHA-512") *hash = HashAlgorithm::Sha512;
  else *known = false;
  return true;
}
}  // namespace ruyijs::web_crypto
