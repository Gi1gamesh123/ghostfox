#pragma once
#include "domain/resource_network/cache_store.h"

namespace ruyijs::domain::resource_network {
inline void CacheStore::put(const std::vector<CacheEntry>& batch) {
  CacheRequire(batch.size() <= limits_.batch, CacheError::Limit);
  if (batch.empty()) return;
  size_t stagedBytes = 0;
  for (size_t i = 0; i < batch.size(); ++i) {
    CacheValidate(batch[i], limits_);
    CacheAddSize(CacheEntryBytes(batch[i], limits_), limits_.retainedBytes, &stagedBytes);
    for (size_t j = 0; j < i; ++j)
      CacheRequire(!CacheBatchOverlap(batch[i].request, batch[j]), CacheError::Duplicate);
  }
  CacheEntries next(entries_);
  for (const auto& value : batch) {
    CacheQuery query{&value.request};
    next.erase(std::remove_if(next.begin(), next.end(),
        [&](const auto& entry) { return CacheMatches(query, *entry); }), next.end());
    auto entry = std::make_shared<CacheEntry>(value);
    entry->bytes = CacheEntryBytes(*entry, limits_);
    next.push_back(std::move(entry));
  }
  size_t retained = 0;
  for (const auto& entry : next) CacheAddSize(entry->bytes, limits_.retainedBytes, &retained);
  CacheRequire(budget_->entries >= entries_.size() && budget_->bytes >= bytes_, CacheError::Limit);
  const size_t otherEntries = budget_->entries - entries_.size(), otherBytes = budget_->bytes - bytes_;
  CacheRequire(otherEntries <= limits_.entries && next.size() <= limits_.entries - otherEntries,
      CacheError::Limit);
  CacheRequire(otherBytes <= limits_.retainedBytes && retained <= limits_.retainedBytes - otherBytes,
      CacheError::Limit);
  commit(next, retained);
}
}
