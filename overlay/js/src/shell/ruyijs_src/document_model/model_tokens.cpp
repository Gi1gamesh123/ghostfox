#include "document_model/model_internal.h"
#include "domain/document_model/attribute_tokens.h"

namespace ruyijs::document_model {
static domain::TokenAction Operation(TokenAction action) {
  switch (action) {
    case TokenAction::Contains: return domain::TokenAction::Contains;
    case TokenAction::Add: return domain::TokenAction::Add;
    case TokenAction::Remove: return domain::TokenAction::Remove;
    case TokenAction::Toggle: return domain::TokenAction::Toggle;
    case TokenAction::Replace: return domain::TokenAction::Replace;
  }
  return domain::TokenAction::Contains;
}
Status Model::AttributeTokens(NodeKey key, DomStringView name, std::vector<DomString>* result) const noexcept {
  return Protect([&] { *result = domain::attribute_tokens(State().element(key), name); });
}
Status Model::EditTokens(NodeKey key, DomStringView name, TokenAction action, DomStringView token,
    DomStringView replacement, bool force, bool has_force, bool* result) noexcept {
  domain::TokenEdit edit;
  const auto status = Protect([&] {
    edit = domain::edit_attribute_tokens(State().element(key), name, Operation(action), token, replacement, force, has_force);
  });
  if (status.code != ErrorCode::None) return status;
  *result = edit.result;
  if (!edit.changed) return {};
  return edit.remove ? RemoveAttribute(key, name) : SetAttribute(key, name, edit.value);
}
Status Model::EditPartTokens(NodeKey key, TokenAction action, const std::vector<DomString>& tokens,
    bool force, bool has_force, bool* result) noexcept {
  domain::TokenEdit edit;
  auto status = Protect([&] { edit = domain::edit_part_tokens(State().element(key), Operation(action), tokens, force, has_force); });
  if (status.code != ErrorCode::None) return status;
  *result = edit.result;
  if (edit.changed) {
    status = edit.remove ? RemoveAttribute(key, u"part") : SetAttribute(key, u"part", edit.value);
    if (status.code != ErrorCode::None) return status;
  }
  if (edit.invalid == 12) return {ErrorCode::Syntax, "Token must not be empty"};
  if (edit.invalid == 5) return {ErrorCode::InvalidCharacter, "Token must not contain ASCII whitespace"};
  return {};
}
}
