#include "resource_network/model_cache_internal.h"

namespace ruyijs::resource_network {
Status OpenRemoteCacheScope(CacheExchange exchange,void* context,CacheScope** result) noexcept {
  return CacheBoundary([&] {
    auto scope=std::make_unique<CacheScope>();
    scope->remote=std::make_shared<CacheRemote>();
    scope->remote->exchange=exchange;scope->remote->context=context;
    scope->remote->released.reserve(10000);
    *result=scope.release();
  });
}
void ReleaseRemoteCacheHandle(const std::shared_ptr<CacheRemote>& remote,uint64_t id) noexcept {
  // Session maximum is 10,000: preallocated release slots never allocate in GC.
  if (remote->released.size()<remote->released.capacity()) remote->released.push_back(id);
}
Status CacheRemoteCall(const std::shared_ptr<CacheRemote>& remote,CacheWireRequest request,
                       CacheWireReply* reply) noexcept {
  try {
    request.releases=remote->released;
    std::string outgoing,incoming;
    if (!EncodeCacheRequest(request,&outgoing)) return {ErrorCode::Range,"Cache request exceeds channel bounds"};
    if (!remote->exchange(remote->context,outgoing,&incoming) || !DecodeCacheReply(incoming,reply))
      return {ErrorCode::InvalidState,"Cache Worker session is unavailable"};
    remote->released.clear();
    switch (reply->code) {
      case ErrorCode::None:return {};
      case ErrorCode::OutOfMemory:return {reply->code,"Cache allocation failed"};
      case ErrorCode::Type:return {reply->code,"Invalid Cache request"};
      case ErrorCode::Range:return {reply->code,"Cache owner limit exceeded"};
      case ErrorCode::InvalidState:return {reply->code,"Cache batch contains overlapping requests"};
    }
  } catch (...) {return {ErrorCode::OutOfMemory,"Cache allocation failed"};}
  return {ErrorCode::InvalidState,"Invalid Cache Worker reply"};
}
}
