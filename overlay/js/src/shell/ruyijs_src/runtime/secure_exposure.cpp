#include "runtime/secure_exposure.h"
#include "runtime/browsing_context.h"
#include "web_crypto/realm.h"
#include "web_geolocation/realm.h"
#include "web_geometry/system_font.h"
#include "domain/styling/css_font_value.h"

namespace ruyijs {
bool ConfigureSecureExposure(BrowsingContext& context) {
  // The browser locale picks the language group of unlabelled text and the
  // initial font-family, as Firefox's intl.accept_languages does.
  std::string language;
  if (!context.environment->ReadLanguage(&language)) return false;
  web_geometry::SetBrowserLanguage(language);
  domain::styling::CssSetInitialFontFamily(web_geometry::DefaultGenericFamily());
  return web_crypto::ConfigureExposure(context.cx, context.page) &&
         web_geolocation::ConfigureExposure(context.cx, context.page) &&
         (!context.locks || context.locks->ConfigureExposure()) &&
         (!context.media || context.media->ConfigureExposure()) &&
         (!context.gpu || context.gpu->ConfigureExposure()) &&
         (!context.wakeLock || context.wakeLock->ConfigureExposure()) &&
         (!context.midi || context.midi->ConfigureExposure()) &&
         (!context.serviceWorker || context.serviceWorker->ConfigureExposure()) &&
         (!context.clipboard || context.clipboard->ConfigureExposure()) &&
         (!context.credentials || context.credentials->ConfigureExposure()) &&
         // window.cookieStore and CookieStore; the per-context port names this
         // context's resource network realm for the main page and for a frame.
         (!context.navigatorNetwork || context.navigatorNetwork->ConfigureExposure()) &&
         context.interaction->ConfigureExposure() &&
         context.document->ConfigureExposure() &&
         context.environment->ConfigureSecureMembers() &&
         context.environment->OrderNavigator();
}
}  // namespace ruyijs
