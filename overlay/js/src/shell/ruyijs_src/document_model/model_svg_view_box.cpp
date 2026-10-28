// Typed viewBox commits share original attribute budgets and mutation publication.
#include "document_model/model_internal.h"
#include <cmath>
namespace ruyijs::document_model {
Status Model::ReadSvgViewBox(NodeKey key, bool* supported, SvgViewBoxValue* result) const noexcept {
  return Protect([&] {
    const auto* element = State().owner(key).get_element(key.node);
    *supported = element && element->has_svg_view_box();
    *result = {};
    if (*supported) for (unsigned i = 0; i < 4; ++i) result->fields[i] = element->svg_view_box().fields[i];
    if (*supported) result->present = element->svg_view_box().hasBaseValue && !element->svg_view_box().none;
  });
}
Status Model::WriteSvgViewBoxField(NodeKey key, unsigned field, float value) noexcept {
  return Protect([&] {
    auto& element = State().element(key);
    if (!element.has_svg_view_box() || field >= 4 || !std::isfinite(value))
      throw domain::DomError(domain::DomErrorCode::InvalidState, "Invalid SVG viewBox field");
    auto next = element.svg_view_box();
    if (next.hasBaseValue && !next.none && next.fields[field] == value) return;
    next.fields[field] = value; next.hasBaseValue = true; next.none = false;
    auto text = domain::SerializeSvgViewBox(next);
    const auto old = element.get_attribute_ns16({}, u"viewBox");
    State().budget(0, text.size() + (old ? 0 : 7), old ? old->size() : 0);
    element.commit_svg_view_box(next, std::move(text));
    State().NoteAttributes(key, u"viewBox", {}, old ? &*old : nullptr);
    State().Touch();
  });
}
}
