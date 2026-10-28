#pragma once
#include "domain/document_model/node.h"

namespace ruyijs::domain::document_model {
class CharacterData : public Node {
 public:
  CharacterData(NodeId id, NodeType kind, DomString data) : Node(id, kind), data_(std::move(data)) {}
  std::optional<DomString> node_value16() const override { return data_; }
  void set_node_value16(DomString value) override { data_.swap(value); }
  DomString text_content16() const override { return data_; }
  const DomString& data16() const noexcept { return data_; }
  std::string data() const { return to_utf8(data_); }
  void set_data(std::string value) { set_node_value16(from_utf8(value)); }
  size_t length() const noexcept { return data_.size(); }
  size_t storage_units() const noexcept override { return data_.size(); }
  DomString substring(uint32_t offset, uint32_t count) const {
    if (offset > data_.size()) throw DomError(DomErrorCode::IndexSize, "CharacterData offset exceeds length");
    return data_.substr(offset, count);
  }
  void replace(uint32_t offset, uint32_t count, DomStringView value) {
    if (offset > data_.size()) throw DomError(DomErrorCode::IndexSize, "CharacterData offset exceeds length");
    DomString next(data_); next.replace(offset, count, value); data_.swap(next);
  }
 private:
  DomString data_;
};
class TextNode : public CharacterData {
 public:
  TextNode(NodeId id, std::string data) : TextNode(id, from_utf8(data)) {}
  TextNode(NodeId id, DomString data) : CharacterData(id, NodeType::Text, std::move(data)) {}
};
class CommentNode : public CharacterData {
 public:
  CommentNode(NodeId id, std::string data) : CommentNode(id, from_utf8(data)) {}
  CommentNode(NodeId id, DomString data) : CharacterData(id, NodeType::Comment, std::move(data)) {}
};
class ProcessingInstructionNode : public CharacterData {
 public:
  ProcessingInstructionNode(NodeId id, DomString target, DomString data)
      : CharacterData(id, NodeType::ProcessingInstruction, std::move(data)), target_(std::move(target)) {}
  DomString node_name16() const override { return target_; }
  const DomString& target() const noexcept { return target_; }
  size_t storage_units() const noexcept override { return target_.size() + data16().size(); }
 private:
  DomString target_;
};
}
