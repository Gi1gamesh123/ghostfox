#include "resource_network/async_http_internal.h"
#include <utility>

namespace ruyijs::resource_network {
bool AsyncHttp::Run() {
  for (;;) {
    auto outcome = inbox_->Pop();
    if (!outcome) return false;
    // Capacity belongs to its native transport outcome, even after source detach.
    if (outcome->transport) { --active_; PumpQueue(); }
    auto found = jobs_.find(outcome->id);
    if (found == jobs_.end()) continue;
    Job& job = *found->second;
    if (outcome->transport && outcome->result.outcome == AsyncHttpOutcome::Http) {
      try {
        job.exchange.Deliver(outcome->result.ok, std::move(outcome->result.response));
        Advance(outcome->id);
      } catch (...) { Fail(job, AsyncHttpOutcome::OutOfMemory); }
      continue;
    }
    std::unique_ptr<Job> finished = std::move(found->second);
    jobs_.erase(found);
    auto result = std::move(outcome->result);
    if (result.outcome == AsyncHttpOutcome::Http) {
      result.ok = finished->exchange.ok();
      result.response = std::move(finished->exchange.result());
    }
    // Job identity leaves before callback reentry; target lifetime is local.
    if (finished->completion.finish)
      finished->completion.finish(finished->completion.target.get(), std::move(result));
    else finished->legacy(result.ok, std::move(result.response));
    return true;
  }
}
}
