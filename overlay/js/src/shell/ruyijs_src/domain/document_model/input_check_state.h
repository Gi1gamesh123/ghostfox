#pragma once
#include <optional>

namespace ruyijs::domain::document_model {
// Live checkedness is independent of the default attribute. A clean input
// starts from the attribute until a group operation supplies a live override.
struct InputCheckState {
  std::optional<bool> checked;
  bool dirty = false;
  bool indeterminate = false;
};
static_assert(sizeof(InputCheckState) == 4);
}
