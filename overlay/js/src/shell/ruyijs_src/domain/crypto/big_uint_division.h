#ifndef RUYIJS_DOMAIN_CRYPTO_BIG_UINT_DIVISION_H
#define RUYIJS_DOMAIN_CRYPTO_BIG_UINT_DIVISION_H
// Quotient and remainder for the RSA modular arithmetic. Kept apart from
// big_uint.h so the Knuth division stays readable on its own.
#include <cstddef>
#include <cstdint>

#include "domain/crypto/big_uint.h"

namespace ruyijs::domain::crypto {

// Knuth, TAOCP vol. 2, 4.3.1 algorithm D. Requires *remainder != b's storage and
// *quotient distinguishable from both operands.
inline bool DivMod(const BigUInt& a, const BigUInt& b, BigUInt* quotient,
                   BigUInt* remainder) {
  if (b.IsZero()) return false;
  if (a.Compare(b) < 0) {
    quotient->SetUint(0);
    *remainder = a;
    return true;
  }
  const size_t n = b.size, m = a.size;
  if (n == 1) {
    uint32_t rest = 0;
    for (size_t i = m; i-- > 0;) {
      const uint64_t value = (uint64_t(rest) << kLimbBits) | a.limb[i];
      quotient->limb[i] = uint32_t(value / b.limb[0]);
      rest = uint32_t(value % b.limb[0]);
    }
    quotient->Reset(m);
    quotient->Trim();
    remainder->SetUint(rest);
    return true;
  }
  unsigned shift = 0;
  while (!((b.limb[n - 1] << shift) & 0x80000000u)) ++shift;
  uint32_t vn[kMaxLimbs] = {}, un[kMaxLimbs + 1] = {};
  for (size_t i = 0; i < n; ++i)
    vn[i] = (b.limb[i] << shift) | (i ? ShiftedDown(b.limb[i - 1], kLimbBits - shift) : 0);
  un[m] = ShiftedDown(a.limb[m - 1], kLimbBits - shift);
  for (size_t i = 0; i < m; ++i)
    un[i] = (a.limb[i] << shift) | (i ? ShiftedDown(a.limb[i - 1], kLimbBits - shift) : 0);
  for (size_t j = m - n + 1; j-- > 0;) {
    uint64_t estimate = ((uint64_t(un[j + n]) << kLimbBits) | un[j + n - 1]) / vn[n - 1];
    uint64_t rest = ((uint64_t(un[j + n]) << kLimbBits) | un[j + n - 1]) % vn[n - 1];
    while (estimate >= kLimbBase ||
           estimate * vn[n - 2] > ((rest << kLimbBits) | un[j + n - 2])) {
      --estimate;
      rest += vn[n - 1];
      if (rest >= kLimbBase) break;
    }
    uint64_t carry = 0, borrow = 0;
    for (size_t i = 0; i < n; ++i) {
      const uint64_t product = estimate * vn[i] + carry;
      carry = product >> kLimbBits;
      const uint64_t value =
          uint64_t(un[i + j]) - (product & (kLimbBase - 1)) - borrow;
      un[i + j] = uint32_t(value);
      borrow = (value >> kLimbBits) ? 1 : 0;
    }
    const uint64_t top = uint64_t(un[j + n]) - carry - borrow;
    un[j + n] = uint32_t(top);
    quotient->limb[j] = uint32_t(estimate);
    if (top >> kLimbBits) {  // The estimate was one too large: add b back.
      quotient->limb[j] = uint32_t(--estimate);
      uint64_t back = 0;
      for (size_t i = 0; i < n; ++i) {
        const uint64_t value = uint64_t(un[i + j]) + vn[i] + back;
        un[i + j] = uint32_t(value);
        back = value >> kLimbBits;
      }
      un[j + n] = uint32_t(uint64_t(un[j + n]) + back);
    }
  }
  quotient->Reset(m - n + 1);
  quotient->Trim();
  for (size_t i = 0; i < n; ++i)
    remainder->limb[i] = (un[i] >> shift) | ShiftedIn(un[i + 1], kLimbBits - shift);
  remainder->Reset(n);
  remainder->Trim();
  return true;
}

}  // namespace ruyijs::domain::crypto
#endif
