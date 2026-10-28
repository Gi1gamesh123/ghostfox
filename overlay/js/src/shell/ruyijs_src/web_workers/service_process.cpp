// Service worker processes (docs/owners/SERVICE_WORKER.md, ARCHITECTURE.md "Service
// Worker execution"): the same worker process and pipe as a dedicated Worker, started
// with the service global kind and without a `Worker` object. Frames other than the
// service channel go to the Service Worker owner's sink untouched.
#include "web_workers/realm.h"
#include "web_workers/state.h"
#include "web_workers/bootstrap.h"
#include "jsapi.h"

namespace ruyijs::web_workers {
bool Realm::StartService(const std::string& url, ServiceSink sink, void* context, uint64_t* id) {
  State& s = *state_;
  JSContext* cx = s.cx;
  *id = 0;
  if (!s.task || !s.environment) return WebKit::Fail(cx, "Worker ports are not installed");
  std::string environment, bootstrap;
  if (!s.environment(s.environmentContext, &environment)) return false;
  if (!EncodeStartup(url, std::string_view(), &bootstrap, std::u16string_view(), environment))
    return WebKit::Fail(cx, "Worker startup frame limit exceeded");
  const std::wstring executable = WorkerExecutable();
  if (executable.empty()) return WebKit::Fail(cx, "Worker executable is unavailable");
  if (s.budget->active >= Budget::kLimit) return WebKit::Fail(cx, "Active Worker limit exceeded");
  auto entry = std::make_unique<ServiceProcess>(&s, s.budget);
  entry->sink = sink;
  entry->context = context;
  entry->id = ++s.nextService;
  if (s.serviceFactory && !s.serviceFactory(s.serviceContext, &entry->record.service)) return false;
  if (!entry->record.process.Start(executable, std::move(bootstrap), WorkerKind::Service))
    return WebKit::Fail(cx, "Service worker could not be started");
  *id = entry->id;
  s.services.push_back(std::move(entry));
  return s.Schedule();
}
bool Realm::QueueService(uint64_t id, std::string frame) {
  for (auto& entry : state_->services)
    if (entry->id == id && !entry->record.terminated) return entry->record.process.Queue(std::move(frame));
  return false;
}
void Realm::StopService(uint64_t id) {
  auto& services = state_->services;
  for (size_t i = 0; i < services.size(); ++i)
    if (services[i]->id == id) { services.erase(services.begin() + i); return; }
}

// One frame of one service process per poll task. The service channel (object
// URLs, caches, fetches) is answered here exactly as for a dedicated Worker; an
// exited process reports one empty frame to its sink.
bool PollServices(State& s, bool* delivered) {
  *delivered = false;
  JSContext* cx = s.cx;
  for (size_t i = 0; i < s.services.size(); ++i) {
    ServiceProcess& entry = *s.services[(s.serviceCursor + i) % s.services.size()];
    if (entry.record.terminated) continue;
    std::string frame;
    bool received = false;
    const auto status = entry.record.process.Poll(&frame, &received);
    if (!received) {
      if (status != PipeResult::Closed && status != PipeResult::Failed) continue;
      entry.record.Stop();
      frame.clear();
    }
    s.serviceCursor = (s.serviceCursor + i + 1) % s.services.size();
    *delivered = true;
    if (!frame.empty() && frame[0] == kServiceRequest) {
      std::string reply;
      if (!entry.record.service.owner || !entry.record.service.request ||
          !entry.record.service.request(entry.record.service.owner.get(), std::string_view(frame).substr(1), &reply)) {
        entry.record.Stop();
        if (!JS_IsExceptionPending(cx)) WebKit::Fail(cx, "Worker service failed");
        return false;
      }
      reply.insert(reply.begin(), kServiceReply);
      return entry.record.process.Queue(std::move(reply));
    }
    if (!frame.empty() && frame[0] == kBroadcastFrame) return RouteBroadcast(s, frame, &entry.record);
    const uint64_t id = entry.id;
    return entry.sink(entry.context, id, frame);
  }
  return true;
}
}
