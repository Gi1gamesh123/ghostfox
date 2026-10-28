#ifndef RUYIJS_DOMAIN_RTC_DTLS_CERTIFICATE_H
#define RUYIJS_DOMAIN_RTC_DTLS_CERTIFICATE_H
// Value rules for the DTLS identity a peer connection announces, as recorded
// from Firefox 158: an RTCCertificate lives for 30 days from generation and
// reports one SHA-256 fingerprint. The key generation algorithm is a recorded
// acceptance table: webrtc-pc rejects with NotSupportedError anything a user
// agent cannot or will not use to generate a certificate.
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace ruyijs::domain::rtc {

// A SHA-256 digest, the only fingerprint algorithm Firefox reports.
inline constexpr size_t kDtlsFingerprintBytes = 32;
inline constexpr char kCertificateFingerprintAlgorithm[] = "sha-256";
inline constexpr uint64_t kCertificateLifetimeMs =
    30ull * 24ull * 60ull * 60ull * 1000ull;

// "AB:CD:..." - uppercase hex, colon separated, one byte per group, the form
// used by the SDP a=fingerprint line.
inline std::string FormatFingerprint(const uint8_t* bytes, size_t size) {
  static const char kDigits[] = "0123456789ABCDEF";
  std::string out;
  out.reserve(size * 3);
  for (size_t i = 0; i < size; ++i) {
    if (i) out.push_back(':');
    out.push_back(kDigits[bytes[i] >> 4]);
    out.push_back(kDigits[bytes[i] & 0x0f]);
  }
  return out;
}

// Firefox's RTCCertificate.getFingerprints() exposes the same digest in
// lower-case form.
inline std::string FormatCertificateFingerprint(const uint8_t* bytes, size_t size) {
  static const char kDigits[] = "0123456789abcdef";
  std::string out;
  out.reserve(size * 3);
  for (size_t i = 0; i < size; ++i) {
    if (i) out.push_back(':');
    out.push_back(kDigits[bytes[i] >> 4]);
    out.push_back(kDigits[bytes[i] & 0x0f]);
  }
  return out;
}

// Algorithm names arrive normalized to upper case. Firefox accepts the three
// NIST ECDSA curves; RSASSA-PKCS1-v1_5 requires the fields needed to generate
// an RSA key, which the WebCrypto algorithm normalization also validates.
inline bool SupportsCertificateAlgorithm(std::string_view name,
                                         std::string_view named_curve,
                                         bool has_rsa_parameters) {
  if (name == "ECDSA")
    return named_curve == "P-256" || named_curve == "P-384" ||
           named_curve == "P-521";
  if (name == "RSASSA-PKCS1-V1_5") return has_rsa_parameters;
  return false;
}

}  // namespace ruyijs::domain::rtc
#endif
