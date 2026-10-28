#pragma once
#include "domain/resource_network/cache_store.h"
#include <map>

namespace ruyijs::domain::resource_network {
class CacheNamespace final {
 public:
  explicit CacheNamespace(CacheLimits limits) : budget_(std::make_shared<CacheBudget>()), limits_(limits) {}
  ~CacheNamespace() noexcept {
    // Handles may retain stores after the namespace list itself is released.
    budget_->bytes -= nameBytes_;
  }
  CacheNamespace(const CacheNamespace&) = delete;
  CacheNamespace& operator=(const CacheNamespace&) = delete;
  std::shared_ptr<CacheStore> find(std::u16string_view name) const noexcept {
    for (const auto& entry : names_) if (entry.first == name) return entry.second;
    return {};
  }
  std::shared_ptr<CacheStore> open(std::u16string_view name) {
    validateName(name);
    if (auto store = find(name)) return store;
    const size_t bytes = name.size() * sizeof(char16_t);
    CacheRequire(budget_->stores < limits_.stores && budget_->bytes <= limits_.retainedBytes &&
        bytes <= limits_.retainedBytes - budget_->bytes, CacheError::Limit);
    auto next = names_;
    auto store = std::shared_ptr<CacheStore>(new CacheStore(budget_, limits_));
    next.emplace_back(std::u16string(name), store);
    // Nothing fallible follows. An uncommitted store destructor changes no counters.
    ++budget_->stores;
    budget_->bytes += bytes;
    nameBytes_ += bytes;
    store->accounted_ = true;
    names_.swap(next);
    return store;
  }
  bool erase(std::u16string_view name) {
    validateName(name);
    auto found = std::find_if(names_.begin(), names_.end(),
        [&](const auto& entry) { return entry.first == name; });
    if (found == names_.end()) return false;
    const size_t bytes = found->first.size() * sizeof(char16_t);
    auto next = names_;
    next.erase(next.begin() + (found - names_.begin()));
    budget_->bytes -= bytes;
    nameBytes_ -= bytes;
    names_.swap(next);
    return true;
  }
  std::vector<std::u16string> names() const {
    std::vector<std::u16string> result;
    result.reserve(names_.size());
    for (const auto& entry : names_) result.push_back(entry.first);
    return result;
  }
  const auto& stores() const noexcept { return names_; }
  const CacheBudget& budget() const noexcept { return *budget_; }
  const CacheLimits& limits() const noexcept { return limits_; }
  void validateName(std::u16string_view name) const {
    CacheRequire(name.size() <= limits_.keyBytes / sizeof(char16_t), CacheError::Limit);
  }
 private:
  std::shared_ptr<CacheBudget> budget_;
  CacheLimits limits_;
  size_t nameBytes_ = 0;
  std::vector<std::pair<std::u16string, std::shared_ptr<CacheStore>>> names_;
};
class CacheStorageState final {
 public:
  explicit CacheStorageState(CacheStorageLimits limits = {}) : limits_(limits) {}
  std::shared_ptr<CacheNamespace> scope(std::string_view origin, CacheLimits limits = {}) {
    // Security/opaque-origin decisions belong to the existing origin owner.
    CacheRequire(origin.size() <= limits.keyBytes, CacheError::Limit);
    const auto found = origins_.find(origin);
    if (found != origins_.end()) return found->second;
    CacheRequire(origins_.size() < limits_.origins && originBytes_ <= limits_.originBytes &&
        origin.size() <= limits_.originBytes - originBytes_, CacheError::Limit);
    auto space = std::make_shared<CacheNamespace>(limits);
    origins_.emplace(std::string(origin), space);
    originBytes_ += origin.size();
    return space;
  }
  size_t origins() const noexcept { return origins_.size(); }
  size_t originBytes() const noexcept { return originBytes_; }
 private:
  CacheStorageLimits limits_;
  size_t originBytes_ = 0;
  std::map<std::string, std::shared_ptr<CacheNamespace>, std::less<>> origins_;
};
}
