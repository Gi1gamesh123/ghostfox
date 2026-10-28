#include "document_model/model_internal.h"
#include "document_model/svg_points_types.h"
#include "domain/document_model/svg_points_state.h"
#include <cmath>
namespace ruyijs::document_model {
Status Model::ReadSvgPoints(NodeKey key, SvgPointsSnapshot* out) const noexcept {
  return Protect([&] {
    const auto* element = State().owner(key).get_element(key.node);
    *out = {}; out->supported = element && element->has_svg_points();
    if (out->supported) { out->base = element->svg_points().base; out->animated = element->svg_points().animated; }
  });
}
Status Model::MutateSvgPoints(NodeKey key, unsigned operation, uint32_t index,
                              const SvgPointRef& input, SvgPointRef* result) noexcept {
  return Protect([&] {
    auto& element = State().element(key);
    if (!element.has_svg_points()) throw domain::DomError(domain::DomErrorCode::InvalidState, "Invalid SVG point list");
    const auto& previous = element.svg_points();
    auto next = previous;
    *result = nullptr;
    if (operation == SvgPointsClear && next.base.empty()) return;
    if ((operation == SvgPointsReplace || operation == SvgPointsRemove) && index >= next.base.size())
      throw domain::DomError(domain::DomErrorCode::IndexSize, "Index out of range");
    SvgPointRef value, animated;
    if (operation != SvgPointsClear && operation != SvgPointsRemove) {
      if (!input) throw domain::DomError(domain::DomErrorCode::InvalidState, "Invalid SVG point");
      value = input->attached ? domain::SvgPointCopy(*input, false, false) : input;
    }
    std::vector<SvgPointRef> detached;
    if (operation == SvgPointsClear || operation == SvgPointsInitialize) {
      detached = next.base; detached.insert(detached.end(), next.animated.begin(), next.animated.end());
      next.base.clear(); next.animated.clear(); index = 0;
    }
    if (operation == SvgPointsRemove) {
      *result = next.base[index]; detached = {next.base[index], next.animated[index]};
      next.base.erase(next.base.begin() + index); next.animated.erase(next.animated.begin() + index);
    } else if (operation == SvgPointsReplace) {
      detached = {next.base[index]}; next.base[index] = value; animated = next.animated[index]; *result = value;
    } else if (operation != SvgPointsClear) {
      if (operation == SvgPointsAppend) index = uint32_t(next.base.size());
      index = std::min(index, uint32_t(next.base.size()));
      animated = domain::SvgPointCopy(*value, true, true);
      next.base.insert(next.base.begin() + index, value);
      next.animated.insert(next.animated.begin() + index, animated); *result = value;
    }
    auto text = domain::SerializeSvgPoints(next.base);
    const auto old = element.get_attribute_ns16({}, u"points");
    State().budget(0, text.size() + (old ? 0 : 6), old ? old->size() : 0);
    element.commit_svg_points(std::move(next), std::move(text));
    for (auto& point : detached) domain::DetachSvgPoint(point);
    if (value) { value->attached = true; value->animated = false; }
    if (animated) { animated->x = value->x; animated->y = value->y; }
    State().NoteAttributes(key, u"points", {}, old ? &*old : nullptr); State().Touch();
  });
}
Status Model::WriteSvgPointField(NodeKey key, const SvgPointRef& point, unsigned field, float value) noexcept {
  return Protect([&] {
    auto& element = State().element(key);
    auto next = element.svg_points();
    const auto found = std::find(next.base.begin(), next.base.end(), point);
    if (!element.has_svg_points() || found == next.base.end() || field > 1 || !std::isfinite(value))
      throw domain::DomError(domain::DomErrorCode::InvalidState, "Invalid SVG point field");
    if ((field ? point->y : point->x) == value) return;
    const size_t index = size_t(found - next.base.begin());
    auto serialized = next.base;
    serialized[index] = domain::SvgPointCopy(*point, true, false);
    (field ? serialized[index]->y : serialized[index]->x) = value;
    auto text = domain::SerializeSvgPoints(serialized);
    const auto old = element.get_attribute_ns16({}, u"points");
    State().budget(0, text.size() + (old ? 0 : 6), old ? old->size() : 0);
    element.commit_svg_points(std::move(next), std::move(text));
    (field ? point->y : point->x) = value;
    auto& mirror = element.svg_points().animated[index];
    (field ? mirror->y : mirror->x) = value;
    State().NoteAttributes(key, u"points", {}, old ? &*old : nullptr); State().Touch();
  });
}
}
