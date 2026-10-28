// Reset input state through the original form, event and Element owners.
#include "document_model/form_submission.h"
#include "document_model/html_control.h"
#include "interaction/realm.h"
#include "js/Realm.h"
#include <cstring>

namespace ruyijs::document_model {
bool ResetForm(Wrapper& form) {
  if (form.formResetting) return true;
  State& state = *form.owner;
  JSAutoRealm realm(state.cx, state.page);
  JS::RootedObject target(state.cx, form.object);
  struct ResetGuard {
    bool& value;
    explicit ResetGuard(bool& v) : value(v) { value = true; }
    ~ResetGuard() { value = false; }
  } guard(form.formResetting);
  const auto generation = state.scriptGeneration;
  bool allowed = false;
  if (!state.interaction.Fire(target, "reset", true, true, &allowed)) return false;
  if (!allowed || form.dead || form.owner != &state || generation != state.scriptGeneration) return true;
  if (!state.Check(state.model.ResetInputValues(form.key))) return false;
  std::vector<NodeKey> controls;
  if (!state.Check(state.model.FormNodes(form.key, nullptr, &controls))) return false;
  for (auto key : controls) {
    NodeSnapshot node;
    if (!state.Check(state.model.Inspect(key, &node))) return false;
    if (std::strcmp(node.interface_name, "HTMLInputElement")) continue;
    auto* entry = state.Find(key);
    if (entry && !ResetInputFiles(*entry)) return false;
  }
  return true;
}
}  // namespace ruyijs::document_model
