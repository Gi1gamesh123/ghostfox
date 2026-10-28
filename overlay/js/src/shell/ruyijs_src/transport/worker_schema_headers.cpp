#include "transport/worker_schema_internal.h"

namespace ruyijs::transport::worker::schema {
namespace {
int Sextet(char c) noexcept {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  return c == '/' ? 63 : -1;
}
bool Byte(unsigned byte) noexcept { return byte == 9 || (byte >= 32 && byte != 127); }
bool HeaderBytes(const Value& value, size_t* length) noexcept {
  if (!value.isString()) return false;
  auto text = Text(value);
  if (text.size() % 4 || text.size() > 87384) return false;
  size_t size = 0;
  for (size_t i = 0; i < text.size(); i += 4) {
    int a = Sextet(text[i]), b = Sextet(text[i + 1]);
    if (a < 0 || b < 0 || !Byte((a << 2) | (b >> 4))) return false;
    ++size;
    if (text[i + 2] == '=') {
      if (text[i + 3] != '=' || i + 4 != text.size() || (b & 15)) return false;
      continue;
    }
    int c = Sextet(text[i + 2]);
    if (c < 0 || !Byte(((b & 15) << 4) | (c >> 2))) return false;
    ++size;
    if (text[i + 3] == '=') {
      if (i + 4 != text.size() || (c & 3)) return false;
      continue;
    }
    int d = Sextet(text[i + 3]);
    if (d < 0 || !Byte(((c & 3) << 6) | d)) return false;
    ++size;
  }
  *length = size;
  return true;
}
}
bool Headers(const Value& value) {
  if (!value.isArray() || value.size() > 256) return false;
  size_t total = 0;
  for (const auto& header : value) {
    if (!Fields(header, {"name", "valueBase64"}) || !Token(header["name"], 65536)) return false;
    size_t size = 0;
    if (!HeaderBytes(header["valueBase64"], &size)) return false;
    size += Text(header["name"]).size() + 4;
    if (size > 65536 - total) return false;
    total += size;
  }
  return true;
}
}
