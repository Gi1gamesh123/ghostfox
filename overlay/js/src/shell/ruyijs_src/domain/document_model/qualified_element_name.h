#pragma once
#include "domain/document_model/node_type.h"

namespace ruyijs::domain::document_model {
inline TagName qualified_element_name(DomStringView namespace_uri, DomStringView name, bool html_document) {
  const auto colon = name.find(u':');
  const auto prefix = colon == DomStringView::npos ? DomStringView() : name.substr(0, colon);
  const auto local = colon == DomStringView::npos ? name : name.substr(colon + 1);
  bool valid_prefix = colon == DomStringView::npos || !prefix.empty();
  for (const auto c : prefix)
    if (!c || ascii_space(c) || c == u'/' || c == u'>') valid_prefix = false;
  if (!valid_prefix || !valid_element_local_name(local))
    throw DomError(DomErrorCode::InvalidCharacter, "Invalid qualified element name");
  constexpr DomStringView xml = u"http://www.w3.org/XML/1998/namespace";
  constexpr DomStringView xmlns = u"http://www.w3.org/2000/xmlns/";
  bool valid_namespace;
  if (prefix.empty()) valid_namespace = (local == u"xmlns") == (namespace_uri == xmlns);
  else if (namespace_uri.empty()) valid_namespace = false;
  else if (namespace_uri == xmlns) valid_namespace = prefix == u"xmlns" && local != u"xmlns";
  else valid_namespace = prefix != u"xmlns" && (namespace_uri == xml || prefix != u"xml");
  if (!valid_namespace) throw DomError(DomErrorCode::Namespace, "Invalid element namespace");
  return TagName(DomString(local), namespace_uri, prefix, html_document);
}
}
