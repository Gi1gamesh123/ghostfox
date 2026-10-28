#ifndef RUYIJS_WEB_CRYPTO_DIGEST_BACKEND_H
#define RUYIJS_WEB_CRYPTO_DIGEST_BACKEND_H
#include <cstddef>
#include <cstdint>
namespace ruyijs::web_crypto {
enum class HashAlgorithm { Sha1, Sha256, Sha384, Sha512 };
struct DigestBytes { uint8_t data[64]{}; size_t size = 0; };
bool ComputeDigest(HashAlgorithm algorithm, const void* input, size_t length,
                    DigestBytes* output) noexcept;
}
#endif
