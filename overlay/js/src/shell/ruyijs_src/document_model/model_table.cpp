#include "document_model/model_internal.h"
#include "domain/document_model/table_plan.h"
#include "domain/document_model/table_reflection.h"

namespace ruyijs::document_model {
Status Model::TableNodes(NodeKey scope, TableList kind, std::vector<NodeKey>* out) const noexcept {
  return Protect([&] {
    std::vector<NodeKey> values;
    for (auto id:domain::table_nodes(State().owner(scope),scope.node,domain::TableList(kind)))
      values.push_back(Key(scope.document,id));
    out->swap(values);
  });
}
Status Model::TableIndex(NodeKey node, unsigned kind, int32_t* out) const noexcept {
  return Protect([&] {*out=domain::table_index(State().owner(node),node.node,kind);});
}
Status Model::TableInsertPlan(NodeKey scope, int32_t index, TableInsertion* out) const noexcept {
  return Protect([&] {
    auto plan=domain::table_insert_plan(State().owner(scope),scope.node,index);
    *out={Key(scope.document,plan.parent),Key(scope.document,plan.before),plan.body};
  });
}
Status Model::TableDeleteTarget(NodeKey scope, int32_t index, NodeKey* out) const noexcept {
  return Protect([&] {
    *out=Key(scope.document,domain::table_delete_target(State().owner(scope),scope.node,index));
  });
}
Status Model::TablePart(NodeKey scope, DomStringView tag, NodeKey* part, NodeKey* before) const noexcept {
  return Protect([&] {
    const auto& doc=State().owner(scope);
    if (part) *part=Key(scope.document,domain::table_part(doc,scope.node,tag));
    if (before) *before=Key(scope.document,domain::table_part_before(doc,scope.node,tag));
  });
}
Status Model::TableAttribute(NodeKey node, DomStringView attr, DomString* out) const noexcept {
  return Protect([&] {*out=domain::table_attribute(State().owner(node),node.node,attr);});
}
Status Model::TableSpan(NodeKey node, DomStringView attr, uint32_t* out) const noexcept {
  return Protect([&] {*out=domain::table_span(State().owner(node),node.node,attr);});
}
}
