#ifndef RUYIJS_WEB_CRYPTO_SUBTLE_H
#define RUYIJS_WEB_CRYPTO_SUBTLE_H
#include "web_crypto/realm.h"
#include "jsapi.h"
#include "js/Utility.h"
namespace ruyijs::web_crypto {
extern const JSClass kCryptoClassDefinition, kSubtleClass, kCryptoKeyClass;
enum CryptoSlot { SubtleInstance, SubtleConstructor, KeyConstructor, SubtleGetter,
                  RandomUuidMember, CryptoConstructor, RandomReplay, CryptoSlotCount };
enum class RandomOperation { Values, Uuid };
// SubtleCrypto carries the realm's DOMException constructor (its error factory)
// and the CryptoKey constructor it creates keys with.
enum SubtleSlot { SubtleException, SubtleKeyConstructor, SubtleSlotCount };
// CryptoKey slots: the four WebIDL members, then the opaque key material the
// owning algorithm reads back.
enum CryptoKeySlot { KeyType, KeyExtractable, KeyAlgorithm, KeyUsages, KeyMaterial,
                     CryptoKeySlotCount };
bool RandomMember(JSContext*, unsigned, JS::Value*);
bool InstallGlobal(JSContext*, JS::HandleObject page, JS::HandleObject target,
                   JS::HandleObject crypto, PageHelpers&);
bool OriginalCrypto(JSContext*, JS::HandleObject page, JS::MutableHandleObject);
struct SubtleOperation { const char* name; unsigned length; };
inline constexpr SubtleOperation kSubtleOperations[] = {
  {"encrypt",3}, {"decrypt",3}, {"sign",3}, {"verify",4}, {"digest",2},
  {"generateKey",3}, {"deriveKey",5}, {"deriveBits",2}, {"importKey",5},
  {"exportKey",2}, {"wrapKey",4}, {"unwrapKey",7}};
bool InstallSubtle(JSContext*, JS::HandleObject page, PageHelpers&,
                   JS::HandleObject crypto);
bool CryptoTypeError(JSContext*, const char* message);
bool SubtleError(JSContext*, JS::HandleObject self, const char* name);
bool SubtleMethod(JSContext*, unsigned, JS::Value*);
bool SubtleGet(JSContext*, unsigned, JS::Value*);
bool KeyGet(JSContext*, unsigned, JS::Value*);
struct CryptoBytes { JS::UniqueChars data; size_t size = 0; };
bool CryptoBuffer(JSContext*, JS::HandleValue input, JS::MutableHandleObject);
bool CryptoSnapshot(JSContext*, JS::HandleObject input, CryptoBytes*);
bool Digest(JSContext*, JS::HandleObject self, JS::HandleValue algorithm,
            JS::HandleValue data, JS::MutableHandleValue result);
}
#endif
