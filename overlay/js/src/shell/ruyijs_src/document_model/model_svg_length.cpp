// Typed SVG length attributes (docs/owners/WEB_GEOMETRY.md, 2026-10-21). The
// attribute string is the source; an SVGLength write commits an exact float
// value alongside its serialization through the original budget and publication.
#include "document_model/model_internal.h"
#include "domain/document_model/svg_length_fields.h"
#include <cmath>
#include <iterator>
namespace ruyijs::document_model {
namespace {
using ::ruyijs::domain::document_model::kSvgLengthFields;
bool FieldApplies(const domain::Element& element, unsigned field) {
  if (field >= std::size(kSvgLengthFields)) return false;
  const auto& tag = element.tag_name();
  if (tag.namespace16() != u"http://www.w3.org/2000/svg") return false;
  const auto local = tag.local16();
  const std::u16string_view wanted = kSvgLengthFields[field].tag;
  if (wanted.empty()) return local == u"text" || local == u"tspan" || local == u"textPath";
  return local == wanted;
}
// Firefox SVGElement::LengthAttrIsNonNegative: a negative attribute value is a parse error.
bool NonNegative(std::u16string_view name) {
  return name == u"width" || name == u"height" || name == u"r" || name == u"rx" || name == u"ry" ||
         name == u"markerWidth" || name == u"markerHeight" || name == u"textLength";
}
// The base value as Firefox holds it: the typed value, else the parsed attribute.
bool BaseValue(const domain::Element& element, unsigned field, SvgLengthValue* out) {
  const auto& info = kSvgLengthFields[field];
  *out = info.initial;
  if (const auto* typed = element.svg_length(info.attribute)) { *out = *typed; return true; }
  const auto text = element.get_attribute_ns16({}, info.attribute);
  SvgLengthValue parsed;
  if (!text || !::ruyijs::domain::geometry::ParseSvgLength(*text, &parsed, true) ||
      (NonNegative(info.attribute) && parsed.number < 0)) return false;
  *out = parsed;
  return true;
}
}  // namespace
Status Model::ReadSvgLength(NodeKey key, unsigned field, bool* supported, SvgLengthValue* result) const noexcept {
  return Protect([&] {
    const auto* element = State().owner(key).get_element(key.node);
    *supported = element && FieldApplies(*element, field);
    *result = {};
    if (!*supported) return;
    BaseValue(*element, field, result);
  });
}
Status Model::WriteSvgLength(NodeKey key, unsigned field, const SvgLengthValue& value) noexcept {
  return Protect([&] {
    auto& element = State().element(key);
    if (!FieldApplies(element, field) || !std::isfinite(value.number) || !value.unit || value.unit >= 26)
      throw domain::DomError(domain::DomErrorCode::InvalidState, "Invalid SVG length");
    const auto& info = kSvgLengthFields[field];
    SvgLengthValue current;
    // SVGAnimatedLength::SetBaseValue*: an equal base value is not a mutation.
    if (BaseValue(element, field, &current) && current.number == value.number && current.unit == value.unit) return;
    const auto serialized = ::ruyijs::domain::geometry::SerializeSvgLength(value);
    DomString text(serialized.begin(), serialized.end());
    const auto old = element.get_attribute_ns16({}, info.attribute);
    const size_t name = std::char_traits<char16_t>::length(info.attribute);
    State().budget(0, text.size() + (old ? 0 : name), old ? old->size() : 0);
    SvgLengthValue stored = value;
    stored.specified = true;
    element.commit_svg_length(info.attribute, stored, std::move(text));
    State().NoteAttributes(key, info.attribute, {}, old ? &*old : nullptr);
    State().Touch();
  });
}
}  // namespace ruyijs::document_model
