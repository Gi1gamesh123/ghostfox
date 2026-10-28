// SubjectPublicKeyInfo / PKCS #8 codec for the WebCrypto NIST curves
// (RFC 5480, RFC 5915, WebCrypto "EC" import/export). Product-authored domain:
// no platform or engine dependency. Coordinates and the private scalar are
// fixed-width big-endian of the curve's field size.
#ifndef RUYIJS_DOMAIN_CRYPTO_EC_DER_H
#define RUYIJS_DOMAIN_CRYPTO_EC_DER_H
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ruyijs::domain::crypto {

struct EcCurve { const char* name; size_t size; std::vector<uint8_t> oid; };
inline const EcCurve* EcCurveNamed(const std::string& name) {
  static const EcCurve curves[] = {
      {"P-256", 32, {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07}},
      {"P-384", 48, {0x2b, 0x81, 0x04, 0x00, 0x22}},
      {"P-521", 66, {0x2b, 0x81, 0x04, 0x00, 0x23}}};
  for (const auto& curve : curves) if (name == curve.name) return &curve;
  return nullptr;
}
inline const EcCurve* EcCurveOfOid(const std::vector<uint8_t>& oid) {
  for (const char* name : {"P-256", "P-384", "P-521"})
    if (EcCurveNamed(name)->oid == oid) return EcCurveNamed(name);
  return nullptr;
}

// One EC key: the public point (x, y) and, for a private key, the scalar d.
struct EcKey { const EcCurve* curve = nullptr; std::vector<uint8_t> x, y, d; };

namespace der {
inline const std::vector<uint8_t> kEcPublicKey = {0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01};
inline void Length(std::vector<uint8_t>* out, size_t n) {
  if (n < 0x80) { out->push_back(uint8_t(n)); return; }
  uint8_t bytes[4]; int count = 0;
  for (size_t v = n; v; v >>= 8) bytes[count++] = uint8_t(v);
  out->push_back(uint8_t(0x80 | count));
  while (count) out->push_back(bytes[--count]);
}
inline std::vector<uint8_t> Tlv(uint8_t tag, const std::vector<uint8_t>& body) {
  std::vector<uint8_t> out{tag};
  Length(&out, body.size());
  out.insert(out.end(), body.begin(), body.end());
  return out;
}
inline std::vector<uint8_t> Cat(std::initializer_list<std::vector<uint8_t>> parts) {
  std::vector<uint8_t> out;
  for (const auto& part : parts) out.insert(out.end(), part.begin(), part.end());
  return out;
}
// A bounded TLV reader: Next() reads one element at `at` within [begin, end).
struct Reader {
  const uint8_t* data; size_t end; size_t at;
  bool Next(uint8_t* tag, size_t* body, size_t* size) {
    if (at + 2 > end) return false;
    *tag = data[at++];
    size_t n = data[at++];
    if (n & 0x80) {
      int count = n & 0x7f; n = 0;
      if (count < 1 || count > 4 || at + count > end) return false;
      while (count--) n = (n << 8) | data[at++];
      if (n < 0x80) return false;  // DER: minimal length encoding
    }
    if (n > end - at) return false;
    *body = at; *size = n; at += n;
    return true;
  }
};
}  // namespace der

inline std::vector<uint8_t> EcPoint(const EcKey& key) {
  return der::Cat({{0x04}, key.x, key.y});
}
inline std::vector<uint8_t> EcAlgorithmIdentifier(const EcCurve& curve) {
  return der::Tlv(0x30, der::Cat({der::Tlv(0x06, der::kEcPublicKey), der::Tlv(0x06, curve.oid)}));
}
inline std::vector<uint8_t> EncodeEcSpki(const EcKey& key) {
  return der::Tlv(0x30, der::Cat({EcAlgorithmIdentifier(*key.curve),
                                  der::Tlv(0x03, der::Cat({{0x00}, EcPoint(key)}))}));
}
// PKCS #8 as Firefox exports it: ECPrivateKey version 1, the scalar and the
// [1] public key, the curve only in the outer AlgorithmIdentifier.
inline std::vector<uint8_t> EncodeEcPkcs8(const EcKey& key) {
  auto inner = der::Tlv(0x30, der::Cat({der::Tlv(0x02, {0x01}), der::Tlv(0x04, key.d),
      der::Tlv(0xa1, der::Tlv(0x03, der::Cat({{0x00}, EcPoint(key)})))}));
  return der::Tlv(0x30, der::Cat({der::Tlv(0x02, {0x00}), EcAlgorithmIdentifier(*key.curve),
                                  der::Tlv(0x04, inner)}));
}
// An uncompressed point of the curve's size.
inline bool DecodeEcPoint(const uint8_t* p, size_t n, const EcCurve& curve, EcKey* out) {
  if (n != 1 + 2 * curve.size || p[0] != 0x04) return false;
  out->curve = &curve;
  out->x.assign(p + 1, p + 1 + curve.size);
  out->y.assign(p + 1 + curve.size, p + n);
  return true;
}
namespace der {
inline bool ReadAlgorithm(const uint8_t* data, size_t body, size_t size, const EcCurve** curve) {
  Reader r{data, body + size, body};
  uint8_t tag; size_t b, s;
  if (!r.Next(&tag, &b, &s) || tag != 0x06 || std::vector<uint8_t>(data + b, data + b + s) != kEcPublicKey) return false;
  if (!r.Next(&tag, &b, &s) || tag != 0x06 || r.at != body + size) return false;
  *curve = EcCurveOfOid(std::vector<uint8_t>(data + b, data + b + s));
  return *curve != nullptr;
}
}  // namespace der
inline bool DecodeEcSpki(const uint8_t* data, size_t size, EcKey* out) {
  der::Reader top{data, size, 0};
  uint8_t tag; size_t body, length;
  if (!top.Next(&tag, &body, &length) || tag != 0x30 || top.at != size) return false;
  der::Reader seq{data, body + length, body};
  size_t b, s; const EcCurve* curve = nullptr;
  if (!seq.Next(&tag, &b, &s) || tag != 0x30 || !der::ReadAlgorithm(data, b, s, &curve)) return false;
  if (!seq.Next(&tag, &b, &s) || tag != 0x03 || s < 1 || data[b] != 0 || seq.at != body + length) return false;
  return DecodeEcPoint(data + b + 1, s - 1, *curve, out);
}
// PKCS #8 PrivateKeyInfo with an ECPrivateKey; the [1] public key is required
// here (the product has no scalar multiplication to derive it).
inline bool DecodeEcPkcs8(const uint8_t* data, size_t size, EcKey* out, bool* missingPublic) {
  *missingPublic = false;
  der::Reader top{data, size, 0};
  uint8_t tag; size_t body, length, b, s;
  if (!top.Next(&tag, &body, &length) || tag != 0x30 || top.at != size) return false;
  der::Reader seq{data, body + length, body};
  const EcCurve* curve = nullptr;
  if (!seq.Next(&tag, &b, &s) || tag != 0x02 || s != 1 || data[b] != 0) return false;
  if (!seq.Next(&tag, &b, &s) || tag != 0x30 || !der::ReadAlgorithm(data, b, s, &curve)) return false;
  if (!seq.Next(&tag, &b, &s) || tag != 0x04) return false;
  der::Reader octets{data, b + s, b};
  size_t ib, is;
  if (!octets.Next(&tag, &ib, &is) || tag != 0x30 || octets.at != b + s) return false;
  der::Reader inner{data, ib + is, ib};
  if (!inner.Next(&tag, &b, &s) || tag != 0x02 || s != 1 || data[b] != 1) return false;
  if (!inner.Next(&tag, &b, &s) || tag != 0x04 || s != curve->size) return false;
  out->d.assign(data + b, data + b + s);
  bool point = false;
  while (inner.at < ib + is) {
    if (!inner.Next(&tag, &b, &s)) return false;
    if (tag == 0xa0) {  // [0] parameters must name the same curve
      der::Reader p{data, b + s, b}; size_t ob, os;
      if (!p.Next(&tag, &ob, &os) || tag != 0x06 ||
          EcCurveOfOid(std::vector<uint8_t>(data + ob, data + ob + os)) != curve) return false;
    } else if (tag == 0xa1) {
      der::Reader p{data, b + s, b}; size_t pb, ps;
      if (!p.Next(&tag, &pb, &ps) || tag != 0x03 || ps < 1 || data[pb] != 0) return false;
      EcKey point_key;
      if (!DecodeEcPoint(data + pb + 1, ps - 1, *curve, &point_key)) return false;
      out->x = std::move(point_key.x); out->y = std::move(point_key.y); point = true;
    } else {
      return false;
    }
  }
  out->curve = curve;
  if (!point) { *missingPublic = true; return false; }
  return seq.at == body + length;
}
}  // namespace ruyijs::domain::crypto
#endif
