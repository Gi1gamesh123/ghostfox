#pragma once
#include <cstddef>

namespace ruyijs::transport::worker {
enum class Error { None, Length, Field, Utf8, Escape, Number, Quota, Json, Envelope, Memory };
struct Status {
  Error code = Error::None;
  size_t offset = 0;
  explicit operator bool() const noexcept { return code == Error::None; }
};
inline Status Failure(Error code, size_t offset = 0) noexcept { return {code, offset}; }
const char* ErrorName(Error code) noexcept;
}
