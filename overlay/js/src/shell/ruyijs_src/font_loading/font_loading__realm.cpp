#include "font_loading/realm.h"

#include "font_loading/internal.h"
#include "runtime/page_helpers.h"
#include "js/Realm.h"

namespace ruyijs::font_loading {

// The constructor, prototype, document.fonts set and ready promise belong to the
// installing realm, so an iframe gets realm-local objects instead of the
// parent's.
bool Install(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
             interaction::Realm& events, JS::HandleObject document, ReceiverBrand isDocument) {
  JSAutoRealm realm(cx, page);
  JS::RootedObject constructor(cx, InstallFontFace(cx, page, helpers));
  if (!constructor) return false;
  JS::RootedObject none(cx);
  return InstallFontFaceSet(cx, page, document, helpers, events, constructor, none, isDocument);
}
// Worker realms: FontFace, FontFaceSet and WorkerGlobalScope.fonts.
bool InstallWorker(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
                   interaction::Realm& events, JS::HandleObject scopePrototype) {
  JSAutoRealm realm(cx, page);
  JS::RootedObject constructor(cx, InstallFontFace(cx, page, helpers)), none(cx);
  if (!constructor) return false;
  return InstallFontFaceSet(cx, page, none, helpers, events, constructor, scopePrototype, nullptr);
}

}  // namespace ruyijs::font_loading
