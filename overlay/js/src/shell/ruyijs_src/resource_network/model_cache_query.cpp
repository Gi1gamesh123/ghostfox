#include "resource_network/model_cache_internal.h"

namespace ruyijs::resource_network {
Status CacheQuery(CacheHandle* handle, const CacheQueryDTO& dto, CacheResults* result) noexcept {
  if (handle && handle->remote) return CacheRemoteBoundary([&] {
    CacheWireRequest request;request.op=CacheWireOp::Query;request.handle=handle->remoteId;request.query.query=dto;
    CacheWireReply reply;auto status=CacheRemoteCall(handle->remote,std::move(request),&reply);
    *result=std::move(reply.results);return status;
  });
  if (!handle || !handle->store) return CacheMissing();
  return CacheBoundary([&] {
    cache_domain::CacheRequire(dto.kind == CacheResultKind::FirstResponse ||
        dto.kind == CacheResultKind::AllResponses || dto.kind == CacheResultKind::Requests,
        cache_domain::CacheError::InvalidRequest);
    const auto request = dto.hasRequest ? CacheRequestIn(dto.request, handle->store->limits()) :
        cache_domain::CacheRequest{};
    const auto query = CacheQueryIn(dto, request);
    CacheResults next;
    for (const auto& entry : handle->store->entries()) if (cache_domain::CacheMatches(query, *entry)) {
      if (dto.kind == CacheResultKind::Requests) next.requests.push_back(CacheRequestOut(entry->request));
      else next.responses.push_back(CacheResponseOut(entry->response));
      if (dto.kind == CacheResultKind::FirstResponse) break;
    }
    result->requests.swap(next.requests);
    result->responses.swap(next.responses);
  });
}
Status CacheStorageMatch(CacheScope* scope, const CacheStorageQueryDTO& dto,
                         CacheResponseDTO* result, bool* found) noexcept {
  if (scope && scope->remote) return CacheRemoteBoundary([&] {
    CacheWireRequest request;request.op=CacheWireOp::StorageMatch;request.query=dto;CacheWireReply reply;
    auto status=CacheRemoteCall(scope->remote,std::move(request),&reply);
    *found=reply.flag;if (*found && !reply.results.responses.empty()) *result=std::move(reply.results.responses.front());
    return status;
  });
  if (!scope || !scope->space) return CacheMissing();
  return CacheBoundary([&] {
    if (dto.hasName) scope->space->validateName(dto.name);
    cache_domain::CacheRequire(dto.query.hasRequest, cache_domain::CacheError::InvalidRequest);
    const auto request = CacheRequestIn(dto.query.request, scope->space->limits());
    const auto query = CacheQueryIn(dto.query, request);
    for (const auto& [name, store] : scope->space->stores()) {
      if (dto.hasName && name != dto.name) continue;
      for (const auto& entry : store->entries()) if (cache_domain::CacheMatches(query, *entry)) {
        auto next = CacheResponseOut(entry->response);
        *result = std::move(next);
        *found = true;
        return;
      }
    }
    *found = false;
  });
}
Status CacheDelete(CacheHandle* handle, const CacheQueryDTO& dto, bool* result) noexcept {
  if (handle && handle->remote) return CacheRemoteBoundary([&] {
    CacheWireRequest request;request.op=CacheWireOp::Delete;request.handle=handle->remoteId;request.query.query=dto;
    CacheWireReply reply;auto status=CacheRemoteCall(handle->remote,std::move(request),&reply);*result=reply.flag;return status;
  });
  if (!handle || !handle->store) return CacheMissing();
  return CacheBoundary([&] {
    cache_domain::CacheRequire(dto.hasRequest, cache_domain::CacheError::InvalidRequest);
    const auto request = CacheRequestIn(dto.request, handle->store->limits());
    *result = handle->store->erase(CacheQueryIn(dto, request));
  });
}
}
