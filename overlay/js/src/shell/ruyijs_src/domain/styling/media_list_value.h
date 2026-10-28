// CSSOM media-list value. No engine, DOM, screen or transport dependency.
// Basic query serialization; full CSS escapes/functions remain owner debt.
#pragma once
#include <algorithm>
#include "domain/styling/css_components.h"
#include "domain/styling/css_condition_value.h"
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

namespace ruyijs::domain::styling {
class MediaListValue {
 public:
  const std::vector<std::string>& entries() const { return queries_; }
  std::string text() const {
    std::string result;
    for (const auto& q : queries_) { if (!result.empty()) result += ", "; result += q; }
    return result;
  }
  void set(const std::string& text) { queries_ = Parse(text); }
  void append(const std::string& text) {
    bool valid = false;auto query = First(text, &valid);
    if (!valid) return;
    queries_.erase(std::remove(queries_.begin(), queries_.end(), query), queries_.end());
    queries_.push_back(query);
  }
  bool remove(const std::string& text) {
    bool valid = false;auto query = First(text, &valid);
    if (!valid) return false;
    auto size = queries_.size();
    queries_.erase(std::remove(queries_.begin(), queries_.end(), query), queries_.end());
    return queries_.size() != size;
  }
 private:
  static std::string First(const std::string& text,bool* valid) {
    auto parts=CssSplit(text,',');
    return CssNormalizeMediaQuery(parts.front(),valid);
  }
  static std::vector<std::string> Parse(const std::string& text) {
    std::vector<std::string> out;
    if(CssTrim(text).empty()) return out;
    for(const auto& part:CssSplit(text,',')) out.push_back(CssNormalizeMediaQuery(part));
    return out;
  }
  std::vector<std::string> queries_;
};
}  // namespace ruyijs::domain::styling
