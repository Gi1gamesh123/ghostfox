#include "document_model/model_hyperlink.h"
#include "domain/browser_environment/url_value.h"
#include <new>

namespace ruyijs::document_model {
using ruyijs::domain::browser_environment::UrlValue;
Status ReadHyperlink(const std::string& href, const std::string& base,
                     HyperlinkField field, std::string* result) noexcept {
  try {
    UrlValue url;
    if (!UrlValue::Resolve(href, base, &url)) {
      *result = field == HyperlinkField::Href ? href :
          field == HyperlinkField::Protocol ? ":" : "";
      return {};
    }
    switch (field) {
      case HyperlinkField::Href: *result = url.href(); break;
      case HyperlinkField::Origin: *result = url.origin(); break;
      case HyperlinkField::Protocol: *result = url.protocol(); break;
      case HyperlinkField::Host: *result = url.host(); break;
      case HyperlinkField::Hostname: *result = url.hostname(); break;
      case HyperlinkField::Port: *result = url.port(); break;
      case HyperlinkField::Pathname: *result = url.pathname(); break;
      case HyperlinkField::Search: *result = url.search(); break;
      case HyperlinkField::Hash: *result = url.hash(); break;
      case HyperlinkField::Username: *result = url.username(); break;
      case HyperlinkField::Password: *result = url.password(); break;
    }
    return {};
  } catch (const std::bad_alloc&) {
    return {ErrorCode::OutOfMemory, "Hyperlink allocation failed"};
  } catch (...) {
    return {ErrorCode::InvalidState, "Hyperlink conversion failed"};
  }
}
Status WriteHyperlink(const std::string& href, const std::string& base,
                      HyperlinkField field, const std::string& value,
                      std::string* result, bool* changed) noexcept {
  *changed = false;
  try {
    UrlValue url;
    if (!UrlValue::Resolve(href, base, &url)) return {};
    switch (field) {
      case HyperlinkField::Protocol: url.set_protocol(value); break;
      case HyperlinkField::Host: url.set_host(value); break;
      case HyperlinkField::Hostname: url.set_hostname(value); break;
      case HyperlinkField::Port: url.set_port(value); break;
      case HyperlinkField::Pathname: url.set_pathname(value); break;
      case HyperlinkField::Search: url.set_search(value); break;
      case HyperlinkField::Hash: url.set_hash(value); break;
      case HyperlinkField::Username: url.set_username(value); break;
      case HyperlinkField::Password: url.set_password(value); break;
      default: return {};
    }
    *result = url.href();
    *changed = true;
    return {};
  } catch (const std::bad_alloc&) {
    return {ErrorCode::OutOfMemory, "Hyperlink allocation failed"};
  } catch (...) {
    return {ErrorCode::InvalidState, "Hyperlink conversion failed"};
  }
}
}
