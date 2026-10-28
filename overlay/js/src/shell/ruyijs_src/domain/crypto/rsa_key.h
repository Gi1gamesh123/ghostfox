#ifndef RUYIJS_DOMAIN_CRYPTO_RSA_KEY_H
#define RUYIJS_DOMAIN_CRYPTO_RSA_KEY_H
// RSA key generation: random candidates with the top two bits set, trial
// division by small primes, Miller-Rabin, and the private exponent from
// d = e^-1 mod phi(n). No platform dependency: the caller supplies random
// bytes, so the adapter keeps using the OS generator the realm already uses.
#include <cstddef>
#include <cstdint>
#include <vector>

#include "domain/crypto/modular.h"

namespace ruyijs::domain::crypto {

// Engine-neutral random source, e.g. mozilla::GenerateRandomBytesFromOS.
using RandomBytes = bool (*)(uint8_t* out, size_t size);

// Multiples of eight only; the upper bound is what the bignum capacity can hold
// together with n = p*q and the modular products.
inline constexpr uint32_t kMinRsaBits = 512;
inline constexpr uint32_t kMaxRsaBits = 2048;
inline constexpr uint32_t kSieveLimit = 5000;
inline constexpr size_t kMillerRabinRounds = 16;

struct RsaKey {
  std::vector<uint8_t> modulus;           // n, big-endian, no sign padding
  std::vector<uint8_t> exponent;          // e
  std::vector<uint8_t> private_exponent;  // d; empty for a public key
  std::vector<uint8_t> prime1, prime2;    // p, q; private keys
  bool HasPrivate() const { return !private_exponent.empty(); }
};

inline std::vector<uint8_t> BytesOf(const BigUInt& value) {
  std::vector<uint8_t> bytes(value.ByteLength());
  value.ToBytes(bytes.data(), bytes.size());
  return bytes;
}

// Primes below kSieveLimit, used both for trial division and as the fixed
// Miller-Rabin bases.
inline const std::vector<uint32_t>& SmallPrimes() {
  static const std::vector<uint32_t> primes = [] {
    std::vector<bool> composite(kSieveLimit + 1, false);
    std::vector<uint32_t> values;
    for (uint32_t n = 2; n <= kSieveLimit; ++n) {
      if (composite[n]) continue;
      values.push_back(n);
      for (uint64_t multiple = uint64_t(n) * n; multiple <= kSieveLimit; multiple += n)
        composite[size_t(multiple)] = true;
    }
    return values;
  }();
  return primes;
}

// Strong probable prime test for one base, with n - 1 = odd_part * 2^twos.
inline bool MillerRabinRound(const BigUInt& n, const BigUInt& n_minus_one,
                             const BigUInt& odd_part, size_t twos, uint32_t base) {
  BigUInt witness, power, square;
  witness.SetUint(base);
  if (!ModPow(witness, odd_part, n, &power)) return false;
  if (power.IsOne() || power.Compare(n_minus_one) == 0) return true;
  for (size_t i = 1; i < twos; ++i) {
    if (!MulMod(power, power, n, &square)) return false;
    power = square;
    if (power.Compare(n_minus_one) == 0) return true;
    if (power.IsOne()) return false;
  }
  return false;
}

inline bool IsProbablePrime(const BigUInt& candidate, size_t rounds) {
  BigUInt one, n_minus_one, odd_part;
  one.SetUint(1);
  if (candidate.Compare(one) <= 0) return false;
  if (!candidate.IsOdd()) return false;
  if (!Sub(candidate, one, &n_minus_one)) return false;
  const size_t twos = n_minus_one.TrailingZeroBits();
  odd_part = n_minus_one;
  odd_part.ShiftRight(twos);
  const auto& bases = SmallPrimes();
  for (size_t i = 0; i < rounds && i < bases.size(); ++i) {
    BigUInt base;
    base.SetUint(bases[i]);
    if (base.Compare(candidate) >= 0) continue;
    if (!MillerRabinRound(candidate, n_minus_one, odd_part, twos, bases[i]))
      return false;
  }
  return true;
}

// Random prime with exactly *bits* bits. The two leading bits are set so the
// product of two such primes keeps 2 * bits significant bits.
inline bool GeneratePrime(size_t bits, RandomBytes random, BigUInt* out) {
  if (bits < 16) return false;
  std::vector<uint8_t> buffer((bits + 7) / 8);
  for (int attempt = 0; attempt < 100000; ++attempt) {
    if (!random(buffer.data(), buffer.size())) return false;
    buffer[0] |= 0xc0;
    buffer[buffer.size() - 1] |= 1;
    BigUInt candidate;
    if (!candidate.FromBytes(buffer.data(), buffer.size())) return false;
    bool composite = false;
    for (uint32_t prime : SmallPrimes()) {
      if (!candidate.ModSmall(prime)) { composite = true; break; }
    }
    if (composite || !IsProbablePrime(candidate, kMillerRabinRounds)) continue;
    *out = candidate;
    return true;
  }
  return false;
}

// RSA key pair with modulus (k, e) as WebCrypto's generateKey describes it.
inline bool GenerateRsaKey(uint32_t bits, const std::vector<uint8_t>& exponent,
                           RandomBytes random, RsaKey* out) {
  if (bits < kMinRsaBits || bits > kMaxRsaBits || bits % 8 != 0) return false;
  if (exponent.empty() || exponent.size() > 4) return false;
  BigUInt e;
  if (!e.FromBytes(exponent.data(), exponent.size()) || !e.IsOdd() || e.IsOne())
    return false;
  const size_t low_bits = bits / 2;
  const size_t high_bits = bits - low_bits;
  for (int attempt = 0; attempt < 32; ++attempt) {
    BigUInt p, q, n, phi, p_minus_one, q_minus_one, d, one;
    one.SetUint(1);
    if (!GeneratePrime(low_bits, random, &p) || !GeneratePrime(high_bits, random, &q))
      return false;
    if (p.Compare(q) == 0) continue;
    Mul(p, q, &n);
    if (n.BitLength() != bits || n.Compare(e) <= 0) continue;
    Sub(p, one, &p_minus_one);
    Sub(q, one, &q_minus_one);
    Mul(p_minus_one, q_minus_one, &phi);
    if (!ModInverse(e, phi, &d)) continue;  // gcd(e, phi) != 1: draw again
    out->modulus = BytesOf(n);
    out->exponent = exponent;
    out->private_exponent = BytesOf(d);
    out->prime1 = BytesOf(p);
    out->prime2 = BytesOf(q);
    return true;
  }
  return false;
}

}  // namespace ruyijs::domain::crypto
#endif
