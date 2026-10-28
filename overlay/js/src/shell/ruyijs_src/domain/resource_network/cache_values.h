#pragma once
#include "domain/resource_network/http_headers.h"
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace ruyijs::domain::resource_network {
enum class CacheError { InvalidRequest, InvalidResponse, Duplicate, Limit };
inline void CacheRequire(bool condition, CacheError error) {
  if (!condition) throw error;
}
struct CacheLimits {
  size_t keyBytes = 16384, bodyBytes = 8 * 1024 * 1024;
  size_t headerFields = 256, headerBytes = 64 * 1024;
  size_t stores = 10000, entries = 10000, retainedBytes = 64 * 1024 * 1024, batch = 256;
};
struct CacheStorageLimits { size_t origins = 10000, originBytes = 16 * 1024 * 1024; };
struct CacheRequest {
  std::string method = "GET", url;
  HttpHeaders headers;
  std::string mode = "cors", credentials = "same-origin", cache = "default",
      redirect = "follow", referrer = "about:client", referrerPolicy;
  std::u16string integrity;
  bool keepalive = false;
};
struct CacheResponse {
  int status = 200;
  std::string statusText, url, type = "default", body;
  HttpHeaders headers;
  bool redirected = false, bytes = false, nullBody = true;
};
struct CacheEntry { CacheRequest request; CacheResponse response; size_t bytes = 0; };
using CacheEntries = std::vector<std::shared_ptr<const CacheEntry>>;
struct CacheQuery {
  const CacheRequest* request = nullptr;
  bool ignoreMethod = false, ignoreSearch = false, ignoreVary = false;
};
struct CacheBudget { size_t stores = 0, entries = 0, bytes = 0; };
inline void CacheAddSize(size_t amount, size_t maximum, size_t* total) {
  CacheRequire(*total <= maximum && amount <= maximum - *total, CacheError::Limit);
  *total += amount;
}
inline size_t CacheHeaderBytes(const HttpHeaders& headers, const CacheLimits& limits) {
  CacheRequire(headers.value_count() <= limits.headerFields, CacheError::Limit);
  size_t total = 0;
  for (const auto& [name, value] : headers.raw_pairs()) {
    CacheRequire(valid_header_name(name) && valid_header_value(value), CacheError::InvalidResponse);
    CacheAddSize(name.size(), limits.headerBytes, &total);
    CacheAddSize(value.size(), limits.headerBytes, &total);
  }
  return total;
}
inline size_t CacheRequestBytes(const CacheRequest& request, const CacheLimits& limits) {
  size_t total = CacheHeaderBytes(request.headers, limits);
  for (const auto* text : {&request.method, &request.url, &request.mode, &request.credentials,
      &request.cache, &request.redirect, &request.referrer, &request.referrerPolicy}) {
    CacheRequire(text->size() <= limits.keyBytes, CacheError::Limit);
    CacheAddSize(text->size(), limits.retainedBytes, &total);
  }
  CacheRequire(request.integrity.size() <= limits.keyBytes / sizeof(char16_t), CacheError::Limit);
  CacheAddSize(request.integrity.size() * sizeof(char16_t), limits.retainedBytes, &total);
  return total;
}
inline size_t CacheEntryBytes(const CacheEntry& entry, const CacheLimits& limits) {
  size_t total = CacheRequestBytes(entry.request, limits);
  CacheAddSize(CacheHeaderBytes(entry.response.headers, limits), limits.retainedBytes, &total);
  for (const auto* text : {&entry.response.statusText, &entry.response.url, &entry.response.type}) {
    CacheRequire(text->size() <= limits.keyBytes, CacheError::Limit);
    CacheAddSize(text->size(), limits.retainedBytes, &total);
  }
  CacheRequire(entry.response.body.size() <= limits.bodyBytes, CacheError::Limit);
  CacheAddSize(entry.response.body.size(), limits.retainedBytes, &total);
  return total;
}
}
