// A Worker realm's network surface: the same Headers/Response/Request
// installers as the Window realm and the one fetch factory on
// WorkerGlobalScope.prototype. The Worker process keeps no catalog, NetLog or
// Cookie jar; its model's remote port hands each whole request to the parent
// Context's Resource Network owner over the Worker service channel
// (worker_network_wire.h), and the request conversion resolves against the
// parent browsing context's HttpContext, read over the same channel the first
// time a request needs it, so a Worker that never fetches never uses it.
#include "resource_network/state.h"
#include "resource_network/cache.h"
#include "resource_network/webtransport.h"
#include "resource_network/worker_network_wire.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
namespace wire = worker_network_wire;
namespace {
struct WorkerLink {
  JSContext* cx = nullptr;
  Realm::WorkerExchange exchange = nullptr;
  void* context = nullptr;
  bool known = false;
  HttpContext http;
};
bool Context(void* opaque, HttpContext* value) {
  auto& link = *static_cast<WorkerLink*>(opaque);
  if (!link.known) {
    std::string reply;
    HttpContext read;
    if (!link.exchange(link.context, std::string(1, wire::kContext), &reply)) return false;
    if (!wire::DecodeContext(reply, &read)) return Error(link.cx, "Worker network context is unavailable");
    link.http = std::move(read);
    link.known = true;
  }
  *value = link.http;
  return true;
}
bool Remote(void* opaque, const Model::RemoteRequest& request, HttpResult* result, bool* ok) {
  auto& link = *static_cast<WorkerLink*>(opaque);
  wire::Request outgoing{request.method, request.url, request.body, request.headers,
                         request.redirect, request.options};
  std::string frame, reply;
  if (!wire::EncodeRequest(outgoing, &frame)) return false;
  // A closed channel is a network error to the caller, as a transport failure is.
  if (!link.exchange(link.context, frame, &reply)) { JS_ClearPendingException(link.cx); return false; }
  return wire::DecodeResult(reply, ok, result);
}
}
bool Realm::InstallWorkerNetwork(PageHelpers& helpers, JS::HandleObject globalScopePrototype,
                                 WorkerExchange exchange, void* context) {
  if (!state_ || !exchange || !globalScopePrototype) return false;
  State& s = *state_;
  JSAutoRealm realm(s.cx, s.page);
  auto link = std::make_shared<WorkerLink>();
  link->cx = s.cx;
  link->exchange = exchange;
  link->context = context;
  s.workerScope = true;
  s.cacheExchange=exchange;s.cacheExchangeContext=context;
  s.workerExchange=exchange;s.workerExchangeContext=context;
  s.resources.SetHttpContext(nullptr, Context, link.get());
  s.resources.SetRemoteHttp(Remote, link.get());
  s.workerLink = std::move(link);
  if (!Report(s.cx, NewArena(&s.arena))) return false;
  return InstallHeaders(s, helpers) && InstallResponse(s, helpers) && InstallRequest(s, helpers) &&
         InstallCache(s, helpers, globalScopePrototype) &&
         InstallWebTransport(s, helpers) &&
         InstallFetch(s, helpers, globalScopePrototype, "WorkerGlobalScope") && ConfigureExposure();
}
bool Realm::FetchWorkerScript(std::string_view base, std::string_view input, std::string* url,
                              std::string* source, bool* ok) {
  *ok = false;
  if (!state_ || !state_->workerScope) return false;
  if (!ResolveURL(base, input, url)) return true;
  HttpResult result;
  if (!PerformHttp(state_->resources, "GET", *url, {{"Accept", "*/*"}}, std::string(), &result)) {
    JS_ClearPendingException(state_->cx);
    return true;
  }
  *ok = result.status >= 200 && result.status < 300;
  if (!result.finalURL.empty()) *url = result.finalURL;
  *source = std::move(result.body);
  return true;
}
bool Realm::InstallWorkerTransports(PageHelpers& helpers, bool xhr) {
  if (!state_ || !state_->workerScope) return false;
  JSAutoRealm realm(state_->cx, state_->page);
  // XMLHttpRequest is not exposed in a ServiceWorkerGlobalScope (Firefox).
  return (!xhr || InstallXhr(*state_, helpers)) && InstallWebSocket(*state_, helpers) &&
         InstallEventSource(*state_);
}
}
