#ifndef RUYIJS_DOMAIN_CRYPTO_BIG_UINT_H
#define RUYIJS_DOMAIN_CRYPTO_BIG_UINT_H
// Fixed-capacity unsigned integers for the WebCrypto RSA key algorithms. No
// engine or platform dependency: limbs are 32-bit so every intermediate fits
// in uint64_t, and the capacity covers a 2048-bit modulus with its products.
// Division lives in big_uint_division.h.
#include <cstddef>
#include <cstdint>

namespace ruyijs::domain::crypto {

inline constexpr size_t kLimbBits = 32;
inline constexpr size_t kMaxLimbs = 128;  // 4096 bits
inline constexpr uint64_t kLimbBase = 1ull << kLimbBits;

// Shifts by 0..31 bits; an amount of kLimbBits or more contributes nothing, so
// the normalization paths above need no separate whole-limb case.
constexpr uint32_t ShiftedIn(uint32_t value, unsigned amount) {
  return amount >= kLimbBits ? 0 : value << amount;
}
constexpr uint32_t ShiftedDown(uint32_t value, unsigned amount) {
  return amount >= kLimbBits ? 0 : value >> amount;
}

struct BigUInt {
  uint32_t limb[kMaxLimbs] = {};
  size_t size = 0;  // significant limbs; zero when the value is zero

  // Keeps [0, count) and clears the tail, so trimmed values stay canonical.
  void Reset(size_t count) {
    for (size_t i = count; i < kMaxLimbs; ++i) limb[i] = 0;
    size = count;
  }
  void Trim() {
    size_t count = size;
    while (count && !limb[count - 1]) --count;
    Reset(count);
  }
  bool IsZero() const { return size == 0; }
  bool IsOne() const { return size == 1 && limb[0] == 1; }
  bool IsOdd() const { return size != 0 && (limb[0] & 1u) != 0; }
  bool Bit(size_t index) const {
    const size_t at = index / kLimbBits;
    return at < size && ((limb[at] >> (index % kLimbBits)) & 1u) != 0;
  }
  size_t BitLength() const {
    if (!size) return 0;
    uint32_t top = limb[size - 1];
    size_t bits = 0;
    while (top) { ++bits; top >>= 1; }
    return (size - 1) * kLimbBits + bits;
  }
  size_t ByteLength() const { return (BitLength() + 7) / 8; }
  void SetUint(uint64_t value) {
    limb[0] = uint32_t(value);
    limb[1] = uint32_t(value >> kLimbBits);
    Reset(limb[1] ? 2 : (limb[0] ? 1 : 0));
  }
  int Compare(const BigUInt& other) const {
    if (size != other.size) return size < other.size ? -1 : 1;
    for (size_t i = size; i-- > 0;)
      if (limb[i] != other.limb[i]) return limb[i] < other.limb[i] ? -1 : 1;
    return 0;
  }
  void Double() {
    uint64_t carry = 0;
    for (size_t i = 0; i < size; ++i) {
      const uint64_t value = (uint64_t(limb[i]) << 1) | carry;
      limb[i] = uint32_t(value);
      carry = value >> kLimbBits;
    }
    if (carry) limb[size++] = uint32_t(carry);
  }
  size_t TrailingZeroBits() const {
    size_t bits = 0;
    for (size_t i = 0; i < size; ++i) {
      if (!limb[i]) { bits += kLimbBits; continue; }
      uint32_t value = limb[i];
      while (!(value & 1u)) { value >>= 1; ++bits; }
      return bits;
    }
    return bits;
  }
  void ShiftRight(size_t amount) {
    const size_t drop = amount / kLimbBits;
    const unsigned part = unsigned(amount % kLimbBits);
    if (drop >= size) { SetUint(0); return; }
    const size_t count = size - drop;
    for (size_t i = 0; i < count; ++i) {
      const uint32_t upper = i + drop + 1 < size ? limb[i + drop + 1] : 0;
      limb[i] = ShiftedDown(limb[i + drop], part) | ShiftedIn(upper, kLimbBits - part);
    }
    Reset(count);
    Trim();
  }
  void ShiftRightOne() {
    for (size_t i = 0; i < size; ++i) {
      const uint32_t carry = i + 1 < size ? limb[i + 1] << (kLimbBits - 1) : 0;
      limb[i] = (limb[i] >> 1) | carry;
    }
    Trim();
  }
  bool FromBytes(const uint8_t* bytes, size_t length) {
    if ((length + 3) / 4 > kMaxLimbs) return false;
    Reset((length + 3) / 4);
    for (size_t i = 0; i < length; ++i) {
      const size_t from = length - 1 - i;
      limb[i / 4] |= uint32_t(bytes[from]) << ((i % 4) * 8);
    }
    Trim();
    return true;
  }
  void ToBytes(uint8_t* out, size_t length) const {
    for (size_t i = 0; i < length; ++i) {
      const size_t from = length - 1 - i;
      out[from] = uint8_t(limb[i / 4] >> ((i % 4) * 8));
    }
  }
  // Value modulo one limb, without copying the value.
  uint32_t ModSmall(uint32_t divisor) const {
    uint64_t rest = 0;
    for (size_t i = size; i-- > 0;) rest = ((rest << kLimbBits) | limb[i]) % divisor;
    return uint32_t(rest);
  }
  // In-place division by one limb; *remainder receives the remainder.
  bool DivSmall(uint32_t divisor, uint32_t* remainder) {
    if (!divisor) return false;
    uint64_t rest = 0;
    for (size_t i = size; i-- > 0;) {
      const uint64_t value = (rest << kLimbBits) | limb[i];
      limb[i] = uint32_t(value / divisor);
      rest = value % divisor;
    }
    Trim();
    *remainder = uint32_t(rest);
    return true;
  }
};

inline void Add(const BigUInt& a, const BigUInt& b, BigUInt* out) {
  uint64_t carry = 0;
  const size_t count = a.size > b.size ? a.size : b.size;
  for (size_t i = 0; i < count; ++i) {
    const uint64_t value = carry + (i < a.size ? a.limb[i] : 0) +
                           uint64_t(i < b.size ? b.limb[i] : 0);
    out->limb[i] = uint32_t(value);
    carry = value >> kLimbBits;
  }
  out->limb[count] = uint32_t(carry);
  out->Reset(count + size_t(carry != 0));
}

// Requires a >= b.
inline bool Sub(const BigUInt& a, const BigUInt& b, BigUInt* out) {
  if (a.Compare(b) < 0) return false;
  uint64_t borrow = 0;
  for (size_t i = 0; i < a.size; ++i) {
    const uint64_t value = uint64_t(a.limb[i]) - (i < b.size ? b.limb[i] : 0) - borrow;
    out->limb[i] = uint32_t(value);
    borrow = (value >> kLimbBits) ? 1 : 0;
  }
  out->Reset(a.size);
  out->Trim();
  return true;
}

// Schoolbook multiplication; *out must not alias either operand.
inline void Mul(const BigUInt& a, const BigUInt& b, BigUInt* out) {
  if (a.IsZero() || b.IsZero()) { out->SetUint(0); return; }
  uint32_t product[kMaxLimbs] = {};
  for (size_t i = 0; i < a.size; ++i) {
    uint64_t carry = 0;
    for (size_t j = 0; j < b.size; ++j) {
      const uint64_t value = uint64_t(a.limb[i]) * b.limb[j] + product[i + j] + carry;
      product[i + j] = uint32_t(value);
      carry = value >> kLimbBits;
    }
    product[i + b.size] = uint32_t(carry);
  }
  for (size_t i = 0; i < a.size + b.size; ++i) out->limb[i] = product[i];
  out->Reset(a.size + b.size);
  out->Trim();
}

}  // namespace ruyijs::domain::crypto
#endif
