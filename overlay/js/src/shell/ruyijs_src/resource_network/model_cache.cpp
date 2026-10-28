#include "resource_network/model_internal.h"
#include "resource_network/model_cache_internal.h"

namespace ruyijs::resource_network {
Status Model::OpenCacheScope(std::string_view origin, CacheScope** result) noexcept {
  if (!impl_) return {ErrorCode::InvalidState, "Resource model is not initialized"};
  return CacheBoundary([&] {
    auto scope = std::make_unique<CacheScope>();
    scope->space = impl_->resources().cache_storage().scope(origin);
    *result = scope.release();
  });
}
void DeleteCacheScope(CacheScope* scope) noexcept { delete scope; }
void DeleteCacheHandle(CacheHandle* handle) noexcept {
  if (handle && handle->remote) ReleaseRemoteCacheHandle(handle->remote,handle->remoteId);
  delete handle;
}
Status CacheOpen(CacheScope* scope, std::u16string_view name, CacheHandle** result) noexcept {
  if (scope && scope->remote) return CacheRemoteBoundary([&]() -> Status {
    auto handle=std::make_unique<CacheHandle>();
    CacheWireRequest request;request.op=CacheWireOp::Open;request.name=name;
    CacheWireReply reply;
    auto status=CacheRemoteCall(scope->remote,std::move(request),&reply);
    if (status.code!=ErrorCode::None) return status;
    handle->remote=scope->remote;handle->remoteId=reply.handle;*result=handle.release();return {};
  });
  if (!scope || !scope->space) return CacheMissing();
  return CacheBoundary([&] {
    // Prepare handle allocation before domain open can publish a new name.
    auto handle = std::make_unique<CacheHandle>();
    handle->store = scope->space->open(name);
    *result = handle.release();
  });
}
Status CacheHas(CacheScope* scope, std::u16string_view name, bool* result) noexcept {
  if (scope && scope->remote) return CacheRemoteBoundary([&] {
    CacheWireRequest request;request.op=CacheWireOp::Has;request.name=name;CacheWireReply reply;
    auto status=CacheRemoteCall(scope->remote,std::move(request),&reply);*result=reply.flag;return status;
  });
  if (!scope || !scope->space) return CacheMissing();
  return CacheBoundary([&] {
    scope->space->validateName(name);
    *result = bool(scope->space->find(name));
  });
}
Status CacheRemoveName(CacheScope* scope, std::u16string_view name, bool* result) noexcept {
  if (scope && scope->remote) return CacheRemoteBoundary([&] {
    CacheWireRequest request;request.op=CacheWireOp::Remove;request.name=name;CacheWireReply reply;
    auto status=CacheRemoteCall(scope->remote,std::move(request),&reply);*result=reply.flag;return status;
  });
  if (!scope || !scope->space) return CacheMissing();
  return CacheBoundary([&] { *result = scope->space->erase(name); });
}
Status CacheNames(CacheScope* scope, std::vector<std::u16string>* result) noexcept {
  if (scope && scope->remote) return CacheRemoteBoundary([&] {
    CacheWireRequest request;request.op=CacheWireOp::Names;CacheWireReply reply;
    auto status=CacheRemoteCall(scope->remote,std::move(request),&reply);result->swap(reply.names);return status;
  });
  if (!scope || !scope->space) return CacheMissing();
  return CacheBoundary([&] {
    auto next = scope->space->names();
    result->swap(next);
  });
}
}
