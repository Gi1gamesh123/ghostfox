#include "resource_network/cache_session.h"
#include "resource_network/cache_wire.h"
#include <map>
#include <new>

namespace ruyijs::resource_network {
namespace {
struct ScopeDelete {void operator()(CacheScope* p) const noexcept {DeleteCacheScope(p);}};
struct HandleDelete {void operator()(CacheHandle* p) const noexcept {DeleteCacheHandle(p);}};
class Session final:public CacheSession {
 public:
  std::unique_ptr<CacheScope,ScopeDelete> scope;
  std::map<uint64_t,std::unique_ptr<CacheHandle,HandleDelete>> handles;
  uint64_t next=1;
  Status Execute(const CacheWireRequest& request,CacheWireReply* reply) {
    for (auto id:request.releases) handles.erase(id);
    if (request.op==CacheWireOp::Open) {
      if (handles.size()>=10000 || !next) return {ErrorCode::Range,"Cache session handle limit exceeded"};
      // Allocate map node before CacheOpen can publish a new named store.
      const uint64_t id=next++;
      auto inserted=handles.emplace(id,nullptr).first;
      CacheHandle* handle=nullptr;const auto status=CacheOpen(scope.get(),request.name,&handle);
      if (status.code!=ErrorCode::None) {handles.erase(inserted);return status;}
      inserted->second.reset(handle);reply->handle=id;return {};
    }
    if (request.op==CacheWireOp::Has) return CacheHas(scope.get(),request.name,&reply->flag);
    if (request.op==CacheWireOp::Remove) return CacheRemoveName(scope.get(),request.name,&reply->flag);
    if (request.op==CacheWireOp::Names) return CacheNames(scope.get(),&reply->names);
    if (request.op==CacheWireOp::StorageMatch) {
      CacheResponseDTO response;
      auto status=CacheStorageMatch(scope.get(),request.query,&response,&reply->flag);
      if (reply->flag) reply->results.responses.push_back(std::move(response));
      return status;
    }
    const auto found=handles.find(request.handle);
    if (found==handles.end()) return {ErrorCode::InvalidState,"Cache session handle is unavailable"};
    CacheHandle* handle=found->second.get();
    if (request.op==CacheWireOp::Query) return CacheQuery(handle,request.query.query,&reply->results);
    if (request.op==CacheWireOp::Put) return CachePutBatch(handle,request.pairs);
    if (request.op==CacheWireOp::Delete) return CacheDelete(handle,request.query.query,&reply->flag);
    return {ErrorCode::Type,"Invalid Cache operation"};
  }
  bool Request(std::string_view input,std::string* output) noexcept override {
    try {
      CacheWireRequest request;CacheWireReply reply;
      if (!DecodeCacheRequest(input,&request)) return false;
      reply.code=Execute(request,&reply).code;
      return EncodeCacheReply(reply,output);
    } catch (...) {
      CacheWireReply reply;reply.code=ErrorCode::OutOfMemory;
      try {return EncodeCacheReply(reply,output);} catch (...) {return false;}
    }
  }
};
}
Status NewCacheSession(Model& model,std::string_view origin,std::shared_ptr<CacheSession>* result) noexcept {
  try {
    auto session=std::make_shared<Session>();
    // Opaque-origin Workers still use the common service for non-Cache APIs.
    // An empty scope rejects Cache frames without creating a shared null origin.
    if (origin=="null") {*result=std::move(session);return {};}
    CacheScope* scope=nullptr;auto status=model.OpenCacheScope(origin,&scope);
    if (status.code!=ErrorCode::None) return status;
    session->scope.reset(scope);*result=std::move(session);return {};
  } catch (...) {return {ErrorCode::OutOfMemory,"Cache session allocation failed"};}
}
}
