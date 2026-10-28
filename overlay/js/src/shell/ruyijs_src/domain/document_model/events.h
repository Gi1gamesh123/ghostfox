#pragma once
#include "domain/shared/domain_event.h"
#include "domain/document_model/node_type.h"

namespace ruyijs::domain::document_model {
class NodeInsertedEvent : public shared::DomainEvent {
 public:
  NodeInsertedEvent(NodeId node, NodeId parent, std::string tag) : NodeInsertedEvent(node, parent, from_utf8(tag)) {}
  NodeInsertedEvent(NodeId node, NodeId parent, DomString tag)
      : DomainEvent("NodeInserted"), node_(node), parent_(parent), tag_(std::move(tag)) {}
  NodeId node_id() const noexcept { return node_; }
  NodeId parent_id() const noexcept { return parent_; }
  std::string tag() const { return to_utf8(tag_); }
  const DomString& tag16() const noexcept { return tag_; }
 private:
  NodeId node_, parent_;
  DomString tag_;
};
class NodeRemovedEvent : public shared::DomainEvent {
 public:
  NodeRemovedEvent(NodeId node, NodeId parent) : DomainEvent("NodeRemoved"), node_(node), parent_(parent) {}
  NodeId node_id() const noexcept { return node_; }
  NodeId parent_id() const noexcept { return parent_; }
 private:
  NodeId node_, parent_;
};
class DomContentLoadedEvent : public shared::DomainEvent {
 public:
  explicit DomContentLoadedEvent(NodeId document) : DomainEvent("DOMContentLoaded"), document_(document) {}
  NodeId document_id() const noexcept { return document_; }
 private:
  NodeId document_;
};
class DocumentLoadEvent : public shared::DomainEvent {
 public:
  explicit DocumentLoadEvent(NodeId document) : DomainEvent("DocumentLoad"), document_(document) {}
  NodeId document_id() const noexcept { return document_; }
 private:
  NodeId document_;
};
class ScriptElementEncounteredEvent : public shared::DomainEvent {
 public:
  ScriptElementEncounteredEvent(NodeId script, std::string src, bool inlined)
      : ScriptElementEncounteredEvent(script, from_utf8(src), inlined) {}
  ScriptElementEncounteredEvent(NodeId script, DomString src, bool inlined)
      : DomainEvent("ScriptElementEncountered"), script_(script), src_(std::move(src)), inline_(inlined) {}
  NodeId script_id() const noexcept { return script_; }
  std::string src() const { return to_utf8(src_); }
  const DomString& src16() const noexcept { return src_; }
  bool is_inline() const noexcept { return inline_; }
 private:
  NodeId script_;
  DomString src_;
  bool inline_;
};
}
