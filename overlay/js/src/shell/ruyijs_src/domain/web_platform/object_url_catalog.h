#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace ruyijs::domain::web_platform {
// The catalog owns URL validity and retention accounting. References are opaque
// identities supplied by the adapter; no engine object or body bytes live here.
class ObjectUrlCatalog final {
 public:
  static constexpr size_t kEntryLimit = 1024;
  static constexpr size_t kByteLimit = 64 * 1024 * 1024;
  enum class Added { Ok, Full, Duplicate };
  struct Entry { uint64_t owner, reference, principal; std::string origin; };
  Added Add(const std::string& url, std::string origin, uint64_t owner,
            uint64_t reference, size_t bytes, uint64_t principal = 0) {
    if (entries_.find(url) != entries_.end()) return Added::Duplicate;
    const bool existing = references_.find(reference) != references_.end();
    if (entries_.size() >= kEntryLimit || (!existing && bytes > kByteLimit - bytes_))
      return Added::Full;
    auto inserted = references_.try_emplace(reference, Reference{bytes, 0});
    try { entries_.emplace(url, Entry{owner, reference, principal ? principal : owner, std::move(origin)}); }
    catch (...) { if (inserted.second) references_.erase(inserted.first); throw; }
    if (inserted.second) bytes_ += bytes;
    ++inserted.first->second.count;
    return Added::Ok;
  }
  uint64_t Read(std::string url, const std::string& origin, uint64_t owner) const {
    url.resize(url.find('#') == std::string::npos ? url.size() : url.find('#'));
    auto found = entries_.find(url);
    return found != entries_.end() && Allows(found->second, origin, owner)
        ? found->second.reference : 0;
  }
  void Revoke(const std::string& url, const std::string& origin, uint64_t owner) {
    // Reading ignores fragments; revocation requires the registered URL itself.
    auto found = entries_.find(url);
    if (found != entries_.end() && Allows(found->second, origin, owner)) Remove(found);
  }
  void ClearOwner(uint64_t owner) noexcept {
    for (auto it = entries_.begin(); it != entries_.end();) {
      if (it->second.owner == owner) { auto old = it++; Remove(old); }
      else ++it;
    }
  }
  bool Retained(uint64_t reference) const { return references_.find(reference) != references_.end(); }
  size_t size() const noexcept { return entries_.size(); }
  size_t bytes() const noexcept { return bytes_; }
 private:
  struct Reference { size_t bytes, count; };
  using Entries = std::unordered_map<std::string, Entry>;
  static bool Allows(const Entry& entry, const std::string& origin, uint64_t owner) {
    return entry.origin == origin && (origin != "null" || entry.principal == owner);
  }
  void Remove(Entries::iterator found) noexcept {
    auto reference = references_.find(found->second.reference);
    if (--reference->second.count == 0) {
      bytes_ -= reference->second.bytes;
      references_.erase(reference);
    }
    entries_.erase(found);
  }
  Entries entries_;
  std::unordered_map<uint64_t, Reference> references_;
  size_t bytes_ = 0;
};
}
