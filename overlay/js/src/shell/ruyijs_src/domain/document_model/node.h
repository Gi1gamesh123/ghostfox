#pragma once
#include "domain/shared/entity.h"
#include "domain/document_model/node_type.h"
#include <algorithm>
#include <optional>
#include <vector>

namespace ruyijs::domain::document_model {
class Node : public shared::Entity<NodeId> {
 public:
  Node(NodeId id, NodeType type) : Entity<NodeId>(id), node_type_(type) {}
  ~Node() override = default;
  NodeType node_type() const noexcept { return node_type_; }
  virtual NodeId shadow_host_id() const noexcept { return 0; }
  virtual NodeId shadow_root_id() const noexcept { return 0; }
  virtual size_t storage_units() const noexcept { return 0; }
  virtual DomString node_name16() const {
    switch (node_type_) {
      case NodeType::Text: return u"#text"; case NodeType::Comment: return u"#comment";
      case NodeType::Document: return u"#document"; case NodeType::DocumentFragment: return u"#document-fragment";
      default: return {};
    }
  }
  virtual std::optional<DomString> node_value16() const { return std::nullopt; }
  virtual void set_node_value16(DomString) {}
  virtual DomString text_content16() const { return node_value16().value_or(DomString{}); }
  virtual std::string node_name() const { return to_utf8(node_name16()); }
  virtual std::string node_value() const { return to_utf8(node_value16().value_or(DomString{})); }
  virtual void set_node_value(const std::string& value) { set_node_value16(from_utf8(value)); }
  virtual std::string text_content() const { return to_utf8(text_content16()); }
  NodeId parent_id() const noexcept { return parent_id_; }
  bool has_parent() const noexcept { return parent_id_ != 0; }
  void set_parent(NodeId id) noexcept { parent_id_ = id; }
  const std::vector<NodeId>& child_ids() const noexcept { return children_; }
  size_t child_count() const noexcept { return children_.size(); }
  bool has_children() const noexcept { return !children_.empty(); }
  NodeId first_child_id() const noexcept { return children_.empty() ? 0 : children_.front(); }
  NodeId last_child_id() const noexcept { return children_.empty() ? 0 : children_.back(); }
  // Internal owner operations. Production callers mutate through DocumentAggregate.
  void append_child_id(NodeId id) { children_.push_back(id); }
  void insert_child_id_at(NodeId id, size_t index) {
    children_.insert(children_.begin() + std::min(index, children_.size()), id);
  }
  bool remove_child_id(NodeId id) {
    auto it = std::find(children_.begin(), children_.end(), id);
    if (it == children_.end()) return false;
    children_.erase(it); return true;
  }
  void clear_children() noexcept { children_.clear(); }
  void replace_children(std::vector<NodeId>& value) noexcept { children_.swap(value); }
  int index_of_child(NodeId id) const {
    auto it = std::find(children_.begin(), children_.end(), id);
    return it == children_.end() ? -1 : int(it - children_.begin());
  }
 protected:
  Node(const Node&) = default;
  Node& operator=(const Node&) = default;
  NodeType node_type_;
  NodeId parent_id_ = 0;
  std::vector<NodeId> children_;
};
}
