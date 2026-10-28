#include "document_model/model_internal.h"
#include "domain/document_model/input_radio_group.h"

namespace ruyijs::document_model {
Status Model::InputChecked(NodeKey key, bool* value, bool indeterminate) const noexcept {
  return Protect([&] {
    const auto& e = State().element(key);
    *value = indeterminate ? e.input_check_state().indeterminate : e.input_checked();
  });
}
Status Model::SetInputChecked(NodeKey key, bool value, bool indeterminate) noexcept {
  return Protect([&] {
    if (indeterminate) State().element(key).input_check_state().indeterminate = value;
    else domain::input_set_checked(State().owner(key), key.node, value);
  });
}
}
