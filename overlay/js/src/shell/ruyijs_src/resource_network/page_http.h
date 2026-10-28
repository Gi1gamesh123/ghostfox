#ifndef RUYIJS_PAGE_HTTP_H
#define RUYIJS_PAGE_HTTP_H
#include "resource_network/model.h"
#include "domain/resource_network/http_redirect.h"
#include <string>
#include <utility>
#include <vector>

namespace ruyijs::resource_network {
using RedirectMode = ruyijs::domain::resource_network::RedirectMode;
struct HttpResult {
  int status = 0;
  std::string body, error, statusText;
  HeaderPairs headers;
  BodyKind kind = BodyKind::Text;
  std::string finalURL;
  // The Referer the final request carried (policy applied); a navigation's
  // document reports it as document.referrer.
  std::string referrer;
  bool redirected = false, opaqueRedirect = false, crossOrigin = false;
  std::string protocol;
  uint64_t encodedBytes=0,decodedBytes=0;
  bool transferred=false;
  // Windows QPC milliseconds from the existing Necko transport boundaries.
  double requestTick=0,responseTick=0,headersTick=0;
};
struct HttpTimingHop {
  std::string url;HeaderPairs headers;double start=0,end=0;
  double requestStart=0,responseStart=0,headersStart=0;
};
// One whole request as PerformHttp received it; the parent runs it unchanged.
struct Model::RemoteRequest {
  const std::string& method;
  const std::string& url;
  const HeaderPairs& headers;
  const std::string& body;
  RedirectMode redirect;
  const HttpOptions& options;
};
struct HttpObservation {
  const std::string& url;
  const HttpContext& context;
  const HttpOptions& options;
  const HttpResult& result;
  const std::vector<HttpTimingHop>& hops;
};
bool ResolveURL(std::string_view base, std::string_view input, std::string* url);
bool PerformHttp(Model& model, const std::string& method, const std::string& url,
                 const HeaderPairs& headers, const std::string& body, HttpResult* result,
                 RedirectMode redirect = RedirectMode::Follow, const HttpOptions& options = {});
}
#endif
