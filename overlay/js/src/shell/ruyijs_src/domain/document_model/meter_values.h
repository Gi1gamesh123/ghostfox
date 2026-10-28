#pragma once
#include "domain/document_model/element.h"
#include <cstdlib>

namespace ruyijs::domain::document_model {
inline double meter_raw(const Element& e, DomStringView name, double fallback) {
  const auto raw = to_utf8(e.get_attribute16(name).value_or(u""));
  char* end = nullptr;
  const double value = std::strtod(raw.c_str(), &end);
  return end == raw.c_str() ? fallback : value;
}
inline double meter_value(const Element& e, DomStringView name) {
  const double min = meter_raw(e, u"min", 0);
  if (name == u"min") return min;
  const double max = std::max(meter_raw(e, u"max", 1), min);
  if (name == u"max") return max;
  const auto clamp = [&](double value, double low) { return std::min(std::max(value, low), max); };
  if (name == u"value") return clamp(meter_raw(e, u"value", 0), min);
  if (name == u"optimum") return clamp(meter_raw(e, u"optimum", (min + max) / 2), min);
  const double low = clamp(meter_raw(e, u"low", min), min);
  if (name == u"low") return low;
  return clamp(meter_raw(e, u"high", max), low);
}
}
