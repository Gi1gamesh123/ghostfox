#pragma once
#include "domain/document_model/table_tree.h"
#include <algorithm>

namespace ruyijs::domain::document_model {
inline DomString table_attribute(const DocumentAggregate& doc, NodeId id, DomStringView attr) {
  auto raw=doc.get_element(id)->get_attribute16(attr);
  if (attr==u"align" && !raw && (table_tag(doc,id,u"td") || table_tag(doc,id,u"th"))) {
    auto parent=doc.require(id).parent_id();
    if (table_tag(doc,parent,u"tr")) raw=doc.get_element(parent)->get_attribute16(attr);
  }
  DomString value=raw.value_or(DomString{});
  if (attr==u"scope") {
    for (auto& c:value) if (c>=u'A' && c<=u'Z') c+=u'a'-u'A';
    if (value!=u"row" && value!=u"col" && value!=u"rowgroup" && value!=u"colgroup") value.clear();
  }
  return value;
}
inline uint32_t table_span(const DocumentAggregate& doc, NodeId id, DomStringView attr) {
  const auto raw=table_attribute(doc,id,attr);
  size_t i=0;
  while (i<raw.size() && ascii_space(raw[i])) ++i;
  bool negative=i<raw.size() && raw[i]==u'-';
  if (i<raw.size() && (raw[i]==u'+' || raw[i]==u'-')) ++i;
  if (i==raw.size() || raw[i]<u'0' || raw[i]>u'9') return 1;
  uint32_t n=0;
  for (;i<raw.size() && raw[i]>=u'0' && raw[i]<=u'9';++i) {
    auto digit=uint32_t(raw[i]-u'0');
    if (n>(2147483647u-digit)/10) return negative ? 1 : attr==u"rowspan" ? 65534 : 1000;
    n=n*10+digit;
  }
  if ((negative && n) || (!n && attr!=u"rowspan")) return 1;
  return std::min(n,attr==u"rowspan" ? 65534u : 1000u);
}
}
