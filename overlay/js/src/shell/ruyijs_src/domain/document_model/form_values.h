#pragma once
#include "domain/document_model/dom_string.h"

namespace ruyijs::domain::document_model {
inline DomString form_attribute(DomStringView name, DomStringView raw) {
  if (name == u"accept-charset") return raw.empty() ? u"UTF-8" : DomString(raw);
  if (name == u"target") return raw.empty() ? u"_self" : DomString(raw);
  if (name == u"autocomplete") return ascii_lower(raw) == u"off" ? u"off" : u"on";
  if (name == u"method") {
    auto value = ascii_lower(raw);
    return value == u"post" || value == u"dialog" ? value : u"get";
  }
  if (name == u"enctype")
    return raw == u"multipart/form-data" || raw == u"text/plain"
        ? DomString(raw) : u"application/x-www-form-urlencoded";
  return DomString(raw);
}
}
