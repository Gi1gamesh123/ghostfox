#pragma once
#include "domain/resource_network/cache_values.h"
#include <string_view>

namespace ruyijs::domain::resource_network {
inline std::string_view CacheURL(std::string_view url, bool ignoreSearch) noexcept {
  url = url.substr(0, url.find('#'));
  return ignoreSearch ? url.substr(0, url.find('?')) : url;
}
template<class Visit>
bool CacheVaryFields(const HttpHeaders& headers, Visit visit) {
  for (const auto& [name, value] : headers.raw_pairs()) {
    if (name != "vary") continue;
    std::string_view remaining(value);
    do {
      auto comma = remaining.find(',');
      auto field = trim_header_value(remaining.substr(0, comma));
      if (!field.empty() && !visit(field)) return false;
      if (comma == std::string_view::npos) break;
      remaining.remove_prefix(comma + 1);
    } while (true);
  }
  return true;
}
inline bool CacheVaryMatches(const CacheRequest& query, const CacheEntry& entry) {
  return CacheVaryFields(entry.response.headers, [&](std::string_view name) {
    // The donor suppresses invalid InternalHeaders::Get and compares empty values.
    if (!valid_header_name(name)) return true;
    const std::string key(name);
    return query.headers.combined_get(key) == entry.request.headers.combined_get(key);
  });
}
inline bool CacheMatches(const CacheQuery& query, const CacheEntry& entry) {
  if (!query.request) return true;
  const auto& request = *query.request;
  if (!query.ignoreMethod && request.method != "GET") return false;
  if (CacheURL(request.url, query.ignoreSearch) != CacheURL(entry.request.url, query.ignoreSearch))
    return false;
  return query.ignoreVary || entry.response.type == "opaque" || CacheVaryMatches(request, entry);
}
inline bool CacheBatchOverlap(const CacheRequest& request, const CacheEntry& earlier) {
  // AutoUtils::MatchInPutList does not apply QueryCache's opaque-response shortcut.
  return CacheURL(request.url, false) == CacheURL(earlier.request.url, false) &&
      CacheVaryMatches(request, earlier);
}
inline void CacheValidate(const CacheEntry& entry, const CacheLimits& limits) {
  const auto& request = entry.request;
  CacheRequire(request.method == "GET" && (request.url.rfind("http://", 0) == 0 ||
      request.url.rfind("https://", 0) == 0), CacheError::InvalidRequest);
  const auto& response = entry.response;
  CacheRequire(response.status >= 0 && response.status <= 599 && response.status != 206,
      CacheError::InvalidResponse);
  CacheRequire(response.type == "opaque" || CacheVaryFields(response.headers,
      [](std::string_view name) { return name != "*"; }), CacheError::InvalidResponse);
  CacheRequire(!response.nullBody || response.body.empty(), CacheError::InvalidResponse);
  (void)CacheEntryBytes(entry, limits);
}
}
