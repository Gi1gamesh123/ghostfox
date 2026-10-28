#pragma once
#include "domain/document_model/element.h"
#include <algorithm>

namespace ruyijs::domain::document_model {
inline void validate_attribute_token(DomStringView name, DomStringView token) {
  if (token.empty()) throw DomError(DomErrorCode::Syntax, "Token must not be empty");
  for (auto unit : token)
    if (ascii_space(unit)) throw DomError(name == u"sandbox" ? DomErrorCode::InvalidCharacter
                                                           : DomErrorCode::Syntax,
                                         "Token must not contain ASCII whitespace");
}
inline std::vector<DomString> attribute_tokens(const Element& element, DomStringView name) {
  auto values = tokens(element.get_attribute16(name).value_or(DomString{}));
  if (name != u"sandbox" && name != u"part") return values;
  std::vector<DomString> unique;
  for (const auto& token : values)
    if (std::find(unique.begin(), unique.end(), token) == unique.end()) unique.push_back(token);
  return unique;
}
enum class TokenAction { Contains, Add, Remove, Toggle, Replace };
struct TokenEdit { bool changed = false, result = false, remove = false; DomString value; int invalid = 0; };
inline TokenEdit edit_token_values(std::vector<DomString>& all, DomStringView name, TokenAction action,
    DomStringView token, DomStringView replacement, bool force, bool has_force) {
  auto found = std::find(all.begin(), all.end(), DomString(token));
  const bool present = found != all.end();
  TokenEdit edit;
  if (action == TokenAction::Contains) { edit.result = present; return edit; }
  if (action == TokenAction::Add) {
    if (present) return edit;
    all.emplace_back(token);
  } else if (action == TokenAction::Remove) {
    if (!present) return edit;
    all.erase(std::remove(all.begin(), all.end(), DomString(token)), all.end());
  } else if (action == TokenAction::Toggle) {
    edit.result = has_force ? force : !present;
    if (edit.result == present) return edit;
    if (edit.result) all.emplace_back(token);
    else all.erase(std::remove(all.begin(), all.end(), DomString(token)), all.end());
  } else {
    edit.result = present;
    if (!present) return edit;
    if (name == u"part" && replacement == token) return edit;
    if (replacement.empty() && name != u"part") all.erase(found);
    else *found = DomString(replacement);
    if (name == u"sandbox" || name == u"part") {
      std::vector<DomString> unique;
      for (const auto& value : all)
        if (std::find(unique.begin(), unique.end(), value) == unique.end()) unique.push_back(value);
      all.swap(unique);
    }
  }
  edit.changed = true;
  edit.remove = name == u"sandbox" && all.empty();
  for (size_t i = 0; i < all.size(); ++i) {
    if (i && (name == u"part" || !edit.value.empty())) edit.value.push_back(u' ');
    edit.value += all[i];
  }
  return edit;
}
inline TokenEdit edit_attribute_tokens(const Element& element, DomStringView name, TokenAction action,
    DomStringView token, DomStringView replacement, bool force, bool has_force) {
  validate_attribute_token(name, token);
  if (action == TokenAction::Replace && (name == u"sandbox" || !replacement.empty()))
    validate_attribute_token(name, replacement);
  auto all = attribute_tokens(element, name);
  return edit_token_values(all, name, action, token, replacement, force, has_force);
}
// Public part operations convert all arguments before a single attribute edit.
// Validation records the last invalid argument but does not prevent the edit.
inline TokenEdit edit_part_tokens(const Element& element, TokenAction action,
    const std::vector<DomString>& arguments, bool force, bool has_force) {
  auto all = attribute_tokens(element, u"part");
  TokenEdit result;
  for (const auto& token : arguments) {
    if (token.empty()) result.invalid = 12;
    else if (std::any_of(token.begin(), token.end(), ascii_space)) result.invalid = 5;
  }
  const size_t count = action == TokenAction::Add || action == TokenAction::Remove ? arguments.size() : 1;
  for (size_t i = 0; i < count; ++i) {
    const auto edit = edit_token_values(all, u"part", action, arguments[i],
        action == TokenAction::Replace ? DomStringView(arguments[1]) : DomStringView(), force, has_force);
    result.result = edit.result;
    if (edit.changed) { result.changed = true; result.value = edit.value; result.remove = edit.value.empty(); }
  }
  return result;
}
}
