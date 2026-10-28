#ifndef RUYIJS_WEB_CREDENTIALS_REALM_H
#define RUYIJS_WEB_CREDENTIALS_REALM_H
#include <memory>
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace ruyijs { class PageHelpers; }
namespace ruyijs::web_credentials {
struct State;
// Credential Management / Web Authentication interfaces for one Window realm
// (docs/owners/CREDENTIALS.md). This slice installs the interface objects and
// PublicKeyCredential's static operations; no credential instance exists yet,
// because navigator.credentials (the only factory) is a later slice.
class Realm final {
 public:
  Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers);
  ~Realm();
  Realm(const Realm&) = delete;
  Realm& operator=(const Realm&) = delete;
  // Installs after DocumentModel: EncodingError uses this realm's DOMException.
  bool Install();
  // navigator.credentials and navigator.login over this realm's Navigator prototype.
  bool InstallNavigator(JS::HandleObject navigatorPrototype);
  // [SecureContext] exposure of every interface and member of this owner.
  bool ConfigureExposure();
 private:
  std::unique_ptr<State> state_;
};
}
#endif
