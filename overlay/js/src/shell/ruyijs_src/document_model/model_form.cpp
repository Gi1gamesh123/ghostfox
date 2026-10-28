#include "document_model/model_internal.h"
#include "domain/document_model/form_tree.h"
#include "domain/document_model/form_values.h"

namespace ruyijs::document_model {
Status Model::FormNodes(NodeKey form, const DomStringView* name, std::vector<NodeKey>* out) const noexcept {
  return Protect([&] {
    std::vector<NodeKey> values;
    for (auto id : domain::form_nodes(State().owner(form), form.node, name)) values.push_back(Key(form.document, id));
    out->swap(values);
  });
}
Status Model::FormAttribute(NodeKey key, DomStringView name, DomString* out) const noexcept {
  return Protect([&] {
    *out = domain::form_attribute(name, State().element(key).get_attribute16(name).value_or(u""));
  });
}
Status Model::SetFormAttribute(NodeKey key, DomStringView name, DomStringView value) noexcept {
  DomString normalized;
  auto status = Protect([&] { normalized = domain::form_attribute(name, value); });
  if (status.code != ErrorCode::None) return status;
  bool normalize = name == u"autocomplete" || name == u"method" || name == u"enctype";
  return SetAttribute(key, name, normalize ? DomStringView(normalized) : value);
}
Status Model::RadioValue(NodeKey form, DomStringView name, DomString* out) const noexcept {
  return Protect([&] {
    const auto& doc = State().owner(form);
    *out = domain::radio_value(doc, domain::form_nodes(doc, form.node, &name));
  });
}
Status Model::SetRadioValue(NodeKey form, DomStringView name, DomStringView value) noexcept {
  NodeKey selected;
  auto status = Protect([&] {
    const auto& doc = State().owner(form);
    for (auto id : domain::form_nodes(doc, form.node, &name)) {
      const auto& e = *doc.get_element(id);
      if (!domain::form_radio(e)) continue;
      if (Null(selected) && domain::input_value(e) == value)
        selected = Key(form.document, id);
    }
  });
  if (status.code != ErrorCode::None || Null(selected)) return status;
  return SetInputChecked(selected, true);
}
}
