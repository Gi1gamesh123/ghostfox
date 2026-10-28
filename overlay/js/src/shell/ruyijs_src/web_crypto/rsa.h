#ifndef RUYIJS_WEB_CRYPTO_RSA_H
#define RUYIJS_WEB_CRYPTO_RSA_H
#include <cstdint>
#include <string>
#include <vector>
#include "domain/crypto/rsa_key.h"
#include "domain/crypto/rsa_oaep.h"
#include "web_crypto/subtle.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace ruyijs::web_crypto {
// How a SubtleCrypto operation this realm implements reports itself: it set the
// result, it declined the algorithm (the caller keeps rejecting with
// NotSupportedError), or it failed with a pending exception.
enum class SubtleStatus { kHandled, kUnsupported, kFailed };

// RSA-OAEP (hash SHA-1, SHA-256, SHA-384 or SHA-512) is the key algorithm with
// a native implementation; every other name is declined. The key material a CryptoKey carries lives in
// its own reserved slot, so no second key interface or state table exists.
SubtleStatus RsaOperation(JSContext* cx, JS::HandleObject subtle, unsigned operation,
                          const JS::CallArgs& args, JS::MutableHandleValue result);
// The digest for a canonical WebCrypto hash name, wrapping the digest backend the
// realm already uses so the domain OAEP unit never depends on a platform API.
// An empty descriptor (size 0) for any other name.
domain::crypto::OaepHash OaepHashFor(const std::string& name);
// The realm's CryptoKey constructor and prototype live on the Crypto object, so
// this owner creates no second key interface.
// |hash| is the canonical hash name recorded in CryptoKey.algorithm.hash.
JSObject* NewRsaKey(JSContext* cx, JS::HandleObject subtle, const char* type,
                    bool extractable, const domain::crypto::RsaKey& key,
                    JS::HandleObject usages, const std::string& hash);
// The canonical hash name a CryptoKey records in algorithm.hash.name.
bool RsaKeyHash(JSContext* cx, JS::HandleObject key, std::string* hash);
bool ReadRsaKey(JSContext* cx, JS::HandleObject key, domain::crypto::RsaKey* out);
// Dictionary readers shared by the five operations below.
bool CryptoAlgorithmName(JSContext* cx, JS::HandleValue algorithm, std::string* name);
bool ReadRsaOaepName(JSContext* cx, JS::HandleValue algorithm, bool* supported);
// The canonical hash of an RsaHashedImportParams / RsaHashedKeyGenParams
// dictionary; empty when the algorithm is not RSA-OAEP or the hash is not one
// of SHA-1/256/384/512.
bool ReadRsaOaepHash(JSContext* cx, JS::HandleValue algorithm, std::string* hash);
bool RsaBytes(JSContext* cx, JS::HandleValue value, std::vector<uint8_t>* out);
// The OS random generator the realm already uses, shaped for the domain units,
// and a fresh ArrayBuffer over a byte vector in the current realm.
bool RsaRandom(uint8_t* out, size_t size);
JSObject* RsaBuffer(JSContext* cx, const std::vector<uint8_t>& bytes);
bool RsaUsage(JSContext* cx, JS::HandleObject usages, const char* name, bool* present);
bool RsaKeyType(JSContext* cx, JS::HandleObject key, std::string* type, bool* extractable);
bool RsaEncryptOperation(JSContext* cx, JS::HandleObject subtle, const JS::CallArgs& args,
                         JS::MutableHandleValue result);
bool RsaDecryptOperation(JSContext* cx, JS::HandleObject subtle, const JS::CallArgs& args,
                         JS::MutableHandleValue result);
bool RsaGenerateOperation(JSContext* cx, JS::HandleObject subtle, const JS::CallArgs& args,
                          JS::MutableHandleValue result);
bool RsaImportOperation(JSContext* cx, JS::HandleObject subtle, const JS::CallArgs& args,
                        JS::MutableHandleValue result);
bool RsaExportOperation(JSContext* cx, JS::HandleObject subtle, const JS::CallArgs& args,
                        JS::MutableHandleValue result);
}  // namespace ruyijs::web_crypto
#endif
