#ifndef RUYIJS_DOMAIN_CRYPTO_MODULAR_H
#define RUYIJS_DOMAIN_CRYPTO_MODULAR_H
// Modular arithmetic over big_uint values: reduce, exponentiate and invert.
// Nothing here knows about RSA or about the WebCrypto surface.
#include "domain/crypto/big_uint.h"
#include "domain/crypto/big_uint_division.h"

namespace ruyijs::domain::crypto {

// a * b mod modulus; *out may alias either operand.
inline bool MulMod(const BigUInt& a, const BigUInt& b, const BigUInt& modulus,
                   BigUInt* out) {
  if (modulus.IsZero()) return false;
  BigUInt product, quotient, remainder;
  Mul(a, b, &product);
  if (!DivMod(product, modulus, &quotient, &remainder)) return false;
  *out = remainder;
  return true;
}

// Square-and-multiply over the exponent's bits, lowest reduction first so an
// unreduced base is accepted.
inline bool ModPow(const BigUInt& base, const BigUInt& exponent,
                   const BigUInt& modulus, BigUInt* out) {
  if (modulus.IsZero()) return false;
  BigUInt reduced, quotient, accumulator;
  if (!DivMod(base, modulus, &quotient, &reduced)) return false;
  accumulator.SetUint(1);
  for (size_t bit = exponent.BitLength(); bit-- > 0;) {
    if (!MulMod(accumulator, accumulator, modulus, &accumulator)) return false;
    if (exponent.Bit(bit) && !MulMod(accumulator, reduced, modulus, &accumulator))
      return false;
  }
  *out = accumulator;
  return true;
}

// (a - b) mod modulus for a, b < modulus.
inline void SubMod(const BigUInt& a, const BigUInt& b, const BigUInt& modulus,
                   BigUInt* out) {
  if (a.Compare(b) >= 0) {
    Sub(a, b, out);
    return;
  }
  BigUInt sum;
  Add(a, modulus, &sum);
  Sub(sum, b, out);
}

// Inverse of odd mod m by the binary extended gcd (HAC 14.61). The modulus has
// to be odd, which is what the RSA path guarantees (odd part of phi(n) and the
// odd public exponent).
inline bool ModInverseOdd(const BigUInt& odd, const BigUInt& modulus,
                          BigUInt* out) {
  if (!modulus.IsOdd()) return false;
  BigUInt u, v = modulus, x1, x2, quotient, swapped;
  if (!DivMod(odd, modulus, &quotient, &u) || u.IsZero()) return false;
  x1.SetUint(1);
  x2.SetUint(0);
  while (!u.IsOne() && !v.IsOne()) {
    if (u.IsZero() || v.IsZero()) return false;  // gcd(odd, modulus) != 1
    while (!u.IsOdd()) {
      u.ShiftRightOne();
      if (x1.IsOdd()) Add(x1, modulus, &x1);
      x1.ShiftRightOne();
    }
    while (!v.IsOdd()) {
      v.ShiftRightOne();
      if (x2.IsOdd()) Add(x2, modulus, &x2);
      x2.ShiftRightOne();
    }
    if (u.Compare(v) >= 0) {
      Sub(u, v, &u);
      SubMod(x1, x2, modulus, &swapped);
      x1 = swapped;
    } else {
      Sub(v, u, &v);
      SubMod(x2, x1, modulus, &swapped);
      x2 = swapped;
    }
  }
  *out = u.IsOne() ? x1 : x2;
  return true;
}

// Inverse of the odd value *of* modulo *modulus* of any parity. With an even
// modulus the inverse modulo the odd part is lifted one power of two at a time:
// of x and x + m exactly one is the inverse modulo 2m (both x and m are odd).
inline bool ModInverse(const BigUInt& of, const BigUInt& modulus, BigUInt* out) {
  if (!of.IsOdd()) return false;
  BigUInt odd_part = modulus, inverse, product, quotient, remainder, sum;
  const size_t twos = modulus.TrailingZeroBits();
  odd_part.ShiftRight(twos);
  if (odd_part.IsOne()) {
    inverse.SetUint(0);
  } else if (!ModInverseOdd(of, odd_part, &inverse)) {
    return false;
  }
  BigUInt lifted = odd_part;
  for (size_t step = 0; step < twos; ++step) {
    Mul(of, inverse, &product);
    const BigUInt previous = lifted;
    lifted.Double();
    if (!DivMod(product, lifted, &quotient, &remainder)) return false;
    if (!remainder.IsOne()) {
      Add(inverse, previous, &sum);
      inverse = sum;
    }
  }
  *out = inverse;
  return true;
}

}  // namespace ruyijs::domain::crypto
#endif
