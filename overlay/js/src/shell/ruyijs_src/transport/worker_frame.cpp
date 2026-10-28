#include "transport/worker_frame.h"
#include <algorithm>
#include <limits>
#include <new>

namespace ruyijs::transport::worker {
namespace {
uint64_t Read(const char* p, size_t size) noexcept {
  uint64_t value = 0;
  for (size_t i = 0; i < size; ++i) value |= uint64_t(static_cast<unsigned char>(p[i])) << (8 * i);
  return value;
}
void Write(uint64_t value, char* p, size_t size) noexcept {
  for (size_t i = 0; i < size; ++i) p[i] = static_cast<char>(value >> (8 * i));
}
bool Valid(const Preface& p) noexcept {
  return (p.role == 1 || p.role == 2) && p.epoch && p.pid &&
      std::any_of(p.nonce.begin(), p.nonce.end(), [](unsigned char c) { return c != 0; });
}
bool Valid(const BodyHeader& h) noexcept {
  return (h.type == 1 || h.type == 2) && h.epoch && h.requestId && h.generation &&
      h.hop < 20 && h.seq != UINT64_MAX && h.payloadLength >= 1 &&
      h.payloadLength <= kBodyPayloadLimit && h.flags == 0;
}
}
uint32_t ReadLength(const char* bytes) noexcept { return static_cast<uint32_t>(Read(bytes, 4)); }
void WriteLength(uint32_t value, char* bytes) noexcept { Write(value, bytes, 4); }
const char* ErrorName(Error code) noexcept {
  switch (code) {
    case Error::None: return "none";
    case Error::Length: return "length";
    case Error::Field: return "field";
    case Error::Utf8: return "utf8";
    case Error::Escape: return "escape";
    case Error::Number: return "number";
    case Error::Quota: return "quota";
    case Error::Json: return "json";
    case Error::Envelope: return "envelope";
    case Error::Memory: return "memory";
  }
  return "unknown";
}
Status DecodePreface(std::string_view bytes, Preface* result) noexcept {
  if (!result || bytes.size() != kPrefaceBytes) return Failure(Error::Length);
  const char* p = bytes.data();
  if (Read(p, 4) != 0x48575952 || Read(p + 4, 2) != 1 || Read(p + 20, 4))
    return Failure(Error::Field);
  Preface next;
  next.role = static_cast<uint16_t>(Read(p + 6, 2));
  next.epoch = Read(p + 8, 8);
  next.pid = static_cast<uint32_t>(Read(p + 16, 4));
  for (size_t i = 0; i < 16; ++i) next.nonce[i] = static_cast<unsigned char>(p[24 + i]);
  if (!Valid(next)) return Failure(Error::Field);
  *result = next;
  return {};
}
Status EncodePreface(const Preface& p, PrefaceBytes* result) noexcept {
  if (!result || !Valid(p)) return Failure(Error::Field);
  PrefaceBytes next{};
  Write(0x48575952, next.data(), 4); Write(1, next.data() + 4, 2);
  Write(p.role, next.data() + 6, 2); Write(p.epoch, next.data() + 8, 8);
  Write(p.pid, next.data() + 16, 4);
  for (size_t i = 0; i < 16; ++i) next[24 + i] = static_cast<char>(p.nonce[i]);
  *result = next;
  return {};
}
Status DecodeBodyHeader(uint32_t length, std::string_view bytes, BodyHeader* result) noexcept {
  if (!result || bytes.size() != kBodyHeaderBytes || length < 49 || length > 65584)
    return Failure(Error::Length);
  const char* p = bytes.data();
  if (Read(p, 4) != 0x42575952 || Read(p + 4, 2) != 1) return Failure(Error::Field);
  BodyHeader next;
  next.type = static_cast<uint16_t>(Read(p + 6, 2));
  next.epoch = Read(p + 8, 8); next.requestId = Read(p + 16, 8);
  next.generation = static_cast<uint32_t>(Read(p + 24, 4));
  next.hop = static_cast<uint32_t>(Read(p + 28, 4)); next.seq = Read(p + 32, 8);
  next.payloadLength = static_cast<uint32_t>(Read(p + 40, 4));
  next.flags = static_cast<uint32_t>(Read(p + 44, 4));
  if (!Valid(next)) return Failure(Error::Field);
  if (length != kBodyHeaderBytes + next.payloadLength) return Failure(Error::Length);
  *result = next;
  return {};
}
Status EncodeBodyHeader(const BodyHeader& h, HeaderBytes* result) noexcept {
  if (!result || !Valid(h)) return Failure(Error::Field);
  HeaderBytes next{};
  Write(0x42575952, next.data(), 4); Write(1, next.data() + 4, 2);
  Write(h.type, next.data() + 6, 2); Write(h.epoch, next.data() + 8, 8);
  Write(h.requestId, next.data() + 16, 8); Write(h.generation, next.data() + 24, 4);
  Write(h.hop, next.data() + 28, 4); Write(h.seq, next.data() + 32, 8);
  Write(h.payloadLength, next.data() + 40, 4); Write(h.flags, next.data() + 44, 4);
  *result = next;
  return {};
}
Status DecodeBodyFrame(std::string_view bytes, BodyView* result) noexcept {
  if (!result || bytes.size() < 4 + kBodyHeaderBytes) return Failure(Error::Length);
  const uint32_t length = ReadLength(bytes.data());
  if (bytes.size() - 4 != length) return Failure(Error::Length);
  BodyView next;
  Status status = DecodeBodyHeader(length, bytes.substr(4, kBodyHeaderBytes), &next.header);
  if (!status) return status;
  next.payload = bytes.substr(4 + kBodyHeaderBytes);
  *result = next;
  return {};
}
Status EncodeBodyFrame(const BodyHeader& h, std::string_view payload, std::string* result) noexcept {
  if (!result || payload.size() != h.payloadLength) return Failure(Error::Length);
  HeaderBytes header;
  Status status = EncodeBodyHeader(h, &header);
  if (!status) return status;
  try {
    std::string next(4 + kBodyHeaderBytes + payload.size(), '\0');
    WriteLength(static_cast<uint32_t>(kBodyHeaderBytes + payload.size()), next.data());
    std::copy(header.begin(), header.end(), next.begin() + 4);
    std::copy(payload.begin(), payload.end(), next.begin() + 4 + kBodyHeaderBytes);
    result->swap(next);
    return {};
  } catch (...) { return Failure(Error::Memory); }
}
}
