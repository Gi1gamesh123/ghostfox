#include "transport/worker_session.h"
#include "transport/worker_json_internal.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <new>

namespace ruyijs::transport::worker {
namespace {
std::string Hex(const std::array<unsigned char, 16>& bytes) {
  std::string text;
  text.reserve(32);
  for (auto byte : bytes) { text += "0123456789abcdef"[byte >> 4]; text += "0123456789abcdef"[byte & 15]; }
  return text;
}
std::string_view Text(const Json::Value& value) noexcept {
  const char* begin = nullptr;
  const char* end = nullptr;
  return value.getString(&begin, &end) ? std::string_view(begin, end - begin) : std::string_view{};
}
}
JobStatus Session::Prefaces(uint64_t deadline) noexcept {
  Preface self;
  self.role = identity_.role == Sender::Parent ? 1 : 2;
  self.epoch = identity_.epoch; self.pid = pid_; self.nonce = identity_.nonce;
  PrefaceBytes encoded;
  if (!EncodePreface(self, &encoded)) return Fail({ERROR_INVALID_DATA, "PrefaceEncode"});
  for (NeckoChannel* channel : {&control_, &body_}) {
    auto result = channel->Write(encoded.data(), static_cast<uint32_t>(encoded.size()), deadline);
    if (!result) return Fail(result);
    PrefaceBytes received;
    result = channel->Read(received.data(), static_cast<uint32_t>(received.size()), deadline);
    if (!result) return Fail(result);
    Preface peer;
    if (!DecodePreface(std::string_view(received.data(), received.size()), &peer) ||
        peer.role != (self.role == 1 ? 2 : 1) || peer.epoch != self.epoch ||
        peer.pid != identity_.peer_pid || peer.nonce != self.nonce)
      return Fail({ERROR_INVALID_DATA, "PrefaceIdentity"});
  }
  return {};
}
JobStatus Session::Hello(uint64_t deadline) noexcept {
  try {
    const auto nonce = Hex(identity_.nonce);
    const std::string manifest(identity_.manifest.data(), identity_.manifest.size());
    const auto ownRole = identity_.role == Sender::Parent ? "parent" : "worker";
    const auto peerRole = identity_.role == Sender::Parent ? "worker" : "parent";
    std::string payload = "{\"role\":\"";
    payload.append(ownRole).append("\",\"pid\":").append(std::to_string(pid_));
    payload.append(",\"nonce\":\"").append(nonce).append("\",\"manifest\":\"").append(manifest);
    payload.append("\",\"qpcFrequency\":\"").append(std::to_string(frequency_)).append("\"}");
    auto result = WriteMessage("hello", payload, deadline);
    if (!result) return result;
    ControlDocument peer;
    result = ReadMessage(&peer, deadline);
    if (!result) return result;
    const auto& value = (*ControlAccess::Root(peer))["payload"];
    uint64_t frequency = 0;
    if (peer.Type() != "hello" || Text(value["role"]) != peerRole ||
        value["pid"].asUInt() != identity_.peer_pid || Text(value["nonce"]) != nonce ||
        Text(value["manifest"]) != manifest ||
        !ValidateU64(Text(value["qpcFrequency"]), &frequency) || frequency != frequency_)
      return Fail({ERROR_INVALID_DATA, "HelloIdentity"});
    return {};
  } catch (const std::bad_alloc&) { return Fail({ERROR_NOT_ENOUGH_MEMORY, "HelloAllocation"}); }
  catch (...) { return Fail({ERROR_INVALID_DATA, "HelloMessage"}); }
}
JobStatus Session::Handshake(const SessionIdentity& identity, uint64_t deadline) noexcept {
  if (started_.exchange(true)) return {ERROR_INVALID_STATE, "HandshakeAlreadyStarted"};
  if (failed_) return {ERROR_OPERATION_ABORTED, "SessionStopped"};
  if (!identity.epoch || !identity.peer_pid ||
      (identity.role != Sender::Parent && identity.role != Sender::Worker))
    return Fail({ERROR_INVALID_PARAMETER, "SessionIdentity"});
  for (auto c : identity.manifest)
    if (!(c >= '0' && c <= '9') && !(c >= 'a' && c <= 'f'))
      return Fail({ERROR_INVALID_PARAMETER, "SessionManifest"});
  LARGE_INTEGER frequency;
  if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0)
    return Fail({ERROR_GEN_FAILURE, "QueryPerformanceFrequency"});
  identity_ = identity;
  pid_ = GetCurrentProcessId(); frequency_ = static_cast<uint64_t>(frequency.QuadPart);
  auto result = Prefaces(deadline);
  if (!result) return result;
  result = Hello(deadline);
  if (!result) return result;
  if (failed_) return {ERROR_OPERATION_ABORTED, "SessionStopped"};
  ready_ = true;
  return {};
}
}
