#include "resource_network/necko_http.h"
#include "resource_network/necko_helper.h"
#include "resource_network/necko_wire.h"
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#ifdef _WIN32
#include "runtime/ghostfox_port.h"
namespace ruyijs { using ::GetTickCount64; }
#else
#include "runtime/ghostfox_port.h"
namespace ruyijs {
inline Ticks GetTickCount64() { return TicksNow(); }
using ULONGLONG = unsigned long long;
}
#include <fcntl.h>
#include <unistd.h>
#endif


namespace ruyijs::resource_network {
namespace {
// Each original helper session retains its native connection manager. Origin
// is idle-session affinity, not an origin-wide request lock. Broken framing
// ends that session; its next lease starts a new one.
struct Session {
  NeckoHelper worker;
  NeckoPipe input, output;
  bool Start() {
    ruyijs::resource_network::WorkerString command;
    if (!NeckoWorkerCommand(&command) || !input.Open(true) || !output.Open(false) ||
        !worker.Start(command, input, output, L"--session --timing-v2")) return false;
    input.CloseChild();
    output.CloseChild();
    return true;
  }
};
// A lease admits exactly one framed transaction per helper pipe.
struct Slot {
  std::unique_ptr<Session> session;
  std::string origin;
  uint64_t used = 0;
  bool busy = false;
};
// Firefox limits connections per server (network.http.max-persistent-
// connections-per-server = 6), not per process: one origin holds at most six
// helpers, while the pool admits the asynchronous transport's 16 active
// requests plus the JS thread's synchronous one, so a few requests that hang
// until their deadline do not starve every other origin.
constexpr size_t kSessions = 18;
constexpr size_t kPerOrigin = 6;
struct Pool {
  std::mutex lock;
  std::condition_variable available;
  std::vector<std::unique_ptr<Slot>> slots;
  uint64_t tick = 0;
};
// Process lifetime: transport threads can still be inside NeckoHttp when the
// process exits, so the pool is never destroyed by static destruction (that
// destroyed sessions under a running request and closed their handles twice).
// Helpers belong to a KILL_ON_JOB_CLOSE job and end when the process does.
Pool& Sessions() { static Pool* pool = new Pool; return *pool; }
struct Lease {
  Slot* slot = nullptr;
  ~Lease() {
    if (!slot) return;
    auto& pool = Sessions();
    { std::lock_guard<std::mutex> guard(pool.lock); slot->busy = false; }
    // Waiters differ by origin: wake them all, each rechecks its own limit.
    pool.available.notify_all();
  }
};
// scheme://host[:port] of an absolute URL, lower-cased, without userinfo.
std::string OriginOf(const std::string& url) {
  const size_t scheme = url.find("://");
  if (scheme == std::string::npos) return std::string();
  const size_t end = url.find_first_of("/?#", scheme + 3);
  std::string authority = url.substr(scheme + 3, end == std::string::npos ? end : end - scheme - 3);
  const size_t at = authority.rfind('@');
  if (at != std::string::npos) authority.erase(0, at + 1);
  std::string origin = url.substr(0, scheme + 3) + authority;
  for (char& c : origin) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
  return origin;
}
bool Acquire(const std::string& origin, ULONGLONG deadline, Lease* lease) {
  auto& pool = Sessions();
  std::unique_lock<std::mutex> guard(pool.lock);
  for (;;) {
    Slot* chosen = nullptr;
    size_t busy = 0;
    for (auto& slot : pool.slots)
      if (slot->busy && slot->origin == origin) ++busy;
    if (busy < kPerOrigin) {
      for (auto& slot : pool.slots)
        if (!slot->busy && slot->origin == origin) { chosen = slot.get(); break; }
      // A running helper serves any origin (its connection manager keeps one
      // pool per origin), so an idle one is preferred to starting a process.
      if (!chosen)
        for (auto& slot : pool.slots)
          if (!slot->busy && slot->session && (!chosen || slot->used < chosen->used)) chosen = slot.get();
      if (!chosen && pool.slots.size() < kSessions) {
        pool.slots.push_back(std::make_unique<Slot>());
        chosen = pool.slots.back().get();
      }
      if (!chosen)
        for (auto& slot : pool.slots)
          if (!slot->busy && (!chosen || slot->used < chosen->used)) chosen = slot.get();
    }
    if (chosen) {
      chosen->origin = origin;
      chosen->used = ++pool.tick;
      chosen->busy = true;
      lease->slot = chosen;
      return true;
    }
    const ULONGLONG now = GetTickCount64();
    if (now >= deadline) return false;
    pool.available.wait_for(guard, std::chrono::milliseconds(deadline - now));
  }
}
}
bool NeckoHttp(const std::string& method, const std::string& url, const HeaderPairs& headers,
               const std::string& body, HttpResult* result, uint32_t timeoutMs) {
  const ULONGLONG deadline = GetTickCount64() + timeoutMs;
  const std::string origin = OriginOf(url);
  // A helper can disappear between two requests (proxy tunnel reset, worker
  // restart, or a half-closed pipe).  In that case no response frame exists,
  // so retrying the same request on a fresh helper is safe and preserves the
  // original deadline.  Once a frame was read, HTTP status semantics belong to
  // the server and must be returned without transport-level retry.
  for (unsigned attempt = 0; attempt != 2; ++attempt) {
    Lease lease;
    if (!Acquire(origin, deadline, &lease)) {
      result->error = "Necko session capacity deadline exceeded";
      return false;
    }
    auto& session = lease.slot->session;
    if (!session) {
      session = std::make_unique<Session>();
      if (!session->Start()) {
        session.reset();
        result->error = "Necko worker startup failed";
        if (attempt == 0 && GetTickCount64() < deadline) continue;
        return false;
      }
    }
    HttpResult candidate;
    bool framed = false;
    const bool written = WriteNeckoRequest(session->input, method, url, headers, body, deadline);
    const bool read = written && ReadNeckoResponse(session->output, &candidate, deadline, &framed);
    if (read) {
      *result = std::move(candidate);
      return true;
    }
    if (!framed) session.reset();
    result->error = candidate.error.empty() ? "Necko request failed" : std::move(candidate.error);
    if (!framed && attempt == 0 && GetTickCount64() < deadline) continue;
    return false;
  }
  result->error = "Necko request failed";
  return false;
}
// Starts `count` helpers in the background so the first requests of a Context
// do not wait for the helper's interpreter; each becomes an idle slot.
void PrewarmNeckoSessions(size_t count) noexcept {
  for (size_t i = 0; i < count; ++i) {
    Slot* slot = nullptr;
    {
      auto& pool = Sessions();
      std::lock_guard<std::mutex> guard(pool.lock);
      if (pool.slots.size() >= kSessions) return;
      pool.slots.push_back(std::make_unique<Slot>());
      slot = pool.slots.back().get();
      slot->busy = true;  // leased by the warm-up until its helper runs
    }
    std::thread([slot] {
      auto session = std::make_unique<Session>();
      const bool started = session->Start();
      auto& pool = Sessions();
      { std::lock_guard<std::mutex> guard(pool.lock);
        if (started) slot->session = std::move(session);
        slot->busy = false; }
      pool.available.notify_all();
    }).detach();
  }
}
}
