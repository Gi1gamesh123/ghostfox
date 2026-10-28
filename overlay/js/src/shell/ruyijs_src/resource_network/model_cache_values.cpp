#include "resource_network/model_cache_internal.h"

namespace ruyijs::resource_network {
namespace d = cache_domain;
d::HttpHeaders CacheHeadersIn(const HeaderPairs& fields, const d::CacheLimits& limits) {
  d::CacheRequire(fields.size() <= limits.headerFields, d::CacheError::Limit);
  d::HttpHeaders headers;
  size_t bytes = 0;
  for (const auto& [name, value] : fields) {
    d::CacheRequire(d::valid_header_name(name) && d::valid_header_value(value), d::CacheError::InvalidResponse);
    d::CacheAddSize(name.size(), limits.headerBytes, &bytes);
    d::CacheAddSize(value.size(), limits.headerBytes, &bytes);
    headers.append(name, std::string(d::trim_header_value(value)));
  }
  return headers;
}
d::CacheRequest CacheRequestIn(const RequestDraft& source, const d::CacheLimits& limits) {
  // Query snapshots intentionally omit Body and signal; no transfer or consume.
  for (const auto* value : {&source.method, &source.url, &source.mode, &source.credentials,
      &source.cache, &source.redirect, &source.referrer, &source.referrer_policy})
    d::CacheRequire(value->size() <= limits.keyBytes, d::CacheError::Limit);
  d::CacheRequire(source.integrity.size() <= limits.keyBytes / sizeof(char16_t), d::CacheError::Limit);
  d::CacheRequest result;
  result.method = source.method; result.url = source.url;
  result.headers = CacheHeadersIn(source.headers, limits);
  result.mode = source.mode; result.credentials = source.credentials; result.cache = source.cache;
  result.redirect = source.redirect; result.referrer = source.referrer;
  result.referrerPolicy = source.referrer_policy; result.integrity = source.integrity;
  result.keepalive = source.keepalive;
  (void)d::CacheRequestBytes(result, limits);
  return result;
}
RequestDraft CacheRequestOut(const d::CacheRequest& source) {
  RequestDraft result;
  result.method = source.method; result.url = source.url;
  result.headers = source.headers.raw_pairs();
  result.mode = source.mode; result.credentials = source.credentials; result.cache = source.cache;
  result.redirect = source.redirect; result.referrer = source.referrer;
  result.referrer_policy = source.referrerPolicy; result.integrity = source.integrity;
  result.keepalive = source.keepalive;
  return result;
}
d::CacheResponse CacheResponseIn(const CacheResponseDTO& source, const d::CacheLimits& limits) {
  d::CacheRequire(source.body.size() <= limits.bodyBytes, d::CacheError::Limit);
  for (const auto* text : {&source.statusText, &source.url, &source.type})
    d::CacheRequire(text->size() <= limits.keyBytes, d::CacheError::Limit);
  d::CacheResponse result;
  result.status = source.status; result.statusText = source.statusText;
  result.url = source.url; result.type = source.type; result.body = source.body;
  result.headers = CacheHeadersIn(source.headers, limits);
  result.bytes = source.kind == BodyKind::Bytes;
  result.redirected = source.redirected; result.nullBody = source.nullBody;
  return result;
}
CacheResponseDTO CacheResponseOut(const d::CacheResponse& source) {
  CacheResponseDTO result;
  result.status = source.status; result.statusText = source.statusText;
  result.url = source.url; result.type = source.type; result.body = source.body;
  result.headers = source.headers.raw_pairs();
  result.kind = source.bytes ? BodyKind::Bytes : BodyKind::Text;
  result.redirected = source.redirected; result.nullBody = source.nullBody;
  return result;
}
}
