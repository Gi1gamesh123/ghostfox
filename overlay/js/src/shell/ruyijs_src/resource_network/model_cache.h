#pragma once
#include "resource_network/request_value.h"

namespace ruyijs::resource_network {
struct CacheScope;
struct CacheHandle;
struct CacheResponseDTO {
  int status = 200;
  std::string statusText, url, type = "default", body;
  HeaderPairs headers;
  BodyKind kind = BodyKind::Text;
  bool redirected = false, nullBody = true;
};
struct CachePairDTO { RequestDraft request; CacheResponseDTO response; };
using CachePairsDTO = std::vector<CachePairDTO>;
enum class CacheResultKind { FirstResponse, AllResponses, Requests };
struct CacheQueryDTO {
  RequestDraft request;
  bool hasRequest = false, ignoreMethod = false, ignoreSearch = false, ignoreVary = false;
  CacheResultKind kind = CacheResultKind::FirstResponse;
};
struct CacheResults {
  std::vector<RequestDraft> requests;
  std::vector<CacheResponseDTO> responses;
};
struct CacheStorageQueryDTO {
  CacheQueryDTO query;
  bool hasName = false;
  std::u16string name;
};
void DeleteCacheScope(CacheScope*) noexcept;
void DeleteCacheHandle(CacheHandle*) noexcept;
Status CacheOpen(CacheScope*, std::u16string_view, CacheHandle**) noexcept;
Status CacheHas(CacheScope*, std::u16string_view, bool*) noexcept;
Status CacheRemoveName(CacheScope*, std::u16string_view, bool*) noexcept;
Status CacheNames(CacheScope*, std::vector<std::u16string>*) noexcept;
Status CacheQuery(CacheHandle*, const CacheQueryDTO&, CacheResults*) noexcept;
Status CacheStorageMatch(CacheScope*, const CacheStorageQueryDTO&, CacheResponseDTO*, bool*) noexcept;
Status CachePutBatch(CacheHandle*, const CachePairsDTO&) noexcept;
Status CacheDelete(CacheHandle*, const CacheQueryDTO&, bool*) noexcept;
}
