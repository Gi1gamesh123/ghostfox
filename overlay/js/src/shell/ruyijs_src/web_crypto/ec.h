// ECDSA and ECDH (P-256, P-384, P-521) for this realm's SubtleCrypto, and the
// AES-CBC/AES-GCM secret keys. Key material lives in the one CryptoKey's
// material slot (crypto_key.h); CNG does the arithmetic (cng_backend.h).
#ifndef RUYIJS_WEB_CRYPTO_EC_H
#define RUYIJS_WEB_CRYPTO_EC_H
#include <string>
#include <vector>
#include "domain/crypto/ec_der.h"
#include "web_crypto/rsa.h"
namespace ruyijs::web_crypto {
SubtleStatus EcOperation(JSContext* cx, JS::HandleObject subtle, unsigned operation,
                         const JS::CallArgs& args, JS::MutableHandleValue result);
SubtleStatus HmacOperation(JSContext* cx, JS::HandleObject subtle, unsigned operation,
                           const JS::CallArgs& args, JS::MutableHandleValue result);
SubtleStatus AesOperation(JSContext* cx, JS::HandleObject subtle, unsigned operation,
                          const JS::CallArgs& args, JS::MutableHandleValue result);
// ec_key.cpp: a CryptoKey for `key` (private when key.d is set) and its reader.
JSObject* NewEcKey(JSContext* cx, JS::HandleObject subtle, bool ecdh, bool extractable,
                   const domain::crypto::EcKey& key, JS::HandleObject usages);
bool ReadEcKey(JSContext* cx, JS::HandleObject key, domain::crypto::EcKey* out);
bool EcImport(JSContext* cx, JS::HandleObject subtle, const JS::CallArgs& args, bool ecdh,
              JS::MutableHandleValue result);
bool EcExport(JSContext* cx, JS::HandleObject subtle, const std::string& format, JS::HandleObject key,
              JS::MutableHandleValue result);
// The key's usages as a JWK key_ops array (shared by the EC and AES exports).
JSObject* KeyOps(JSContext* cx, JS::HandleObject key);
// aes.cpp: a secret AES key from raw bytes (importKey "raw" and ECDH deriveKey).
bool AesKeyFromRaw(JSContext* cx, JS::HandleObject subtle, const std::string& name,
                   const std::vector<uint8_t>& raw, bool extractable, JS::HandleValue usages,
                   JS::MutableHandleValue result);
}
#endif
