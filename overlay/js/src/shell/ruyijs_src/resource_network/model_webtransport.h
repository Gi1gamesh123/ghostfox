#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ruyijs::resource_network {
// Plain-native transport port. JS, stream queues and promises remain in the
// existing realm/Streams owners. One IO worker serializes the original Necko
// helper channel. Poll never blocks or calls JS on the IO thread.
struct WebTransportHash { std::string algorithm, bytes; };
struct WebTransportOptions {
  std::string url, origin;
  std::vector<WebTransportHash> hashes;
  std::vector<std::string> protocols;
  std::string congestion = "default";
  bool allowPooling = false, requireUnreliable = false;
};
enum class WebTransportAction {
  OpenStream, Read, Write, Finish, Reset, StopSending, Datagram, Stats,
  StreamStats, Group, Order, SetGroup, Export, Close
};
struct WebTransportCommand {
  WebTransportAction action = WebTransportAction::Stats;
  uint64_t stream = 0, group = 0;
  int64_t order = 0;
  bool bidirectional = false, hasGroup = false, hasOrder = true, sending = false;
  uint32_t code = 0, size = 65536;
  std::string data, label, context, reason;
};
struct WebTransportStatistic { std::string name; double value = 0; };
enum class WebTransportEventKind { Ready, Result, Stream, Datagram, Draining, Closed, Error, StreamReset, StopSending };
struct WebTransportEvent {
  WebTransportEventKind kind = WebTransportEventKind::Error;
  uint64_t ticket = 0, stream = 0, group = 0, consumed = 0;
  bool success = true, bidirectional = false, pending = false, fin = false, clean = false;
  uint32_t code = 0, maxDatagramSize = 1024;
  std::string data, reason, protocol;
  std::vector<WebTransportStatistic> statistics;
};
class WebTransportSession final {
 public:
  WebTransportSession();
  ~WebTransportSession();
  WebTransportSession(const WebTransportSession&) = delete;
  WebTransportSession& operator=(const WebTransportSession&) = delete;
  bool Begin(const WebTransportOptions&, std::string* error) noexcept;
  // Accepted commands receive a unique ticket; completion appears in Poll.
  bool Submit(const WebTransportCommand&, uint64_t* ticket, std::string* error) noexcept;
  bool Poll(std::vector<WebTransportEvent>*, std::string* error) noexcept;
  // Cancels IO and joins the sole native worker. No JS or callback during Stop.
  void Stop() noexcept;
 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
}
