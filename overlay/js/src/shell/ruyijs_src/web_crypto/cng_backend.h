// Windows CNG (BCrypt) adapter for the WebCrypto AES and EC algorithms, beside
// the digest backend the realm already uses. Infrastructure only: byte vectors
// in and out, no engine or WebIDL dependency.
#ifndef RUYIJS_WEB_CRYPTO_CNG_BACKEND_H
#define RUYIJS_WEB_CRYPTO_CNG_BACKEND_H
#include <cstdint>
#include <vector>
#include "domain/crypto/ec_der.h"
#include "web_crypto/digest_backend.h"
namespace ruyijs::web_crypto {
using Bytes = std::vector<uint8_t>;
// AES-CBC with PKCS #7 padding (WebCrypto AES-CBC). False on a bad key/IV or,
// when decrypting, bad padding.
bool AesCbc(const Bytes& key, const Bytes& iv, const Bytes& input, bool encrypt, Bytes* out);
// AES-GCM; `tagBytes` 4..16. Encrypt returns ciphertext || tag; decrypt takes it.
bool AesGcm(const Bytes& key, const Bytes& iv, const Bytes& aad, size_t tagBytes,
            const Bytes& input, bool encrypt, Bytes* out);
// A fresh key pair of `curve` (ECDSA or ECDH provider).
bool EcGenerate(const domain::crypto::EcCurve& curve, bool ecdh, domain::crypto::EcKey* out);
// The public point is on the curve (CNG import accepts it).
bool EcValidate(const domain::crypto::EcKey& key, bool ecdh);
// ECDSA over the message digest; the signature is r || s (IEEE P1363).
bool EcdsaSign(const domain::crypto::EcKey& key, HashAlgorithm hash, const Bytes& message, Bytes* out);
bool EcdsaVerify(const domain::crypto::EcKey& key, HashAlgorithm hash, const Bytes& message,
                 const Bytes& signature, bool* valid);
// HMAC of `data` under `key` with the digest backend's hash.
bool Hmac(HashAlgorithm hash, const Bytes& key, const Bytes& data, Bytes* out);
// ECDH shared secret: the x coordinate, big-endian, field size bytes.
bool EcdhSecret(const domain::crypto::EcKey& privateKey, const domain::crypto::EcKey& publicKey, Bytes* out);
}
#endif
