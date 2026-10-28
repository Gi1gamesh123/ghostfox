#pragma once
#include "domain/document_model/dom_string.h"

namespace ruyijs::domain::document_model {
// The public tool's detached namespace factory consumes the namespace but
// retains only the spelling after the first colon in a NUL-terminated name.
inline DomString public_attribute_name(DomStringView qualified) {
  const auto nul = qualified.find(u'\0');
  if (nul != DomStringView::npos) qualified = qualified.substr(0, nul);
  const auto colon = qualified.find(u':');
  if (colon != DomStringView::npos) qualified = qualified.substr(colon + 1);
  return DomString(qualified);
}
}
