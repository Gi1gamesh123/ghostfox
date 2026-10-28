#include "transport/worker_schema_internal.h"

namespace ruyijs::transport::worker::schema {
bool Fields(const Value& value, std::initializer_list<const char*> names) {
  if (!value.isObject() || value.size() != names.size()) return false;
  for (const char* name : names) if (!value.isMember(name)) return false;
  return true;
}
std::string_view Text(const Value& value) noexcept {
  const char* begin = nullptr;
  const char* end = nullptr;
  return value.getString(&begin, &end) ? std::string_view(begin, end - begin) : std::string_view{};
}
bool TextSize(const Value& value, size_t low, size_t high) noexcept {
  if (!value.isString()) return false;
  size_t size = Text(value).size();
  return size >= low && size <= high;
}
bool Number(const Value& value, uint32_t low, uint32_t high) {
  if (value.type() != Json::intValue && value.type() != Json::uintValue) return false;
  auto number = value.asUInt64();
  return number >= low && number <= high;
}
bool Boolean(const Value& value, bool expected) {
  return value.isBool() && value.asBool() == expected;
}
bool OneOf(const Value& value, std::initializer_list<std::string_view> choices) noexcept {
  if (!value.isString()) return false;
  for (auto choice : choices) if (Text(value) == choice) return true;
  return false;
}
bool U64(const Value& value, bool nonzero, bool sequence) noexcept {
  uint64_t parsed = 0;
  return value.isString() && ValidateU64(Text(value), &parsed) &&
      (!nonzero || parsed != 0) && (!sequence || parsed != UINT64_MAX);
}
bool Watermark(const Value& value) noexcept { return value.isNull() || U64(value, false, true); }
bool Key(const Value& value) {
  return Fields(value, {"requestId", "generation", "hop"}) && U64(value["requestId"]) &&
      Number(value["generation"], 1) && Number(value["hop"], 0, 19);
}
bool Token(const Value& value, size_t limit) noexcept {
  if (!TextSize(value, 1, limit)) return false;
  constexpr std::string_view extra = "!#$%&'*+-.^_`|~";
  for (unsigned char c : Text(value)) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) continue;
    if (extra.find(static_cast<char>(c)) == std::string_view::npos) return false;
  }
  return true;
}
bool Reason(const Value& value) noexcept {
  return OneOf(value, {"cancelled", "deadline", "admission", "closing", "failure"});
}
bool Hex(const Value& value, size_t size, bool nonzero) noexcept {
  if (!TextSize(value, size, size)) return false;
  bool hasNonzero = false;
  for (char c : Text(value)) {
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    hasNonzero |= c != '0';
  }
  return !nonzero || hasNonzero;
}
bool ErrorValue(const Value& value, bool nullable) {
  if (value.isNull()) return nullable;
  if (!Fields(value, {"kind", "phase", "code", "message", "attempt"}) ||
      !OneOf(value["kind"], {"Cancelled", "DeadlineExceeded", "DnsError", "ConnectError",
          "TlsError", "ProtocolError", "BodyLimit", "DecodeError", "WorkerExited", "AdmissionError"}) ||
      !TextSize(value["phase"], 1, 64) || !Number(value["code"]) ||
      !TextSize(value["message"], 0, 512) || !Number(value["attempt"], 0, 3)) return false;
  for (char c : Text(value["phase"]))
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
  return true;
}
bool Tls(const Value& value) {
  if (value.isNull()) return true;
  if (!Fields(value, {"backend", "alpn", "version"}) || !OneOf(value["backend"], {"nss"}) ||
      !OneOf(value["alpn"], {"http/1.1", "h2", "h3"}) || !TextSize(value["version"], 1, 32)) return false;
  for (unsigned char c : Text(value["version"])) if (c < 0x21 || c > 0x7e) return false;
  return true;
}
}
