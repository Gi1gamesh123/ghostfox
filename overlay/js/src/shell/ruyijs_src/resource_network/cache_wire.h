#pragma once
#include "resource_network/model_cache.h"
#include "resource_network/worker_network_wire.h"
#include <memory>

namespace ruyijs::resource_network {
enum class CacheWireOp:uint32_t { Open,Has,Remove,Names,Query,StorageMatch,Put,Delete };
struct CacheWireRequest {
  CacheWireOp op=CacheWireOp::Names;
  uint64_t handle=0;
  std::u16string name;
  CacheStorageQueryDTO query;
  CachePairsDTO pairs;
  std::vector<uint64_t> releases;
};
struct CacheWireReply {
  ErrorCode code=ErrorCode::None;
  uint64_t handle=0;
  bool flag=false;
  std::vector<std::u16string> names;
  CacheResults results;
};
using CacheExchange=bool (*)(void*,std::string_view,std::string*);
struct CacheRemote {
  CacheExchange exchange=nullptr;
  void* context=nullptr;
  std::vector<uint64_t> released;
};
bool EncodeCacheRequest(const CacheWireRequest&,std::string*);
bool DecodeCacheRequest(std::string_view,CacheWireRequest*);
bool EncodeCacheReply(const CacheWireReply&,std::string*);
bool DecodeCacheReply(std::string_view,CacheWireReply*);
Status CacheRemoteCall(const std::shared_ptr<CacheRemote>&,CacheWireRequest,CacheWireReply*) noexcept;
Status OpenRemoteCacheScope(CacheExchange,void*,CacheScope**) noexcept;
void ReleaseRemoteCacheHandle(const std::shared_ptr<CacheRemote>&,uint64_t) noexcept;
namespace cache_wire {
using Writer=worker_network_wire::Writer;
using Reader=worker_network_wire::Reader;
bool Text16(Writer&,std::u16string_view);
bool Text16(Reader&,std::u16string*);
bool Request(Writer&,const RequestDraft&);
bool Request(Reader&,RequestDraft*);
bool Response(Writer&,const CacheResponseDTO&);
bool Response(Reader&,CacheResponseDTO*);
}
}
