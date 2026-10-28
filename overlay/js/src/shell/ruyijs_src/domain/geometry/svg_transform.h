#pragma once
#include "domain/geometry/svg_values.h"
#include <cstdlib>
#include <string>

namespace ruyijs::domain::geometry {
inline SvgMatrix ParseSvgTransform(const std::string& source) {
  SvgMatrix result;
  size_t at = 0;
  while (at < source.size()) {
    const auto open = source.find('(', at), close = source.find(')', open);
    if (open == source.npos || close == source.npos) break;
    auto name = source.substr(at, open-at);
    const auto start = name.find_first_not_of(" ,\t\r\n");
    if (start == name.npos) break;
    name.erase(0, start);
    const auto numbers = source.substr(open+1, close-open-1);
    const char* p = numbers.c_str();
    double values[6]{}; unsigned count = 0;
    while (*p && count < 6) {
      while (*p == ' ' || *p == ',' || *p == '\t' || *p == '\n') ++p;
      char* end = nullptr;
      const double number = std::strtod(p, &end);
      if (end == p || !std::isfinite(number)) break;
      values[count++] = number; p = end;
      while ((*p >= 'a' && *p <= 'z') || *p == '%') ++p;
    }
    SvgMatrix next;
    if (name == "matrix" && count == 6) next = {values[0],values[1],values[2],values[3],values[4],values[5]};
    else if (name == "scale" && count) { next.a=float(values[0]); next.d=float(count > 1 ? values[1] : values[0]); }
    else if (name == "scaleX" && count) next.a=float(values[0]);
    else if (name == "scaleY" && count) next.d=float(values[0]);
    else if (name == "translate" && count) { next.e=values[0]; next.f=count > 1 ? values[1] : 0; }
    else if (name == "translateX" && count) next.e=values[0];
    else if (name == "translateY" && count) next.f=values[0];
    else if ((name == "rotate" || name == "skewX" || name == "skewY") && count) {
      const auto angle = values[0]*3.14159265358979323846/180;
      if (name == "rotate") {
        next.a=next.d=std::cos(angle); next.b=std::sin(angle); next.c=-next.b;
        if (count == 3) { next.e=values[1]-next.a*values[1]-next.c*values[2];
                         next.f=values[2]-next.b*values[1]-next.d*values[2]; }
      } else if (name == "skewX") next.c=std::tan(angle); else next.b=std::tan(angle);
    } else return {};
    result = result.Then(next); at = close+1;
  }
  return result;
}
}
