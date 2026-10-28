#pragma once
#include "domain/document_model/document.h"

namespace ruyijs::domain::document_model {
// Transient views of the unique document tree. Never retains an index or node.
inline bool table_tag(const DocumentAggregate& doc, NodeId id, DomStringView tag) {
  const auto* e = doc.get_element(id);
  return e && e->tag_name().html() && e->tag_name().local16() == tag;
}
inline bool table_section(const DocumentAggregate& doc, NodeId id) {
  return table_tag(doc,id,u"thead") || table_tag(doc,id,u"tbody") || table_tag(doc,id,u"tfoot");
}
enum class TableList { Rows, Bodies, Cells };
inline std::vector<NodeId> table_nodes(const DocumentAggregate& doc, NodeId id, TableList kind) {
  std::vector<NodeId> out;
  const auto& children = doc.require(id).child_ids();
  auto rows = [&](NodeId section) {
    for (auto child : doc.require(section).child_ids())
      if (table_tag(doc,child,u"tr")) out.push_back(child);
  };
  if (kind == TableList::Rows && table_tag(doc,id,u"table")) {
    for (auto child : children) if (table_tag(doc,child,u"thead")) rows(child);
    for (auto child : children) {
      if (table_tag(doc,child,u"tr")) out.push_back(child);
      else if (table_tag(doc,child,u"tbody")) rows(child);
    }
    for (auto child : children) if (table_tag(doc,child,u"tfoot")) rows(child);
  } else for (auto child : children) {
    bool match = kind == TableList::Rows ? table_tag(doc,child,u"tr") :
        kind == TableList::Bodies ? table_tag(doc,child,u"tbody") :
        table_tag(doc,child,u"td") || table_tag(doc,child,u"th");
    if (match) out.push_back(child);
  }
  return out;
}
inline int32_t table_index(const DocumentAggregate& doc, NodeId id, unsigned kind) {
  NodeId parent = doc.require(id).parent_id();
  if (!parent) return -1;
  TableList list = TableList::Rows;
  if (kind == 2) {
    if (!table_tag(doc,parent,u"tr")) return -1;
    list = TableList::Cells;
  } else if (kind == 1) {
    if (!table_section(doc,parent)) return -1;
  } else if (!table_tag(doc,parent,u"table")) {
    if (!table_section(doc,parent)) return -1;
    parent = doc.require(parent).parent_id();
    if (!table_tag(doc,parent,u"table")) return -1;
  }
  const auto nodes = table_nodes(doc,parent,list);
  for (size_t i=0;i<nodes.size();++i) if (nodes[i]==id) return int32_t(i);
  return -1;
}
inline NodeId table_part(const DocumentAggregate& doc, NodeId id, DomStringView tag) {
  for (auto child : doc.require(id).child_ids()) if (table_tag(doc,child,tag)) return child;
  return 0;
}
}
