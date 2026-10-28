#pragma once
#include "domain/document_model/node.h"
#include <map>
#include <memory>

namespace ruyijs::domain::document_model {
// Legacy UTF-8 parser DTO boundary, retained as a port, not a persistent DOM owner.
class HtmlParserPort {
 public:
  virtual ~HtmlParserPort() = default;
  struct ParseAction {
    enum class Type { OpenElement, CloseElement, Text, Comment, Script, Style, Doctype };
    Type type;
    TagName tag_name;
    std::map<std::string, std::string> attributes;
    std::string text_data;
    bool is_void = false;
  };
  virtual std::vector<ParseAction> parse(const std::string& html) = 0;
  virtual void parse_chunk(const std::string& chunk) = 0;
  virtual bool has_pending_actions() const = 0;
  virtual ParseAction next_action() = 0;
  virtual void reset() = 0;
};
class NodeRepository {
 public:
  virtual ~NodeRepository() = default;
  virtual void save(NodeId id, std::unique_ptr<Node> node) = 0;
  virtual Node* find(NodeId id) = 0;
  virtual void remove(NodeId id) = 0;
  virtual size_t count() const = 0;
  virtual void clear() = 0;
};
}
