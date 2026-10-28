#ifndef RUYIJS_FONT_LOADING_REALM_H
#define RUYIJS_FONT_LOADING_REALM_H
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
namespace ruyijs { class PageHelpers; }
namespace ruyijs::interaction { class Realm; }
namespace ruyijs::font_loading {
// FontFace and FontFaceSet keep every observable in reserved slots: the working
// set of faces, their statuses, the load promises and the seven CSS descriptors.
// The installer creates both interfaces in the target realm, exposes
// document.fonts, and attaches the set to the shared event tree so
// loading/loadingdone/loadingerror dispatch on the one interaction path.
// |isDocument| is the Document brand (Document Model's), checked by the
// document.fonts getter; null in a Worker realm, whose holder is the global.
using ReceiverBrand = bool (*)(JS::HandleValue);
bool Install(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
             interaction::Realm& events, JS::HandleObject document, ReceiverBrand isDocument);
bool InstallWorker(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
                   interaction::Realm& events, JS::HandleObject scopePrototype);
}
#endif
