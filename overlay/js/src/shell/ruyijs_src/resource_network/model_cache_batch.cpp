#include "resource_network/model_cache_internal.h"

namespace ruyijs::resource_network {
Status CachePutBatch(CacheHandle* handle, const CachePairsDTO& values) noexcept {
  if (handle && handle->remote) return CacheRemoteBoundary([&] {
    CacheWireRequest request;request.op=CacheWireOp::Put;request.handle=handle->remoteId;request.pairs=values;
    CacheWireReply reply;return CacheRemoteCall(handle->remote,std::move(request),&reply);
  });
  if (!handle || !handle->store) return CacheMissing();
  return CacheBoundary([&] {
    const auto& limits = handle->store->limits();
    cache_domain::CacheRequire(values.size() <= limits.batch, cache_domain::CacheError::Limit);
    std::vector<cache_domain::CacheEntry> batch;
    batch.reserve(values.size());
    size_t staged = 0;
    for (const auto& value : values) {
      cache_domain::CacheRequire(value.request.nullBody && value.request.body.empty(),
          cache_domain::CacheError::InvalidRequest);
      cache_domain::CacheEntry entry;
      entry.request = CacheRequestIn(value.request, limits);
      entry.response = CacheResponseIn(value.response, limits);
      cache_domain::CacheAddSize(cache_domain::CacheEntryBytes(entry, limits),
          limits.retainedBytes, &staged);
      batch.push_back(std::move(entry));
    }
    handle->store->put(batch);
  });
}
}
