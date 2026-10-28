#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace ruyijs::domain::geometry {
using SvgBox = std::array<float, 4>;
struct SvgBounds {
  SvgBox box{};
  bool present = false;
  void Include(const SvgBox& value) {
    if (!present) { box = value; present = true; return; }
    const float x = std::min(box[0], value[0]), y = std::min(box[1], value[1]);
    const float r = std::max(box[0] + box[2], value[0] + value[2]);
    const float b = std::max(box[1] + box[3], value[1] + value[3]);
    box = {x, y, r - x, b - y};
  }
};
struct SvgMatrix {
  double a = 1, b = 0, c = 0, d = 1, e = 0, f = 0;
  SvgMatrix Then(const SvgMatrix& n) const {
    return {a*n.a+c*n.b, b*n.a+d*n.b, a*n.c+c*n.d, b*n.c+d*n.d,
            a*n.e+c*n.f+e, b*n.e+d*n.f+f};
  }
  SvgBox Apply(const SvgBox& v) const {
    return ApplyUnrounded({v[0], v[1], v[2], v[3]});
  }
  SvgBox ApplyUnrounded(const std::array<double, 4>& v) const {
    double xs[4], ys[4];
    for (unsigned i = 0; i < 4; ++i) {
      double x = v[0] + (i & 1 ? v[2] : 0), y = v[1] + (i & 2 ? v[3] : 0);
      xs[i] = a*x+c*y+e; ys[i] = b*x+d*y+f;
    }
    const auto xx = std::minmax_element(xs, xs+4), yy = std::minmax_element(ys, ys+4);
    return {float(*xx.first), float(*yy.first), float(*xx.second-*xx.first), float(*yy.second-*yy.first)};
  }
  float Scale() const { return float(std::max(std::hypot(a,b), std::hypot(c,d))); }
};
struct SvgTextMeasurement {
  SvgBox box{}, transformed{};
  std::vector<SvgBox> characters;
  float length = 0;
};
}
