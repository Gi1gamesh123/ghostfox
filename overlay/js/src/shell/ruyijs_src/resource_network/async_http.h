#ifndef RUYIJS_ASYNC_HTTP_H
#define RUYIJS_ASYNC_HTTP_H
#include "resource_network/page_http.h"
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>

namespace ruyijs::resource_network {
struct AsyncHttpSource;
struct AsyncHttpLifetime;
struct AsyncHttpTicket { uint64_t id = 0; };
// Borrowed arguments for admission; the original owner copies them before return.
struct NativeHttpRequest {
  const std::string& method;
  const std::string& url;
  const HeaderPairs& headers;
  const std::string& body;
  RedirectMode redirect;
  const HttpOptions& options;
};
enum class AsyncHttpOutcome { Http, OutOfMemory, LaunchFailure };
struct AsyncHttpResult {
  AsyncHttpOutcome outcome = AsyncHttpOutcome::Http;
  bool ok = false;
  HttpResult response;
};
struct AsyncHttpCompletion {
  std::shared_ptr<void> target;
  void (*finish)(void*, AsyncHttpResult&&) noexcept = nullptr;
};
void DetachAsyncHttpSource(const std::shared_ptr<AsyncHttpSource>&) noexcept;

// One Context owner: transport threads use native values only, all Model and
// completion work runs on its owning thread. Even a local result runs later.
class AsyncHttp final {
 public:
  using Done = std::function<void(bool ok, HttpResult&& result)>;
  explicit AsyncHttp(Model& model);
  ~AsyncHttp();
  AsyncHttp(const AsyncHttp&) = delete;
  AsyncHttp& operator=(const AsyncHttp&) = delete;
  Status OpenSource(Model&, std::shared_ptr<AsyncHttpSource>*) noexcept;
  Status Start(const std::shared_ptr<AsyncHttpSource>&, const NativeHttpRequest&,
               const AsyncHttpCompletion&, AsyncHttpTicket*) noexcept;
  void Cancel(const std::shared_ptr<AsyncHttpSource>&,AsyncHttpTicket) noexcept;
  // Original image entry and successful scheduling contract remain unchanged.
  void Start(const std::string& method, const std::string& url, const HeaderPairs& headers,
             const std::string& body, RedirectMode mode, const HttpOptions& options, Done done);
  void DetachSource(AsyncHttpSource&) noexcept;
  bool Pending() const { return !jobs_.empty() || active_ != 0; }
  bool Wait(uint32_t ms);
  bool Run();

 private:
  struct Job;
  struct Inbox;
  struct Outcome;
  Status Admit(const std::shared_ptr<AsyncHttpSource>&, const NativeHttpRequest&,
               const AsyncHttpCompletion&, Done, AsyncHttpTicket*) noexcept;
  void Advance(uint64_t id) noexcept;
  void Launch(uint64_t id) noexcept;
  void PumpQueue() noexcept;
  void Fail(Job&, AsyncHttpOutcome) noexcept;
  std::shared_ptr<AsyncHttpLifetime> lifetime_;
  std::shared_ptr<AsyncHttpSource> root_;
  std::shared_ptr<Inbox> inbox_;
  std::map<uint64_t, std::unique_ptr<Job>> jobs_;
  std::deque<uint64_t> queued_;
  uint32_t active_ = 0;
  uint64_t next_ = 1;
};
Status StartAsyncHttp(const std::shared_ptr<AsyncHttpSource>&,const NativeHttpRequest&,
                      const AsyncHttpCompletion&,AsyncHttpTicket*) noexcept;
void CancelAsyncHttp(const std::shared_ptr<AsyncHttpSource>&,AsyncHttpTicket) noexcept;
}
#endif
