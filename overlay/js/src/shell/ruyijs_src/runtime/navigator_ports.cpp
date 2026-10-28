#include "runtime/navigator_ports.h"
#include "runtime/browsing_context.h"
#include "runtime/runtime.h"
#include "domain/browser_environment/url_value.h"
#include "jsapi.h"
#include "web_platform/request_body.h"

namespace ruyijs {
using UrlValue = ruyijs::domain::browser_environment::UrlValue;
bool DocumentBase(BrowsingContext& target, std::string* base, std::string* source) {
  JSContext* cx = target.cx;
  JSAutoRealm realm(cx, target.page);
  JS::RootedValue href(cx), reference(cx);
  if (!target.environment->URL(&href) || !href.isString()) return false;
  JS::RootedString text(cx, href.toString());
  if (!ToUTF8(cx, text, source)) return false;
  *base = *source;
  if (target.parent && (*source == "about:blank" || *source == "about:srcdoc")) {
    std::string inherited;
    if (!DocumentBase(*target.parent, base, &inherited)) return false;
    *source = inherited;
  }
  if (!target.document->ReadBaseReference(&reference)) return false;
  if (reference.isString()) {
    JS::RootedString raw(cx, reference.toString());
    std::string value;
    if (!ToUTF8(cx, raw, &value)) return false;
    UrlValue resolved;
    if (UrlValue::Resolve(value, *base, &resolved) &&
        resolved.protocol() != "data:" && resolved.protocol() != "javascript:")
      *base = resolved.href();
  }
  return true;
}
void BindNavigatorPorts(BrowsingContext& target, resource_network::Realm& network) {
  target.navigatorNetwork = &network;
  target.environment->SetSendBeaconPort([](void* opaque, JSContext* cx,
      const std::string& url, JS::HandleValue body, bool* accepted) {
    auto& target = *static_cast<BrowsingContext*>(opaque);
    web_platform::RequestBody snapshot;
    if (!web_platform::ReadRequestBody(cx, body, &snapshot)) return false;
    std::string base, source;
    return DocumentBase(target, &base, &source) && target.navigatorNetwork &&
        target.navigatorNetwork->Beacon(cx, base, source, url, std::move(snapshot), accepted);
  }, &target);
}
void BindWorkerScriptUrls(BrowsingContext& target) {
  target.workers->SetScriptUrlPort([](void* opaque, const std::string& input, std::string* url,
                                      bool* sameOrigin) {
    auto& target = *static_cast<BrowsingContext*>(opaque);
    std::string base, source;
    UrlValue resolved, document;
    if (!DocumentBase(target, &base, &source) || !UrlValue::Resolve(input, base, &resolved)) return false;
    *url = resolved.href();
    *sameOrigin = UrlValue::Resolve(source, source, &document) && document.origin() == resolved.origin();
    return true;
  }, &target);
}
}
