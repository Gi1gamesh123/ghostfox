#include "document_model/html_control.h"
#include "document_model/form_submission.h"
#include "interaction/realm.h"
#include "js/Realm.h"

namespace ruyijs::document_model {
bool ControlForm(Wrapper& entry, JS::MutableHandleValue result) {
  auto& s = *entry.owner;
  NodeKey key;
  return s.Check(s.model.FormOwner(entry.key, &key)) && s.Wrap(key, result);
}
bool ValidateForm(Wrapper& form, bool* valid) {
  auto& s = *form.owner;
  std::vector<NodeKey> nodes;
  *valid = true;
  if (!s.Check(s.model.FormNodes(form.key, nullptr, &nodes))) return false;
  for (auto key : nodes) {
    bool disabled, submit, will = false;
    NodeSnapshot node;
    uint32_t flags = 0;
    if (!s.Check(s.model.Inspect(key, &node)) ||
        !s.Check(s.model.FormControlInfo(key, &disabled, &submit))) return false;
    if (disabled) continue;
    if (node.local_name == u"input") {
      if (!s.Check(s.model.InputWillValidate(key, &will))) return false;
    } else if (node.local_name == u"textarea") {
      if (!s.Check(s.model.TextareaWillValidate(key, &will))) return false;
    } else if (node.local_name == u"select" || submit) will = true;
    if (!will) continue;
    if (!s.Check(s.model.ControlValidation(key, &flags, nullptr))) return false;
    if (!flags) continue;
    *valid = false;
    JS::RootedValue value(s.cx);
    if (!s.Wrap(key, &value)) return false;
    JS::RootedObject target(s.cx, &value.toObject());
    if (!s.interaction.Fire(target, "invalid", false, true)) return false;
  }
  return true;
}
void Realm::SetFormPorts(FormDataPort data, FormNavigatePort navigate, void* context) {
  state_->formDataPort = data; state_->formNavigatePort = navigate; state_->formContext = context;
}
void Realm::CopyFormPortsFrom(const Realm& source) {
  SetFormPorts(source.state_->formDataPort, source.state_->formNavigatePort, source.state_->formContext);
}
bool Realm::FormFields(JS::HandleObject form, JS::HandleValue submitter,
                       std::vector<FormField>* out) {
  State& s = *state_;
  Wrapper* formEntry = s.Find(form);
  if (!formEntry) return TypeError(s.cx, "FormData requires an HTMLFormElement");
  NodeSnapshot snapshot;
  if (!s.Check(s.model.Inspect(formEntry->key, &snapshot))) return false;
  if (snapshot.local_name != u"form")
    return TypeError(s.cx, "FormData requires an HTMLFormElement");
  NodeKey submitterKey;
  if (!submitter.isUndefined() && !submitter.isNull()) {
    if (!submitter.isObject()) return TypeError(s.cx, "FormData submitter is not an element");
    Wrapper* button = s.Find(&submitter.toObject());
    if (!button || button->owner != &s)
      return TypeError(s.cx, "FormData submitter is not an element");
    bool disabled = false, submitButton = false;
    if (!s.Check(s.model.FormControlInfo(button->key, &disabled, &submitButton))) return false;
    if (!submitButton) return TypeError(s.cx, "FormData submitter is not a submit button");
    NodeKey owner;
    if (!s.Check(s.model.FormOwner(button->key, &owner))) return false;
    if (!Same(owner, formEntry->key))
      return ThrowDOM(s, "NotFoundError", "The submitter does not belong to this form");
    submitterKey = button->key;
  }
  return s.Check(s.model.FormFields(formEntry->key, submitterKey, out));
}
bool Realm::FormNavigationCurrent(JS::HandleObject object, uint64_t version) {
  auto& s = *state_;
  auto* entry = s.Find(object);
  if (!entry || entry->formNavigationVersion != version) return false;
  NodeSnapshot node;
  return s.Check(s.model.Inspect(entry->key, &node)) && node.is_connected;
}
}
