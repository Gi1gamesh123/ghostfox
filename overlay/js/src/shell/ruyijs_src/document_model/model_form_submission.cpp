#include "document_model/model_internal.h"
#include "document_model/form_submission.h"
#include "domain/document_model/form_submission.h"

namespace ruyijs::document_model {
Status Model::FormOwner(NodeKey key, NodeKey* out) const noexcept {
  return Protect([&] {
    const auto& doc = State().owner(key);
    auto id = key.node;
    if (domain::form_tag(doc, id, u"option")) {
      for (id = doc.require(id).parent_id(); id; id = doc.require(id).parent_id())
        if (domain::form_tag(doc, id, u"select")) break;
    }
    *out = Key(key.document, domain::form_owner(doc, id));
  });
}
Status Model::FormControlInfo(NodeKey key, bool* disabled, bool* submitButton) const noexcept {
  return Protect([&] {
    const auto& doc = State().owner(key);
    *disabled = domain::form_disabled(doc, key.node);
    *submitButton = domain::form_submit_button(doc, key.node);
  });
}
Status Model::FormFields(NodeKey key, NodeKey submitter, std::vector<FormField>* out) const noexcept {
  return Protect([&] {
    out->clear();
    for (auto& field : domain::form_entries(State().owner(key), key.node, submitter.node))
      out->push_back({std::move(field.name), std::move(field.value), field.empty_file});
  });
}
}
