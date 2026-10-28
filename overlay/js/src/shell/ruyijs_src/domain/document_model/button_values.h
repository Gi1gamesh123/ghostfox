#pragma once
#include "domain/document_model/dom_string.h"

namespace ruyijs::domain::document_model {
inline DomString button_attribute(DomStringView name, DomStringView raw) {
  DomString value = ascii_lower(raw);
  if (name == u"type")
    return value == u"button" || value == u"reset" ? value : u"submit";
  if (name == u"formmethod")
    return value == u"post" || value == u"dialog" ? value : u"get";
  if (name == u"formenctype")
    return value == u"multipart/form-data" || value == u"text/plain"
        ? value : u"application/x-www-form-urlencoded";
  return DomString(raw);
}
}
