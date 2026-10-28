#ifndef RUYIJS_HTTP_EXCHANGE_H
#define RUYIJS_HTTP_EXCHANGE_H
#include "resource_network/page_http.h"
#include "domain/browser_environment/url_value.h"
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace ruyijs::resource_network {
// One request's redirect chain, driven one hop at a time on the thread that
// owns `model`. Next() prepares a hop (defaults, Cookie, NetLog record) and
// reports whether it needs the transport; Deliver() hands the transport's
// outcome back and applies Set-Cookie, timing and the redirect policy. Hops
// served without the transport (blob: URLs, resource-directory entries,
// failures) are handled inside Next(). PerformHttp runs the transport inline;
// AsyncHttp runs it on a background thread. Only the transport leaves this
// thread, so Cookie, NetLog and redirect state keep one owner.
class HttpExchange final {
 public:
  HttpExchange(Model& model, const std::string& method, const std::string& url,
               const HeaderPairs& headers, const std::string& body, RedirectMode mode,
               const HttpOptions& options);
  HttpExchange(const HttpExchange&) = delete;
  HttpExchange& operator=(const HttpExchange&) = delete;
  // True when the prepared hop needs the transport; false once finished.
  bool Next();
  void Deliver(bool ok, HttpResult&& response);
  // The prepared hop's transport input.
  const std::string& method() const { return method_; }
  const std::string& url() const { return url_; }
  const HeaderPairs& headers() const { return outgoing_; }
  const std::string& body() const { return body_; }
  uint32_t remaining() const { return remaining_; }
  bool finished() const { return finished_; }
  bool ok() const { return ok_; }
  HttpResult& result() { return result_; }

 private:
  using Clock = std::chrono::steady_clock;
  bool Prepare(HttpResult* local);
  void Fail(const char* message);
  void Finish(bool ok, HttpResult&& response);
  void Complete(HttpResult&& response);
  bool Record();
  Model& model_;
  const RedirectMode mode_;
  const HttpOptions options_;
  const std::string initialURL_;
  const Clock::time_point started_ = Clock::now();
  ruyijs::domain::browser_environment::UrlValue address_, source_;
  std::string method_, url_, body_, referrer_, policy_, redirectSite_;
  HeaderPairs headers_, outgoing_;
  HttpContext context_;
  std::vector<HttpTimingHop> timings_;
  HttpResult result_;
  uint64_t record_ = 0;
  uint32_t remaining_ = 0;
  unsigned redirects_ = 0;
  double hopStart_ = 0;
  double hopTick_ = 0;
  bool observe_ = false, credentialsTainted_ = false, credentials_ = false,
       crossOriginTainted_ = false;
  bool finished_ = false, ok_ = false;
};
}
#endif
