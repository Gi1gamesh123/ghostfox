#ifndef RUYIJS_DOMAIN_CRYPTO_RSA_OAEP_H
#define RUYIJS_DOMAIN_CRYPTO_RSA_OAEP_H
// RSAES-OAEP (RFC 8017, 7.1) over the domain key values, with the key's hash
// (WebCrypto RSA-OAEP: SHA-1, SHA-256, SHA-384 or SHA-512) used for both the
// label hash and MGF1. The hash is a callback so the caller keeps the digest
// backend it already has, and the random seed comes from the caller's random
// source; nothing here is platform specific.
#include <cstddef>
#include <cstdint>
#include <vector>

#include "domain/crypto/rsa_key.h"

namespace ruyijs::domain::crypto {

inline constexpr size_t kMaxHashBytes = 64;

// A digest function and its output length (hLen).
struct OaepHash {
  bool (*digest)(const void* input, size_t length, uint8_t* out) = nullptr;
  size_t size = 0;
};

inline bool Mgf1(const OaepHash& sha, const uint8_t* seed, size_t seed_size, uint8_t* out,
                 size_t size) {
  std::vector<uint8_t> block(seed_size + 4);
  for (size_t i = 0; i < seed_size; ++i) block[i] = seed[i];
  uint8_t digest[kMaxHashBytes];
  for (uint32_t counter = 0; size;) {
    block[seed_size] = uint8_t(counter >> 24);
    block[seed_size + 1] = uint8_t(counter >> 16);
    block[seed_size + 2] = uint8_t(counter >> 8);
    block[seed_size + 3] = uint8_t(counter);
    if (!sha.digest(block.data(), block.size(), digest)) return false;
    const size_t take = size < sha.size ? size : sha.size;
    for (size_t i = 0; i < take; ++i) out[i] = digest[i];
    out += take;
    size -= take;
    ++counter;
  }
  return true;
}

// EME-OAEP encoding of a message into an encoded_size-byte block (emLen = k).
inline bool OaepEncode(const OaepHash& sha, const uint8_t* message, size_t size,
                       size_t encoded_size, RandomBytes random,
                       std::vector<uint8_t>* out) {
  if (encoded_size < 2 * sha.size + 2) return false;
  if (size > encoded_size - 2 * sha.size - 2) return false;
  const size_t db_size = encoded_size - sha.size - 1;
  const uint8_t empty = 0;
  std::vector<uint8_t> db(db_size, 0), seed(sha.size);
  std::vector<uint8_t> mask(db_size > sha.size ? db_size : sha.size, 0);
  if (!sha.digest(&empty, 0, db.data())) return false;  // lHash over the empty label
  db[db_size - size - 1] = 0x01;
  for (size_t i = 0; i < size; ++i) db[db_size - size + i] = message[i];
  if (!random(seed.data(), seed.size())) return false;
  if (!Mgf1(sha, seed.data(), seed.size(), mask.data(), db_size)) return false;
  for (size_t i = 0; i < db_size; ++i) db[i] ^= mask[i];
  if (!Mgf1(sha, db.data(), db_size, mask.data(), sha.size)) return false;
  for (size_t i = 0; i < sha.size; ++i) seed[i] ^= mask[i];
  out->assign(encoded_size, 0);
  (*out)[0] = 0;
  for (size_t i = 0; i < sha.size; ++i) (*out)[1 + i] = seed[i];
  for (size_t i = 0; i < db_size; ++i) (*out)[1 + sha.size + i] = db[i];
  return true;
}

// EME-OAEP decoding; false when the block does not decode (the caller reports
// OperationError, as Firefox does for a bad padding).
inline bool OaepDecode(const OaepHash& sha, const uint8_t* encoded, size_t size,
                       std::vector<uint8_t>* out) {
  if (size < 2 * sha.size + 2 || encoded[0] != 0) return false;
  const size_t db_size = size - sha.size - 1;
  const uint8_t empty = 0;
  std::vector<uint8_t> label_hash(sha.size, 0), seed(sha.size), db(db_size, 0);
  std::vector<uint8_t> mask(db_size > sha.size ? db_size : sha.size, 0);
  if (!sha.digest(&empty, 0, label_hash.data())) return false;
  for (size_t i = 0; i < sha.size; ++i) seed[i] = encoded[1 + i];
  for (size_t i = 0; i < db_size; ++i) db[i] = encoded[1 + sha.size + i];
  if (!Mgf1(sha, db.data(), db_size, mask.data(), sha.size)) return false;
  for (size_t i = 0; i < sha.size; ++i) seed[i] ^= mask[i];
  if (!Mgf1(sha, seed.data(), seed.size(), mask.data(), db_size)) return false;
  for (size_t i = 0; i < db_size; ++i) db[i] ^= mask[i];
  for (size_t i = 0; i < sha.size; ++i)
    if (db[i] != label_hash[i]) return false;
  size_t at = sha.size;
  while (at < db_size && db[at] == 0) ++at;
  if (at == db_size || db[at] != 0x01) return false;
  ++at;
  out->assign(db.begin() + long(at), db.end());
  return true;
}

// c = m^exponent mod n, then the block is written with k = |n| bytes.
inline bool RsaApply(const RsaKey& key, const std::vector<uint8_t>& exponent,
                     const uint8_t* input, size_t size, std::vector<uint8_t>* out) {
  if (key.modulus.empty() || exponent.empty()) return false;
  BigUInt modulus, power, value, result;
  if (!modulus.FromBytes(key.modulus.data(), key.modulus.size()) ||
      !power.FromBytes(exponent.data(), exponent.size()) ||
      !value.FromBytes(input, size))
    return false;
  if (value.Compare(modulus) >= 0) return false;
  if (!ModPow(value, power, modulus, &result)) return false;
  out->assign(key.modulus.size(), 0);
  result.ToBytes(out->data(), out->size());
  return true;
}

inline bool RsaOaepEncrypt(const RsaKey& key, const OaepHash& sha, const uint8_t* message,
                           size_t size, RandomBytes random,
                           std::vector<uint8_t>* out) {
  std::vector<uint8_t> encoded;
  if (!OaepEncode(sha, message, size, key.modulus.size(), random, &encoded)) return false;
  return RsaApply(key, key.exponent, encoded.data(), encoded.size(), out);
}

// Decryption uses d only: no CRT, because the adapter never claims the key was
// generated by someone else's library.
inline bool RsaOaepDecrypt(const RsaKey& key, const OaepHash& sha, const uint8_t* ciphertext,
                           size_t size, std::vector<uint8_t>* out) {
  if (!key.HasPrivate()) return false;
  std::vector<uint8_t> encoded;
  if (!RsaApply(key, key.private_exponent, ciphertext, size, &encoded)) return false;
  return OaepDecode(sha, encoded.data(), encoded.size(), out);
}

}  // namespace ruyijs::domain::crypto
#endif
