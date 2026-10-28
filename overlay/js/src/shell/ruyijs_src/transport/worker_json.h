#pragma once
#include "transport/worker_status.h"
#include <cstdint>
#include <string>
#include <string_view>

namespace ruyijs::transport::worker {
constexpr size_t kControlLimit = 262144, kJsonNodeLimit = 4096, kJsonDepthLimit = 8;
class ControlDocument final {
 public:
  ControlDocument() noexcept = default;
  ~ControlDocument() noexcept;
  ControlDocument(ControlDocument&& other) noexcept;
  ControlDocument& operator=(ControlDocument&& other) noexcept;
  ControlDocument(const ControlDocument&) = delete;
  ControlDocument& operator=(const ControlDocument&) = delete;
  bool Valid() const noexcept { return impl_ != nullptr; }
  uint64_t Epoch() const noexcept;
  uint64_t ChannelSequence() const noexcept;
  std::string_view Type() const noexcept;
 private:
  struct Impl;
  Impl* impl_ = nullptr;
  friend struct ControlAccess;
  friend Status ParseControlJson(std::string_view, ControlDocument*) noexcept;
  friend Status EncodeControlFrame(const ControlDocument&, std::string*) noexcept;
};
Status ValidateU64(std::string_view text, uint64_t* result) noexcept;
Status ParseControlJson(std::string_view json, ControlDocument* result) noexcept;
Status DecodeControlFrame(std::string_view frame, ControlDocument* result) noexcept;
Status EncodeControlFrame(const ControlDocument& document, std::string* result) noexcept;
}
