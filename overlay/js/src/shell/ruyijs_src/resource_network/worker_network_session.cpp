// The parent side of a Worker's fetch: one session per Worker, opened by the
// runtime's composed service factory, served from the Worker poll task. It
// runs the request through the same PerformHttp against this browsing
// context's one model, so Cookie, NetLog, redirects and the Necko transport
// keep their single owner (worker_network_wire.h).
#include "resource_network/state.h"
#include "resource_network/worker_network_wire.h"
#include "js/Realm.h"

namespace ruyijs::resource_network {
namespace wire = worker_network_wire;
bool Realm::WorkerNetworkRequest(std::string_view frame, std::string* reply) {
  if (!state_) return false;
  State& s = *state_;
  JSAutoRealm realm(s.cx, s.page);
  if (frame.size() == 1 && frame[0] == wire::kContext) {
    HttpContext context;
    if (!s.resources.ReadHttpContext(&context) || !wire::EncodeContext(context, reply))
      return Error(s.cx, "Worker network context is unavailable");
    return true;
  }
  wire::Request request;
  if (frame.empty() || frame[0] != wire::kRequest || !wire::DecodeRequest(frame.substr(1), &request))
    return Error(s.cx, "Invalid Worker network frame");
  HttpResult result;
  const bool ok = PerformHttp(s.resources, request.method, request.url, request.headers,
                              request.body, &result, request.redirect, request.options);
  if (!wire::EncodeResult(ok, result, reply)) {
    HttpResult failure;
    failure.error = "Worker network response exceeds the channel limits";
    return wire::EncodeResult(false, failure, reply) || Error(s.cx, failure.error.c_str());
  }
  return true;
}
}
