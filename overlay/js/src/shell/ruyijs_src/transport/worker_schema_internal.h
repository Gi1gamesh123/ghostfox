#pragma once
#include "transport/worker_schema.h"
#include "transport/worker_json_internal.h"
#include <initializer_list>

namespace ruyijs::transport::worker::schema {
using Value = Json::Value;
bool Fields(const Value&, std::initializer_list<const char*>);
std::string_view Text(const Value&) noexcept;
bool TextSize(const Value&, size_t low, size_t high) noexcept;
bool Number(const Value&, uint32_t low = 0, uint32_t high = UINT32_MAX);
bool Boolean(const Value&, bool expected);
bool OneOf(const Value&, std::initializer_list<std::string_view>) noexcept;
bool U64(const Value&, bool nonzero = true, bool sequence = false) noexcept;
bool Watermark(const Value&) noexcept;
bool Key(const Value&);
bool Token(const Value&, size_t limit) noexcept;
bool Headers(const Value&);
bool ErrorValue(const Value&, bool nullable = true);
bool Reason(const Value&) noexcept;
bool Tls(const Value&);
bool Hex(const Value&, size_t size, bool nonzero) noexcept;
bool Reserve(const Value&);
bool Handshake(std::string_view, const Value&, Sender);
bool Reservation(std::string_view, const Value&);
bool Request(std::string_view, const Value&);
bool Shutdown(std::string_view, const Value&);
}
