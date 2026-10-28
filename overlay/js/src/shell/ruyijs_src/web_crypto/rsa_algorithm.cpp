#include "web_crypto/rsa.h"

#include <cstring>

#include "mozilla/RandomNum.h"
#include "runtime/runtime.h"
#include "web_crypto/digest_backend.h"
#include "js/ArrayBuffer.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"

namespace ruyijs::web_crypto {
namespace {
const char* const kOaep = "RSA-OAEP";
}  // namespace

bool CryptoAlgorithmName(JSContext* cx, JS::HandleValue input, std::string* name) {
  JS::RootedValue value(cx, input);
  if (value.isObject()) {
    JS::RootedObject dictionary(cx, &value.toObject());
    if (!JS_GetProperty(cx, dictionary, "name", &value)) return false;
    if (value.isUndefined() || value.isNull())
      return CryptoTypeError(cx, "WebCrypto algorithm requires a name");
  }
  JS::RootedString text(cx, JS::ToString(cx, value));
  if (!text || !ToUTF8(cx, text, name)) return false;
  for (char& c : *name)
    if (c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
  return true;
}

bool ReadRsaOaepName(JSContext* cx, JS::HandleValue algorithm, bool* supported) {
  std::string name;
  *supported = false;
  if (!CryptoAlgorithmName(cx, algorithm, &name)) return false;
  *supported = name == kOaep;
  return true;
}

bool ReadRsaOaepHash(JSContext* cx, JS::HandleValue algorithm, std::string* out) {
  std::string name;
  out->clear();
  if (!CryptoAlgorithmName(cx, algorithm, &name) || name != kOaep) return true;
  if (!algorithm.isObject()) return true;  // the hash member is required here
  JS::RootedObject dictionary(cx, &algorithm.toObject());
  JS::RootedValue hash(cx);
  if (!JS_GetProperty(cx, dictionary, "hash", &hash)) return false;
  std::string hashName;
  if (!CryptoAlgorithmName(cx, hash, &hashName)) return false;
  if (OaepHashFor(hashName).size) *out = hashName;
  return true;
}

namespace {
template <HashAlgorithm A>
bool Digest(const void* input, size_t length, uint8_t* out) {
  DigestBytes bytes;
  if (!ComputeDigest(A, input, length, &bytes)) return false;
  std::memcpy(out, bytes.data, bytes.size);
  return true;
}
}  // namespace

domain::crypto::OaepHash OaepHashFor(const std::string& name) {
  if (name == "SHA-1") return {&Digest<HashAlgorithm::Sha1>, 20};
  if (name == "SHA-256") return {&Digest<HashAlgorithm::Sha256>, 32};
  if (name == "SHA-384") return {&Digest<HashAlgorithm::Sha384>, 48};
  if (name == "SHA-512") return {&Digest<HashAlgorithm::Sha512>, 64};
  return {};
}

bool RsaBytes(JSContext* cx, JS::HandleValue value, std::vector<uint8_t>* out) {
  JS::RootedObject buffer(cx);
  if (!CryptoBuffer(cx, value, &buffer)) return false;
  CryptoBytes bytes;
  if (!CryptoSnapshot(cx, buffer, &bytes)) return false;
  if (bytes.size) out->assign(bytes.data.get(), bytes.data.get() + bytes.size);
  else out->clear();
  return true;
}

bool RsaRandom(uint8_t* out, size_t size) {
  return mozilla::GenerateRandomBytesFromOS(out, size);
}

JSObject* RsaBuffer(JSContext* cx, const std::vector<uint8_t>& bytes) {
  JS::RootedObject buffer(cx, JS::NewArrayBuffer(cx, bytes.size()));
  if (!buffer) return nullptr;
  bool shared = false;
  void* data = nullptr;
  {
    JS::AutoCheckCannotGC nogc;
    data = JS::GetArrayBufferData(buffer, &shared, nogc);
  }
  if (!data && !bytes.empty()) return nullptr;
  if (!bytes.empty()) std::memcpy(data, bytes.data(), bytes.size());
  return buffer;
}

}  // namespace ruyijs::web_crypto
