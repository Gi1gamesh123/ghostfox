#ifndef RUYIJS_WEB_PLATFORM_OBJECT_URL_WIRE_H
#define RUYIJS_WEB_PLATFORM_OBJECT_URL_WIRE_H
#include <cstdint>
#include <string>
#include <string_view>

namespace ruyijs::web_platform::object_url_wire {
// Read: a Worker realm reads a URL of the origin's store (nested Worker scripts);
// its reply is 'O' + u32 type length + type + bytes, or one failure byte.
enum class Operation : uint8_t { Offer, Body, Revoke, Read };
enum class Reply : char { Ok = 'O', NeedBody = 'N', Limit = 'L', Failed = 'F' };
inline constexpr size_t kUrlLimit = 4096, kTypeLimit = 1024 * 1024;
inline constexpr size_t kBodyLimit = 8 * 1024 * 1024, kHeader = 21;
struct Request {
  Operation operation = Operation::Offer;
  uint32_t reference = 0;
  std::string_view url, type, bytes;
};
inline void Put(std::string& out, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) out.push_back(char(value >> (i * 8)));
}
inline uint32_t Get(std::string_view data, size_t at) {
  uint32_t value = 0;
  for (unsigned i = 0; i < 4; ++i)
    value |= uint32_t(static_cast<unsigned char>(data[at + i])) << (i * 8);
  return value;
}
inline bool Valid(const Request& value) {
  if (value.url.size() > kUrlLimit || value.type.size() > kTypeLimit ||
      value.bytes.size() > kBodyLimit) return false;
  switch (value.operation) {
    case Operation::Offer: return value.reference && value.type.empty() && value.bytes.empty();
    case Operation::Body: return value.reference != 0;
    case Operation::Revoke:
    case Operation::Read: return !value.reference && value.type.empty() && value.bytes.empty();
  }
  return false;
}
inline bool Encode(const Request& value, std::string* out) {
  if (!Valid(value)) return false;
  out->assign("OU1\0", 4);
  out->push_back(char(value.operation));
  Put(*out, value.reference); Put(*out, uint32_t(value.url.size()));
  Put(*out, uint32_t(value.type.size())); Put(*out, uint32_t(value.bytes.size()));
  out->append(value.url); out->append(value.type); out->append(value.bytes);
  return true;
}
inline bool Decode(std::string_view data, Request* out) {
  if (data.size() < kHeader || data.substr(0, 4) != std::string_view("OU1\0", 4)) return false;
  const size_t url = Get(data, 9), type = Get(data, 13), bytes = Get(data, 17);
  if (url > kUrlLimit || type > kTypeLimit || bytes > kBodyLimit ||
      data.size() != kHeader + url + type + bytes) return false;
  *out = {Operation(static_cast<uint8_t>(data[4])), Get(data, 5),
          data.substr(kHeader, url), data.substr(kHeader + url, type),
          data.substr(kHeader + url + type, bytes)};
  return Valid(*out);
}
}
#endif
