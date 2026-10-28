#pragma once
#include "resource_network/model_cache.h"
#include "resource_network/cache_wire.h"
#include "domain/resource_network/cache_storage.h"
#include <new>

namespace ruyijs::resource_network {
namespace cache_domain = ruyijs::domain::resource_network;
struct CacheScope { std::shared_ptr<cache_domain::CacheNamespace> space; std::shared_ptr<CacheRemote> remote; };
struct CacheHandle { std::shared_ptr<cache_domain::CacheStore> store; std::shared_ptr<CacheRemote> remote; uint64_t remoteId=0; };
inline Status CacheFailure(cache_domain::CacheError error) noexcept {
  using E = cache_domain::CacheError;
  switch (error) {
    case E::InvalidRequest: return {ErrorCode::Type, "Invalid Cache request"};
    case E::InvalidResponse: return {ErrorCode::Type, "Invalid Cache response"};
    case E::Duplicate: return {ErrorCode::InvalidState, "Cache batch contains overlapping requests"};
    case E::Limit: return {ErrorCode::Range, "Cache owner limit exceeded"};
  }
  return {ErrorCode::InvalidState, "Invalid Cache operation"};
}
template<class Work>
Status CacheBoundary(Work work) noexcept {
  try { work(); return {}; }
  catch (cache_domain::CacheError error) { return CacheFailure(error); }
  catch (const std::length_error&) { return {ErrorCode::Range, "Cache size limit exceeded"}; }
  catch (...) { return {ErrorCode::OutOfMemory, "Cache allocation failed"}; }
}
inline Status CacheMissing() noexcept { return {ErrorCode::InvalidState, "Cache handle is unavailable"}; }
template<class Work> Status CacheRemoteBoundary(Work work) noexcept {
  try {return work();} catch (...) {return {ErrorCode::OutOfMemory,"Cache allocation failed"};}
}
cache_domain::HttpHeaders CacheHeadersIn(const HeaderPairs&, const cache_domain::CacheLimits&);
cache_domain::CacheRequest CacheRequestIn(const RequestDraft&, const cache_domain::CacheLimits&);
RequestDraft CacheRequestOut(const cache_domain::CacheRequest&);
cache_domain::CacheResponse CacheResponseIn(const CacheResponseDTO&, const cache_domain::CacheLimits&);
CacheResponseDTO CacheResponseOut(const cache_domain::CacheResponse&);
inline cache_domain::CacheQuery CacheQueryIn(const CacheQueryDTO& dto,
                                             const cache_domain::CacheRequest& request) noexcept {
  return {dto.hasRequest ? &request : nullptr, dto.ignoreMethod, dto.ignoreSearch, dto.ignoreVary};
}
}
