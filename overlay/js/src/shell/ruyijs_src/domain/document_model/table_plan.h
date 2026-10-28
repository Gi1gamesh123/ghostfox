#pragma once
#include "domain/document_model/table_tree.h"

namespace ruyijs::domain::document_model {
struct TableInsertion { NodeId parent=0, before=0; bool body=false; };
inline TableInsertion table_insert_plan(const DocumentAggregate& doc, NodeId id, int32_t index) {
  const bool cell = table_tag(doc,id,u"tr");
  const auto nodes = table_nodes(doc,id,cell ? TableList::Cells : TableList::Rows);
  if (index < -1 || (index >= 0 && size_t(index)>nodes.size()))
    throw DomError(DomErrorCode::IndexSize,"Table insertion index is out of range");
  TableInsertion out{id};
  const bool append = index == -1 || size_t(index) == nodes.size();
  if (!table_tag(doc,id,u"table")) {
    out.before = append ? 0 : nodes[size_t(index)];
  } else if (!nodes.empty()) {
    NodeId ref = append ? nodes.back() : nodes[size_t(index)];
    out.parent = doc.require(ref).parent_id();
    out.before = append ? 0 : ref;
  } else {
    for (auto child : doc.require(id).child_ids())
      if (table_tag(doc,child,u"tbody")) out.parent=child;
    out.body = out.parent==id;
  }
  return out;
}
inline NodeId table_delete_target(const DocumentAggregate& doc, NodeId id, int32_t index) {
  const auto nodes = table_nodes(doc,id,table_tag(doc,id,u"tr") ? TableList::Cells : TableList::Rows);
  if (index == -1) return nodes.empty() ? 0 : nodes.back();
  if (index < 0 || size_t(index)>=nodes.size())
    throw DomError(DomErrorCode::IndexSize,"Table deletion index is out of range");
  return nodes[size_t(index)];
}
inline NodeId table_part_before(const DocumentAggregate& doc, NodeId id, DomStringView tag) {
  const auto& parent = doc.require(id);
  if (tag == u"caption") return parent.first_child_id();
  if (tag == u"thead") {
    for (auto child : parent.child_ids()) {
      const auto* e=doc.get_element(child);
      if (e && e->tag_name().html() && !table_tag(doc,child,u"caption") &&
          !table_tag(doc,child,u"colgroup")) return child;
    }
  } else if (tag == u"tbody") {
    NodeId before=0;
    const auto& children=parent.child_ids();
    for (size_t i=0;i<children.size();++i)
      if (table_tag(doc,children[i],u"tbody")) before=i+1<children.size()?children[i+1]:0;
    return before;
  }
  return 0;
}
}
