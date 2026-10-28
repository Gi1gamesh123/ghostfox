#pragma once
#include <string>
#include <string_view>

namespace ruyijs::domain::browser_environment {
enum class OriginKind { Opaque, Tuple };
// Classification and serialization of the original UrlValue origin algorithm.
// This read value owns no principal identity and does not change scheme policy.
struct OriginValue {
  OriginKind kind = OriginKind::Opaque;
  std::string tuple;
  std::string_view text() const noexcept {
    return kind == OriginKind::Tuple ? std::string_view(tuple) : std::string_view("null");
  }
};
}
