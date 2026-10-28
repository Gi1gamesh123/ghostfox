#ifndef RUYIJS_WEB_CRYPTO_REALM_H
#define RUYIJS_WEB_CRYPTO_REALM_H
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace ruyijs { class PageHelpers; }
namespace ruyijs::web_crypto {
// Random bytes and digests come from OS primitives; realm-local JS slots hold
// the stable SubtleCrypto instance and each CryptoKey's own material. RSA-OAEP
// with SHA-256 is the only key algorithm implemented; the rest stay pending.
bool Install(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
             JS::HandleObject globalPrototype = nullptr);
// Before page execution, reapply secure-context exposure after profile changes.
bool ConfigureExposure(JSContext* cx, JS::HandleObject page);
}
#endif
