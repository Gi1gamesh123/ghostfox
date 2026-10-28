#pragma once
namespace ruyijs::domain::document_model {
inline bool Element::has_svg_points() const noexcept {
  return tag_name_.namespace16() == u"http://www.w3.org/2000/svg" &&
      (tag_name_.local16() == u"polyline" || tag_name_.local16() == u"polygon");
}
inline void Element::RefreshSvgPoints(DomStringView ns, DomStringView local, DomStringView value,
                                      const DomString* previous) {
  if (!ns.empty() || local != u"points" || !has_svg_points() || (previous && *previous == value)) return;
  SyncSvgPoints(svg_points_, ParseSvgPoints(value));
}
inline void Element::RemoveSvgPoints(DomStringView ns, DomStringView local) noexcept {
  if (!ns.empty() || local != u"points" || !has_svg_points()) return;
  for (auto& point : svg_points_.base) DetachSvgPoint(point);
  for (auto& point : svg_points_.animated) DetachSvgPoint(point);
  svg_points_.base.clear(); svg_points_.animated.clear();
}
inline void Element::commit_svg_points(SvgPointsState state, DomString text) {
  for (auto& attribute : attributes_)
    if (attribute.namespace_uri.empty() && attribute.local_name == u"points") {
      attribute.value.swap(text); svg_points_ = std::move(state); return;
    }
  attributes_.push_back(Attribute{u"points", std::move(text), {}, {}, u"points"});
  svg_points_ = std::move(state);
}
inline const geometry::SvgLengthValue* Element::svg_length(DomStringView local) const noexcept {
  for (const auto& entry : svg_lengths_) if (entry.first == local) return &entry.second;
  return nullptr;
}
inline void Element::ForgetSvgLength(DomStringView ns, DomStringView local) noexcept {
  if (!ns.empty() || svg_lengths_.empty()) return;
  for (auto it = svg_lengths_.begin(); it != svg_lengths_.end(); ++it)
    if (it->first == local) { svg_lengths_.erase(it); return; }
}
inline void Element::commit_svg_length(DomStringView local, geometry::SvgLengthValue value, DomString text) {
  auto cached = svg_lengths_;
  bool found = false;
  for (auto& entry : cached) if (entry.first == local) { entry.second = value; found = true; }
  if (!found) cached.emplace_back(DomString(local), value);
  for (auto& attribute : attributes_)
    if (attribute.namespace_uri.empty() && attribute.local_name == local) {
      attribute.value.swap(text); svg_lengths_.swap(cached); return;
    }
  attributes_.push_back(Attribute{DomString(local), std::move(text), {}, {}, DomString(local)});
  svg_lengths_.swap(cached);
}
}
