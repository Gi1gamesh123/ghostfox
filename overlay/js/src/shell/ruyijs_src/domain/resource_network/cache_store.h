#pragma once
#include "domain/resource_network/cache_match.h"
#include <algorithm>

namespace ruyijs::domain::resource_network {
class CacheNamespace;
class CacheStore final {
 public:
  ~CacheStore() noexcept {
    if (!accounted_) return;
    --budget_->stores;
    budget_->entries -= entries_.size();
    budget_->bytes -= bytes_;
  }
  CacheStore(const CacheStore&) = delete;
  CacheStore& operator=(const CacheStore&) = delete;
  const CacheEntries& entries() const noexcept { return entries_; }
  const CacheLimits& limits() const noexcept { return limits_; }
  size_t bytes() const noexcept { return bytes_; }
  void put(const std::vector<CacheEntry>& batch);
  bool erase(const CacheQuery& query) {
    CacheEntries next;
    next.reserve(entries_.size());
    size_t retained = 0;
    for (const auto& entry : entries_) if (!CacheMatches(query, *entry)) {
      next.push_back(entry);
      retained += entry->bytes;
    }
    const bool changed = next.size() != entries_.size();
    commit(next, retained);
    return changed;
  }
 private:
  friend class CacheNamespace;
  CacheStore(std::shared_ptr<CacheBudget> budget, CacheLimits limits)
      : budget_(std::move(budget)), limits_(limits) {}
  void commit(CacheEntries& next, size_t bytes) noexcept {
    budget_->entries = budget_->entries - entries_.size() + next.size();
    budget_->bytes = budget_->bytes - bytes_ + bytes;
    entries_.swap(next);
    bytes_ = bytes;
  }
  std::shared_ptr<CacheBudget> budget_;
  CacheLimits limits_;
  CacheEntries entries_;
  size_t bytes_ = 0;
  bool accounted_ = false;
};
}
#include "domain/resource_network/cache_batch.h"
