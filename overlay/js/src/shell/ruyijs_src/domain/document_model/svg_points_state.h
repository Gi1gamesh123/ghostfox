#pragma once
#include "domain/document_model/svg_view_box_state.h"
#include "domain/document_model/svg_point_value.h"
#include <algorithm>
#include <memory>
#include <vector>
namespace ruyijs::domain::document_model {
struct SvgPointsState { std::vector<SvgPointRef> base, animated; };
inline std::vector<SvgPointValue> ParseSvgPoints(DomStringView text) {
  std::vector<float> numbers;
  size_t p = 0; bool afterComma = false;
  while (p < text.size()) {
    while (p < text.size() && SvgViewBoxSpace(text[p])) ++p;
    if (p == text.size()) break;
    const size_t start = p;
    if (text[p] == u'+' || text[p] == u'-') ++p;
    while (p < text.size() && SvgViewBoxDigit(text[p])) ++p;
    if (p < text.size() && text[p] == u'.') {
      ++p; while (p < text.size() && SvgViewBoxDigit(text[p])) ++p;
    }
    if (p < text.size() && (text[p] == u'e' || text[p] == u'E')) {
      ++p; if (p < text.size() && (text[p] == u'+' || text[p] == u'-')) ++p;
      while (p < text.size() && SvgViewBoxDigit(text[p])) ++p;
    }
    float value = 0;
    if (!SvgViewBoxNumber(text.substr(start, p - start), &value)) return {};
    numbers.push_back(value); afterComma = false;
    const size_t end = p;
    while (p < text.size() && SvgViewBoxSpace(text[p])) ++p;
    if (p < text.size() && text[p] == u',') { ++p; afterComma = true; }
    else if (p < text.size() && p == end && (text[p] != u'-' || numbers.size() % 2 == 0)) return {};
  }
  if (afterComma && numbers.size() % 2 == 0) return {};
  std::vector<SvgPointValue> result;
  result.reserve(numbers.size() / 2);
  for (size_t i = 0; i + 1 < numbers.size(); i += 2) result.push_back({numbers[i], numbers[i + 1]});
  return result;
}
inline void SyncSvgPoints(SvgPointsState& state, const std::vector<SvgPointValue>& values) {
  // Allocate before mutating any existing record, including both mirrored views.
  auto base = state.base, animated = state.animated;
  while (base.size() < values.size()) base.push_back(SvgPointCopy({}, true, false));
  while (animated.size() < values.size()) animated.push_back(SvgPointCopy({}, true, true));
  for (size_t i = values.size(); i < base.size(); ++i) DetachSvgPoint(base[i]);
  for (size_t i = values.size(); i < animated.size(); ++i) DetachSvgPoint(animated[i]);
  base.resize(values.size()); animated.resize(values.size());
  for (size_t i = 0; i < values.size(); ++i) {
    base[i]->x = animated[i]->x = values[i].x;
    base[i]->y = animated[i]->y = values[i].y;
  }
  state.base.swap(base); state.animated.swap(animated);
}
inline DomString SerializeSvgPoints(const std::vector<SvgPointRef>& points) {
  DomString result;
  for (size_t i = 0; i < points.size(); ++i) {
    if (i) result += u' ';
    for (unsigned field = 0; field < 2; ++field) {
      if (field) result += u',';
      char buffer[64];
      auto value = std::to_chars(buffer, buffer + sizeof buffer, field ? points[i]->y : points[i]->x,
                                std::chars_format::general, 6);
      if (value.ec != std::errc{}) throw std::length_error("SVG points serialization failed");
      for (const char* p = buffer; p != value.ptr; ++p) result += char16_t(*p);
    }
  }
  return result;
}
}
