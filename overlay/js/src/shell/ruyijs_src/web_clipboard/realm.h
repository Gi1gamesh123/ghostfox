#ifndef RUYIJS_WEB_CLIPBOARD_REALM_H
#define RUYIJS_WEB_CLIPBOARD_REALM_H
#include <memory>
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace ruyijs { class PageHelpers; }
namespace ruyijs::web_clipboard {
struct State;
// ClipboardItem for one Window realm (docs/owners/CLIPBOARD.md). Every item,
// its entries and their data promises live in this realm; nothing is shared
// with the parent or a child realm.
class Realm final {
 public:
  Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers);
  ~Realm();
  Realm(const Realm&) = delete;
  Realm& operator=(const Realm&) = delete;
  // Installs after WebPlatform (Blob) and DocumentModel (DOMException): it
  // keeps this realm's own constructors for the values getType() settles with.
  bool Install();
  // Web Platform's brand check: is a resolved ClipboardItemData value a Blob.
  using BlobBrandPort = bool (*)(JS::HandleValue value);
  void SetBlobBrandPort(BlobBrandPort port);
  // Clipboard extends EventTarget: Interaction attaches the one instance.
  using AttachPort = bool (*)(void* context, JS::HandleObject target);
  // Installs Clipboard and navigator.clipboard over this realm's EventTarget
  // and Navigator prototypes; call after Install.
  bool InstallNavigatorClipboard(JS::HandleObject eventTarget, JS::HandleObject navigatorPrototype,
                                 AttachPort attach, void* context);
  // [SecureContext] navigator.clipboard, Clipboard and ClipboardItem.
  bool ConfigureExposure();
 private:
  std::unique_ptr<State> state_;
};
}
#endif
