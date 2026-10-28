#ifndef RUYIJS_DOMAIN_CRYPTO_RSA_SPKI_H
#define RUYIJS_DOMAIN_CRYPTO_RSA_SPKI_H
// SubjectPublicKeyInfo DER for an RSA public key (RFC 5280, 4.1 with the
// PKCS#1 rsaEncryption AlgorithmIdentifier). Export and import are exact
// inverses, which is what WebCrypto's "spki" key format requires.
#include <cstddef>
#include <cstdint>
#include <vector>

#include "domain/crypto/rsa_key.h"

namespace ruyijs::domain::crypto {

inline void PushLength(std::vector<uint8_t>* out, size_t length) {
  if (length < 0x80) {
    out->push_back(uint8_t(length));
    return;
  }
  uint8_t digits[sizeof(size_t)] = {};
  size_t count = 0;
  for (size_t rest = length; rest; rest >>= 8) digits[count++] = uint8_t(rest);
  out->push_back(uint8_t(0x80 | count));
  while (count) out->push_back(digits[--count]);
}

inline std::vector<uint8_t> DerWrap(uint8_t tag, const std::vector<uint8_t>& body) {
  std::vector<uint8_t> out;
  out.push_back(tag);
  PushLength(&out, body.size());
  out.insert(out.end(), body.begin(), body.end());
  return out;
}

// DER INTEGER of a non-negative big-endian value: a leading zero keeps the sign
// bit clear when the top bit of the value is set.
inline std::vector<uint8_t> DerInteger(const std::vector<uint8_t>& value) {
  std::vector<uint8_t> body;
  if (!value.empty() && (value.front() & 0x80)) body.push_back(0);
  body.insert(body.end(), value.begin(), value.end());
  return DerWrap(0x02, body);
}

inline bool EncodeSpki(const RsaKey& key, std::vector<uint8_t>* out) {
  if (key.modulus.empty() || key.exponent.empty()) return false;
  // SEQUENCE { OID 1.2.840.113549.1.1.1, NULL }
  static const uint8_t kAlgorithm[] = {0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
                                       0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00};
  std::vector<uint8_t> public_key = DerInteger(key.modulus);
  const std::vector<uint8_t> exponent = DerInteger(key.exponent);
  public_key.insert(public_key.end(), exponent.begin(), exponent.end());
  const std::vector<uint8_t> rsa_public = DerWrap(0x30, public_key);
  std::vector<uint8_t> bits(1, 0);  // "no unused bits" prefix of the BIT STRING
  bits.insert(bits.end(), rsa_public.begin(), rsa_public.end());
  const std::vector<uint8_t> algorithm(kAlgorithm, kAlgorithm + sizeof(kAlgorithm));
  std::vector<uint8_t> body = DerWrap(0x30, algorithm);
  const std::vector<uint8_t> encoded_bits = DerWrap(0x03, bits);
  body.insert(body.end(), encoded_bits.begin(), encoded_bits.end());
  *out = DerWrap(0x30, body);
  return true;
}

// Reads one tag-length-value frame at *at, bounded by size.
inline bool ReadTlv(const uint8_t* data, size_t size, size_t* at, uint8_t tag,
                    size_t* body_at, size_t* body_size) {
  if (*at + 2 > size || data[*at] != tag) return false;
  size_t length = data[*at + 1];
  size_t pos = *at + 2;
  if (length & 0x80) {
    const size_t count = length & 0x7f;
    if (!count || count > sizeof(size_t) || pos + count > size) return false;
    length = 0;
    for (size_t i = 0; i < count; ++i) length = (length << 8) | data[pos + i];
    pos += count;
  }
  if (length > size - pos) return false;
  *body_at = pos;
  *body_size = length;
  *at = pos + length;
  return true;
}

// Strips the sign padding of a DER INTEGER body into a minimal big-endian value.
inline std::vector<uint8_t> IntegerValue(const uint8_t* data, size_t size) {
  size_t skip = 0;
  while (skip + 1 < size && data[skip] == 0) ++skip;
  return std::vector<uint8_t>(data + skip, data + size);
}

inline bool ReadInteger(const uint8_t* data, size_t size, size_t* at,
                        std::vector<uint8_t>* out) {
  size_t body_at = 0, body_size = 0;
  if (!ReadTlv(data, size, at, 0x02, &body_at, &body_size) || !body_size) return false;
  if (data[body_at] & 0x80) return false;  // negative INTEGER: not a key
  *out = IntegerValue(data + body_at, body_size);
  return !out->empty();
}

inline bool DecodeSpki(const uint8_t* data, size_t size, RsaKey* out) {
  static const uint8_t kRsaEncryption[] = {0x2a, 0x86, 0x48, 0x86, 0xf7,
                                           0x0d, 0x01, 0x01, 0x01};
  size_t at = 0, sequence_at = 0, sequence_size = 0;
  if (!ReadTlv(data, size, &at, 0x30, &sequence_at, &sequence_size)) return false;
  const size_t end = sequence_at + sequence_size;
  size_t algorithm_at = 0, algorithm_size = 0;
  size_t cursor = sequence_at;
  if (!ReadTlv(data, end, &cursor, 0x30, &algorithm_at, &algorithm_size)) return false;
  size_t oid_at = 0, oid_size = 0;
  size_t oid_cursor = algorithm_at;
  if (!ReadTlv(data, algorithm_at + algorithm_size, &oid_cursor, 0x06, &oid_at,
               &oid_size))
    return false;
  if (oid_size != sizeof(kRsaEncryption)) return false;
  for (size_t i = 0; i < oid_size; ++i)
    if (data[oid_at + i] != kRsaEncryption[i]) return false;
  size_t bits_at = 0, bits_size = 0;
  if (!ReadTlv(data, end, &cursor, 0x03, &bits_at, &bits_size)) return false;
  if (bits_size < 2 || data[bits_at] != 0) return false;
  size_t key_at = bits_at + 1, key_size = 0;
  size_t inner_at = 0;
  if (!ReadTlv(data, bits_at + bits_size, &key_at, 0x30, &inner_at, &key_size))
    return false;
  size_t numbers = inner_at;
  const size_t numbers_end = inner_at + key_size;
  RsaKey key;
  if (!ReadInteger(data, numbers_end, &numbers, &key.modulus) ||
      !ReadInteger(data, numbers_end, &numbers, &key.exponent))
    return false;
  key.private_exponent.clear();
  key.prime1.clear();
  key.prime2.clear();
  *out = key;
  return true;
}

}  // namespace ruyijs::domain::crypto
#endif
