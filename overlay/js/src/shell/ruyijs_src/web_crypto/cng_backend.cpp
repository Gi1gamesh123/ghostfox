#include "web_crypto/cng_backend.h"
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <cstring>
#include <limits>
namespace ruyijs::web_crypto {
namespace {
namespace ec = ruyijs::domain::crypto;
struct Provider {
  BCRYPT_ALG_HANDLE handle = nullptr;
  explicit Provider(const wchar_t* name) { if (BCryptOpenAlgorithmProvider(&handle, name, nullptr, 0) < 0) handle = nullptr; }
  ~Provider() { if (handle) BCryptCloseAlgorithmProvider(handle, 0); }
};
struct Key {
  BCRYPT_KEY_HANDLE handle = nullptr;
  ~Key() { if (handle) BCryptDestroyKey(handle); }
};
bool Fits(size_t n) { return n <= std::numeric_limits<ULONG>::max(); }
const wchar_t* EcAlgorithm(const ec::EcCurve& curve, bool ecdh) {
  if (curve.size == 32) return ecdh ? BCRYPT_ECDH_P256_ALGORITHM : BCRYPT_ECDSA_P256_ALGORITHM;
  if (curve.size == 48) return ecdh ? BCRYPT_ECDH_P384_ALGORITHM : BCRYPT_ECDSA_P384_ALGORITHM;
  return ecdh ? BCRYPT_ECDH_P521_ALGORITHM : BCRYPT_ECDSA_P521_ALGORITHM;
}
ULONG EcMagic(const ec::EcCurve& curve, bool ecdh, bool priv) {
  if (curve.size == 32) return ecdh ? (priv ? BCRYPT_ECDH_PRIVATE_P256_MAGIC : BCRYPT_ECDH_PUBLIC_P256_MAGIC)
                                    : (priv ? BCRYPT_ECDSA_PRIVATE_P256_MAGIC : BCRYPT_ECDSA_PUBLIC_P256_MAGIC);
  if (curve.size == 48) return ecdh ? (priv ? BCRYPT_ECDH_PRIVATE_P384_MAGIC : BCRYPT_ECDH_PUBLIC_P384_MAGIC)
                                    : (priv ? BCRYPT_ECDSA_PRIVATE_P384_MAGIC : BCRYPT_ECDSA_PUBLIC_P384_MAGIC);
  return ecdh ? (priv ? BCRYPT_ECDH_PRIVATE_P521_MAGIC : BCRYPT_ECDH_PUBLIC_P521_MAGIC)
              : (priv ? BCRYPT_ECDSA_PRIVATE_P521_MAGIC : BCRYPT_ECDSA_PUBLIC_P521_MAGIC);
}
// BCRYPT_ECCKEY_BLOB: magic, field size, then X, Y and (private) d.
bool ImportEc(const Provider& provider, const ec::EcKey& key, bool ecdh, bool priv, Key* out) {
  const size_t n = key.curve->size;
  if (key.x.size() != n || key.y.size() != n || (priv && key.d.size() != n)) return false;
  Bytes blob(sizeof(BCRYPT_ECCKEY_BLOB));
  auto* header = reinterpret_cast<BCRYPT_ECCKEY_BLOB*>(blob.data());
  header->dwMagic = EcMagic(*key.curve, ecdh, priv);
  header->cbKey = ULONG(n);
  blob.insert(blob.end(), key.x.begin(), key.x.end());
  blob.insert(blob.end(), key.y.begin(), key.y.end());
  if (priv) blob.insert(blob.end(), key.d.begin(), key.d.end());
  return provider.handle && BCryptImportKeyPair(provider.handle, nullptr,
      priv ? BCRYPT_ECCPRIVATE_BLOB : BCRYPT_ECCPUBLIC_BLOB, &out->handle, blob.data(), ULONG(blob.size()), 0) >= 0;
}
bool Symmetric(const wchar_t* mode, const Bytes& key, Provider* provider, Key* out) {
  if (!provider->handle || (key.size() != 16 && key.size() != 24 && key.size() != 32)) return false;
  if (BCryptSetProperty(provider->handle, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(mode)),
                        ULONG((wcslen(mode) + 1) * sizeof(wchar_t)), 0) < 0) return false;
  return BCryptGenerateSymmetricKey(provider->handle, &out->handle, nullptr, 0,
                                    const_cast<PUCHAR>(key.data()), ULONG(key.size()), 0) >= 0;
}
}  // namespace

bool AesCbc(const Bytes& key, const Bytes& iv, const Bytes& input, bool encrypt, Bytes* out) {
  Provider provider(BCRYPT_AES_ALGORITHM); Key handle;
  if (iv.size() != 16 || !Fits(input.size() + 16) || !Symmetric(BCRYPT_CHAIN_MODE_CBC, key, &provider, &handle)) return false;
  ULONG size = 0;
  uint8_t empty = 0;  // CNG rejects a null input even for zero bytes
  PUCHAR source = input.empty() ? &empty : const_cast<PUCHAR>(input.data());
  auto run = [&](PUCHAR target, ULONG capacity) {
    Bytes local = iv;
    return (encrypt ? BCryptEncrypt : BCryptDecrypt)(handle.handle, source, ULONG(input.size()),
        nullptr, local.data(), ULONG(local.size()), target, capacity, &size, BCRYPT_BLOCK_PADDING) >= 0;
  };
  if (!run(nullptr, 0)) return false;
  out->assign(size, 0);
  if (!run(out->data(), ULONG(out->size()))) return false;
  out->resize(size);
  return true;
}

bool AesGcm(const Bytes& key, const Bytes& iv, const Bytes& aad, size_t tagBytes,
            const Bytes& input, bool encrypt, Bytes* out) {
  Provider provider(BCRYPT_AES_ALGORITHM); Key handle;
  // CNG's GCM takes a 96-bit nonce only.
  if (iv.size() != 12 || tagBytes < 4 || tagBytes > 16 || !Fits(input.size()) || !Fits(aad.size()) ||
      !Symmetric(BCRYPT_CHAIN_MODE_GCM, key, &provider, &handle)) return false;
  if (!encrypt && input.size() < tagBytes) return false;
  const size_t body = encrypt ? input.size() : input.size() - tagBytes;
  Bytes tag = encrypt ? Bytes(tagBytes, 0) : Bytes(input.begin() + body, input.end());
  BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
  BCRYPT_INIT_AUTH_MODE_INFO(info);
  info.pbNonce = const_cast<PUCHAR>(iv.data()); info.cbNonce = ULONG(iv.size());
  info.pbAuthData = aad.empty() ? nullptr : const_cast<PUCHAR>(aad.data()); info.cbAuthData = ULONG(aad.size());
  info.pbTag = tag.data(); info.cbTag = ULONG(tag.size());
  Bytes result(body ? body : 1);
  ULONG size = 0;
  NTSTATUS status = (encrypt ? BCryptEncrypt : BCryptDecrypt)(handle.handle, body ? const_cast<PUCHAR>(input.data()) : nullptr,
      ULONG(body), &info, nullptr, 0, result.data(), ULONG(body), &size, 0);
  if (status < 0) return false;
  result.resize(size);
  if (encrypt) result.insert(result.end(), tag.begin(), tag.end());
  *out = std::move(result);
  return true;
}

bool Hmac(HashAlgorithm hash, const Bytes& key, const Bytes& data, Bytes* out) {
  static const wchar_t* const names[] = {BCRYPT_SHA1_ALGORITHM, BCRYPT_SHA256_ALGORITHM, BCRYPT_SHA384_ALGORITHM, BCRYPT_SHA512_ALGORITHM};
  static const size_t sizes[] = {20, 32, 48, 64};
  BCRYPT_ALG_HANDLE provider = nullptr;
  if (!Fits(key.size()) || !Fits(data.size()) ||
      BCryptOpenAlgorithmProvider(&provider, names[unsigned(hash)], nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) < 0) return false;
  uint8_t empty = 0;
  out->assign(sizes[unsigned(hash)], 0);
  NTSTATUS status = BCryptHash(provider, key.empty() ? &empty : const_cast<PUCHAR>(key.data()), ULONG(key.size()),
                               data.empty() ? &empty : const_cast<PUCHAR>(data.data()), ULONG(data.size()),
                               out->data(), ULONG(out->size()));
  BCryptCloseAlgorithmProvider(provider, 0);
  return status >= 0;
}

bool EcGenerate(const ec::EcCurve& curve, bool ecdh, ec::EcKey* out) {
  Provider provider(EcAlgorithm(curve, ecdh)); Key key;
  const ULONG bits = curve.size == 66 ? 521 : ULONG(curve.size * 8);
  if (!provider.handle || BCryptGenerateKeyPair(provider.handle, &key.handle, bits, 0) < 0 ||
      BCryptFinalizeKeyPair(key.handle, 0) < 0) return false;
  ULONG size = 0;
  if (BCryptExportKey(key.handle, nullptr, BCRYPT_ECCPRIVATE_BLOB, nullptr, 0, &size, 0) < 0) return false;
  Bytes blob(size);
  if (BCryptExportKey(key.handle, nullptr, BCRYPT_ECCPRIVATE_BLOB, blob.data(), size, &size, 0) < 0) return false;
  const size_t n = curve.size, at = sizeof(BCRYPT_ECCKEY_BLOB);
  if (size < at + 3 * n) return false;
  out->curve = &curve;
  out->x.assign(blob.begin() + at, blob.begin() + at + n);
  out->y.assign(blob.begin() + at + n, blob.begin() + at + 2 * n);
  out->d.assign(blob.begin() + at + 2 * n, blob.begin() + at + 3 * n);
  SecureZeroMemory(blob.data(), blob.size());
  return true;
}

bool EcValidate(const ec::EcKey& key, bool ecdh) {
  Provider provider(EcAlgorithm(*key.curve, ecdh)); Key handle;
  return ImportEc(provider, key, ecdh, !key.d.empty(), &handle);
}

bool EcdsaSign(const ec::EcKey& key, HashAlgorithm hash, const Bytes& message, Bytes* out) {
  Provider provider(EcAlgorithm(*key.curve, false)); Key handle;
  DigestBytes digest;
  if (!ComputeDigest(hash, message.data(), message.size(), &digest) || !ImportEc(provider, key, false, true, &handle)) return false;
  ULONG size = 0;
  if (BCryptSignHash(handle.handle, nullptr, digest.data, ULONG(digest.size), nullptr, 0, &size, 0) < 0) return false;
  out->assign(size, 0);
  return BCryptSignHash(handle.handle, nullptr, digest.data, ULONG(digest.size), out->data(), size, &size, 0) >= 0;
}

bool EcdsaVerify(const ec::EcKey& key, HashAlgorithm hash, const Bytes& message,
                 const Bytes& signature, bool* valid) {
  Provider provider(EcAlgorithm(*key.curve, false)); Key handle;
  DigestBytes digest;
  ec::EcKey publicKey{key.curve, key.x, key.y, {}};
  if (!ComputeDigest(hash, message.data(), message.size(), &digest) || !ImportEc(provider, publicKey, false, false, &handle)) return false;
  *valid = signature.size() == 2 * key.curve->size &&
      BCryptVerifySignature(handle.handle, nullptr, digest.data, ULONG(digest.size),
                            const_cast<PUCHAR>(signature.data()), ULONG(signature.size()), 0) >= 0;
  return true;
}

bool EcdhSecret(const ec::EcKey& privateKey, const ec::EcKey& publicKey, Bytes* out) {
  Provider provider(EcAlgorithm(*privateKey.curve, true)); Key mine, theirs;
  ec::EcKey peer{publicKey.curve, publicKey.x, publicKey.y, {}};
  if (privateKey.curve != publicKey.curve || !ImportEc(provider, privateKey, true, true, &mine) ||
      !ImportEc(provider, peer, true, false, &theirs)) return false;
  BCRYPT_SECRET_HANDLE secret = nullptr;
  if (BCryptSecretAgreement(mine.handle, theirs.handle, &secret, 0) < 0) return false;
  ULONG size = 0;
  bool ok = BCryptDeriveKey(secret, BCRYPT_KDF_RAW_SECRET, nullptr, nullptr, 0, &size, 0) >= 0;
  if (ok) {
    out->assign(size, 0);
    ok = BCryptDeriveKey(secret, BCRYPT_KDF_RAW_SECRET, nullptr, out->data(), size, &size, 0) >= 0;
  }
  BCryptDestroySecret(secret);
  // KDF_RAW_SECRET is little-endian; WebCrypto's secret is the big-endian x.
  if (ok) std::reverse(out->begin(), out->end());
  return ok;
}
}  // namespace ruyijs::web_crypto
