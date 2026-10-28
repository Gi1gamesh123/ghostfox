// Original Element attribute hooks, separate from JS view identity.
#pragma once
namespace ruyijs::domain::document_model {
inline bool Element::has_svg_view_box() const noexcept {
  if (tag_name_.namespace16() != u"http://www.w3.org/2000/svg") return false;
  const auto& local = tag_name_.local16();
  return local == u"svg" || local == u"symbol" || local == u"marker" || local == u"pattern" || local == u"view";
}
inline SvgViewBoxState Element::NextSvgViewBox(DomStringView ns, DomStringView local,
    DomStringView value, const DomString* previous) const {
  if (!ns.empty() || local != u"viewBox" || !has_svg_view_box() || (previous && *previous == value))
    return svg_view_box_;
  return ParseSvgViewBox(value);
}
inline void Element::RemoveSvgViewBox(DomStringView ns, DomStringView local) noexcept {
  if (ns.empty() && local == u"viewBox" && has_svg_view_box()) svg_view_box_ = {};
}
inline void Element::commit_svg_view_box(SvgViewBoxState value, DomString text) {
  for (auto& attribute : attributes_)
    if (attribute.namespace_uri.empty() && attribute.local_name == u"viewBox") {
      attribute.value.swap(text); svg_view_box_ = value; return;
    }
  attributes_.push_back(Attribute{u"viewBox", std::move(text), {}, {}, u"viewBox"});
  svg_view_box_ = value;
}
}
