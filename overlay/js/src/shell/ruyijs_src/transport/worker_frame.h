#pragma once
#include "transport/worker_status.h"
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace ruyijs::transport::worker {
constexpr size_t kPrefaceBytes = 40, kBodyHeaderBytes = 48, kBodyPayloadLimit = 65536;
struct Preface {
  uint16_t role = 0;
  uint64_t epoch = 0;
  uint32_t pid = 0;
  std::array<unsigned char, 16> nonce{};
};
struct BodyHeader {
  uint16_t type = 0;
  uint64_t epoch = 0, requestId = 0;
  uint32_t generation = 0, hop = 0;
  uint64_t seq = 0;
  uint32_t payloadLength = 0, flags = 0;
};
struct BodyView { BodyHeader header; std::string_view payload; };
using PrefaceBytes = std::array<char, kPrefaceBytes>;
using HeaderBytes = std::array<char, kBodyHeaderBytes>;
Status DecodePreface(std::string_view bytes, Preface* result) noexcept;
Status EncodePreface(const Preface& value, PrefaceBytes* result) noexcept;
Status DecodeBodyHeader(uint32_t totalLength, std::string_view bytes, BodyHeader* result) noexcept;
Status EncodeBodyHeader(const BodyHeader& value, HeaderBytes* result) noexcept;
Status DecodeBodyFrame(std::string_view bytes, BodyView* result) noexcept;
Status EncodeBodyFrame(const BodyHeader& header, std::string_view payload, std::string* result) noexcept;
uint32_t ReadLength(const char* bytes) noexcept;
void WriteLength(uint32_t value, char* bytes) noexcept;
}
