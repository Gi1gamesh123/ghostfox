#pragma once
#include <cstddef>
#include <string>
#include <unordered_map>

namespace ruyijs::domain::interaction {
// One drag payload. The public tool keeps text formats separate from its
// empty item list; this owner deliberately has no file or clipboard model.
class DataTransferData {
 public:
  static constexpr size_t kStringLimit = 1024 * 1024;
  static constexpr size_t kByteLimit = 8 * 1024 * 1024;
  static constexpr size_t kFormatLimit = 4096;
  using Formats = std::unordered_map<std::string, std::u16string>;
  static void Normalize(std::string& format) noexcept {
    for (char& c : format) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
  }
  bool Set(std::string format, const std::u16string& text) {
    Normalize(format);
    const auto found = formats_.find(format);
    const size_t old = found == formats_.end() ? 0 : found->first.size() + found->second.size() * 2;
    const size_t added = format.size() + text.size() * 2;
    if (format.size() > kStringLimit || text.size() * 2 > kStringLimit ||
        bytes_ - old + added > kByteLimit ||
        (found == formats_.end() && formats_.size() >= kFormatLimit)) return false;
    // Copy before publishing so allocation failure preserves the old value.
    std::u16string replacement(text);
    if (found == formats_.end()) formats_.emplace(std::move(format), std::move(replacement));
    else found->second.swap(replacement);
    bytes_ = bytes_ - old + added;
    return true;
  }
  const std::u16string* Get(std::string format) const {
    Normalize(format);
    const auto found = formats_.find(format);
    return found == formats_.end() ? nullptr : &found->second;
  }
  void Clear(std::string format) {
    if (format.empty()) { formats_.clear(); bytes_ = 0; return; }
    Normalize(format);
    const auto found = formats_.find(format);
    if (found != formats_.end()) {
      bytes_ -= found->first.size() + found->second.size() * 2;
      formats_.erase(found);
    }
  }
  const Formats& formats() const noexcept { return formats_; }
  const char* Drop() const noexcept { return kDrop[drop_]; }
  const char* Effect() const noexcept { return kEffects[effect_]; }
  void SetDrop(const std::string& value) noexcept {
    for (unsigned i = 0; i < 4; ++i) if (value == kDrop[i]) { drop_ = i; return; }
  }
  void SetEffect(const std::string& value) noexcept {
    for (unsigned i = 0; i < 9; ++i) if (value == kEffects[i]) { effect_ = i; return; }
  }
 private:
  inline static constexpr const char* kDrop[] = {"none", "copy", "link", "move"};
  inline static constexpr const char* kEffects[] = {
      "none", "copy", "link", "move", "copyLink", "copyMove", "linkMove", "all", "uninitialized"};
  Formats formats_;
  size_t bytes_ = 0;
  unsigned drop_ = 0, effect_ = 8;
};
}  // namespace ruyijs::domain::interaction
