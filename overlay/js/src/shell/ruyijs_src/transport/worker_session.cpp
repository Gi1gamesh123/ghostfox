#include "transport/worker_session.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <new>
#include <utility>

namespace ruyijs::transport::worker {
namespace {
JobStatus Codec(Status status, const char* operation) noexcept {
  return {static_cast<uint32_t>(status.code == Error::Memory ? ERROR_NOT_ENOUGH_MEMORY : ERROR_INVALID_DATA), operation};
}
struct Reset { std::atomic<bool>& flag; ~Reset() { flag = false; } };
bool TypeToken(std::string_view type) noexcept {
  if (type.empty() || type.size() > 32) return false;
  for (auto c : type)
    if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') && c != '-') return false;
  return true;
}
}
void Session::Stop() noexcept { failed_ = true; control_.Stop(); body_.Stop(); }
JobStatus Session::Fail(JobStatus status) noexcept { Stop(); return status; }
JobStatus Session::WriteMessage(std::string_view type, std::string_view payload, uint64_t deadline) noexcept {
  if (failed_) return {ERROR_OPERATION_ABORTED, "SessionStopped"};
  if (send_seq_ == UINT64_MAX || !TypeToken(type) || payload.size() > kControlLimit)
    return Fail({ERROR_INVALID_DATA, "SendEnvelope"});
  try {
    std::string json = "{\"version\":1,\"type\":\"";
    json.append(type).append("\",\"epoch\":\"").append(std::to_string(identity_.epoch));
    json.append("\",\"channelSeq\":\"").append(std::to_string(send_seq_));
    json.append("\",\"payload\":").append(payload).append("}");
    ControlDocument document;
    auto code = ParseControlJson(json, &document);
    if (!code) return Fail(Codec(code, "SendJson"));
    code = ValidatePayload(document, identity_.role);
    if (!code) return Fail(Codec(code, "SendPayload"));
    std::string frame;
    code = EncodeControlFrame(document, &frame);
    if (!code) return Fail(Codec(code, "SendEncoding"));
    const auto result = control_.Write(frame.data(), static_cast<uint32_t>(frame.size()), deadline);
    if (!result) return Fail(result);
    ++send_seq_;
    return {};
  } catch (const std::bad_alloc&) { return Fail({ERROR_NOT_ENOUGH_MEMORY, "SendAllocation"}); }
  catch (...) { return Fail({ERROR_INVALID_DATA, "SendMessage"}); }
}
JobStatus Session::ReadMessage(ControlDocument* output, uint64_t deadline, uint32_t stall_ms) noexcept {
  if (failed_) return {ERROR_OPERATION_ABORTED, "SessionStopped"};
  if (!output || receive_seq_ == UINT64_MAX) return Fail({ERROR_INVALID_DATA, "ReceiveEnvelope"});
  try {
    char prefix[4];
    auto result = stall_ms ? control_.ReadStart(prefix, sizeof(prefix), stall_ms, &deadline) :
                            control_.Read(prefix, sizeof(prefix), deadline);
    if (!result) return Fail(result);
    const auto length = ReadLength(prefix);
    if (!length || length > kControlLimit) return Fail({ERROR_INVALID_DATA, "ReceiveLength"});
    std::string json(length, '\0');
    result = control_.Read(json.data(), length, deadline);
    if (!result) return Fail(result);
    ControlDocument next;
    auto code = ParseControlJson(json, &next);
    if (!code) return Fail(Codec(code, "ReceiveJson"));
    const Sender peer = identity_.role == Sender::Parent ? Sender::Worker : Sender::Parent;
    code = ValidatePayload(next, peer);
    if (!code) return Fail(Codec(code, "ReceivePayload"));
    if (next.Epoch() != identity_.epoch || next.ChannelSequence() != receive_seq_ ||
        (next.Type() == "hello") != (receive_seq_ == 0))
      return Fail({ERROR_INVALID_DATA, "ReceiveContinuity"});
    *output = std::move(next);
    ++receive_seq_;
    return {};
  } catch (const std::bad_alloc&) { return Fail({ERROR_NOT_ENOUGH_MEMORY, "ReceiveAllocation"}); }
  catch (...) { return Fail({ERROR_INVALID_DATA, "ReceiveMessage"}); }
}
JobStatus Session::Send(std::string_view type, std::string_view payload, uint64_t deadline) noexcept {
  if (!valid()) return {ERROR_INVALID_STATE, "SessionNotReady"};
  bool expected = false;
  if (!writing_.compare_exchange_strong(expected, true)) return {ERROR_BUSY, "SessionWriter"};
  Reset reset{writing_};
  if (type == "hello") return Fail({ERROR_INVALID_DATA, "DuplicateHello"});
  return WriteMessage(type, payload, deadline);
}
JobStatus Session::Receive(ControlDocument* output, uint64_t deadline) noexcept {
  if (!valid()) return {ERROR_INVALID_STATE, "SessionNotReady"};
  bool expected = false;
  if (!reading_.compare_exchange_strong(expected, true)) return {ERROR_BUSY, "SessionReader"};
  Reset reset{reading_};
  return ReadMessage(output, deadline);
}
JobStatus Session::ReceiveNext(ControlDocument* output, uint32_t stall_ms) noexcept {
  if (!output || !stall_ms || stall_ms > 60000) return {ERROR_INVALID_PARAMETER, "ReceiveNext"};
  if (!valid()) return {ERROR_INVALID_STATE, "SessionNotReady"};
  bool expected = false;
  if (!reading_.compare_exchange_strong(expected, true)) return {ERROR_BUSY, "SessionReader"};
  Reset reset{reading_};
  return ReadMessage(output, 0, stall_ms);
}
}
