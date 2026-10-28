// The one CryptoKey factory and the dictionary/usage readers every key
// algorithm of this realm shares (RSA-OAEP, ECDSA/ECDH, AES-CBC/AES-GCM).
#ifndef RUYIJS_WEB_CRYPTO_CRYPTO_KEY_H
#define RUYIJS_WEB_CRYPTO_CRYPTO_KEY_H
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>
#include "web_crypto/subtle.h"
#include "web_crypto/digest_backend.h"
namespace ruyijs::web_crypto {
// A CryptoKey of the realm's interface: type, extractable, algorithm, usages
// and the opaque material blob the owning algorithm reads back.
JSObject* NewCryptoKey(JSContext* cx, JS::HandleObject subtle, const char* type, bool extractable,
                       JS::HandleObject algorithm, JS::HandleObject usages,
                       const std::vector<uint8_t>& material);
bool ReadKeyMaterial(JSContext* cx, JS::HandleObject key, std::vector<uint8_t>* out);
// key.algorithm.name, upper-cased; empty for a value that is not a CryptoKey.
bool KeyAlgorithmName(JSContext* cx, JS::HandleValue key, std::string* name);
// The usages a key keeps: every requested name must be in `allowed` (else
// SyntaxError); the kept ones (in `keep`) follow the order of `allowed`.
bool ReadKeyUsages(JSContext* cx, JS::HandleObject subtle, JS::HandleValue requested,
               std::initializer_list<const char*> allowed, std::initializer_list<const char*> keep,
               JS::MutableHandleObject out, uint32_t* count);
bool HasUsage(JSContext* cx, JS::HandleObject key, const char* name, bool* present);
bool StringMember(JSContext* cx, JS::HandleValue dictionary, const char* name, std::string* out, bool* present);
bool DefineString(JSContext* cx, JS::HandleObject target, const char* name, const std::string& value);
std::string Base64Url(const std::vector<uint8_t>& bytes);
bool FromBase64Url(const std::string& text, std::vector<uint8_t>* out);
// A hash dictionary member (string or {name}) as the digest backend's enum.
bool ReadHash(JSContext* cx, JS::HandleValue algorithm, HashAlgorithm* hash, bool* known);
}
#endif
