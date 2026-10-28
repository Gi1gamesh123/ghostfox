#pragma once
#include "resource_network/model_webtransport_wire.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <thread>

namespace ruyijs::resource_network {
struct WebTransportSession::Impl {
  struct Pending {uint64_t ticket;WebTransportCommand command;};
  std::mutex lock;
  std::condition_variable wake;
  std::deque<Pending> requests;
  std::vector<WebTransportEvent> events;
  size_t requestBytes=0,eventBytes=0;
  uint64_t next=1;
  std::atomic<bool> stopping{false};
  bool started=false,terminal=false,failed=false;
  std::string error;
  NeckoHelper helper;
  NeckoPipe input,output;
  std::thread worker;
  void Run(WebTransportOptions) noexcept;
  void Fail(const char*) noexcept;
  bool Publish(std::vector<WebTransportEvent>&);
};
}
