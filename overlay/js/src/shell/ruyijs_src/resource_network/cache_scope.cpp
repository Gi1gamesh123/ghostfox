#include "resource_network/cache.h"
#include "resource_network/cache_wire.h"
#include "resource_network/cache_session.h"
#include "domain/browser_environment/url_value.h"

namespace ruyijs::resource_network {
bool OpenRealmCacheScope(State& state,CacheScope** result) {
  if (state.workerScope)
    return CacheReport(state.cx,OpenRemoteCacheScope(state.cacheExchange,state.cacheExchangeContext,result));
  std::string origin;
  if (!state.cacheOrigin || !state.cacheOrigin(state.cacheOriginContext,&origin))
    return Error(state.cx,"Cache origin is unavailable");
  if (origin=="null") return Error(state.cx,"Cache storage requires a tuple origin");
  return CacheReport(state.cx,state.resources.OpenCacheScope(origin,result));
}
void Realm::SetCacheOriginPort(bool (*port)(void*,std::string*),void* context) {
  if (state_) {state_->cacheOrigin=port;state_->cacheOriginContext=context;}
}
std::shared_ptr<CacheSession> Realm::OpenCacheSession() {
  if (!state_) return {};
  std::string origin;
  if (!state_->cacheOrigin || !state_->cacheOrigin(state_->cacheOriginContext,&origin)) return {};
  std::shared_ptr<CacheSession> result;
  if (!CacheReport(state_->cx,NewCacheSession(state_->resources,origin,&result))) return {};
  return result;
}
}
