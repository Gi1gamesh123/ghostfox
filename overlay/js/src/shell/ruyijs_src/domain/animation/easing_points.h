#ifndef RUYIJS_DOMAIN_ANIMATION_EASING_POINTS_H
#define RUYIJS_DOMAIN_ANIMATION_EASING_POINTS_H
// The linear() easing function and easing evaluation (css-easing-2).
// Included by easing.h; product-authored, engine-independent.
#include <algorithm>
#include <cmath>
#include <optional>
#include <sstream>

namespace ruyijs::domain::animation {
namespace detail {
inline bool Percent(const std::string& token, double* out) {
  if (token.size() < 2 || token.back() != '%') return false;
  if (!Number(token.substr(0, token.size() - 1), out)) return false;
  *out /= 100;
  return true;
}
inline bool ParseLinearPoints(const std::vector<std::string>& parts, Easing* out) {
  std::vector<std::pair<double, std::optional<double>>> stops;
  for (const std::string& part : parts) {
    std::istringstream words(part);
    std::string word;
    std::optional<double> value;
    std::vector<double> positions;
    while (words >> word) {
      double number = 0;
      if (Percent(word, &number)) {
        if (positions.size() == 2) return false;
        positions.push_back(number);
      } else if (!value && Number(word, &number)) {
        if (!positions.empty()) return false;  // <number> leads its percentages here
        value = number;
      } else {
        return false;
      }
    }
    if (!value) return false;
    if (positions.empty()) stops.push_back({*value, std::nullopt});
    for (double p : positions) stops.push_back({*value, p});
  }
  if (stops.size() < 2) return false;
  if (!stops.front().second) stops.front().second = 0.0;
  if (!stops.back().second) stops.back().second = 1.0;
  double largest = *stops.front().second;
  for (auto& stop : stops)
    if (stop.second) { stop.second = std::max(*stop.second, largest); largest = *stop.second; }
  for (size_t i = 1; i < stops.size();) {
    if (stops[i].second) { ++i; continue; }
    size_t j = i;
    while (!stops[j].second) ++j;
    const double from = *stops[i - 1].second, to = *stops[j].second;
    for (size_t k = i; k < j; ++k)
      stops[k].second = from + (to - from) * double(k - i + 1) / double(j - i + 1);
    i = j;
  }
  Easing e;
  e.kind = EasingKind::Points;
  e.text = "linear(";
  for (size_t i = 0; i < stops.size(); ++i) {
    e.points.push_back({stops[i].first, *stops[i].second});
    e.text += (i ? ", " : "") + CssNumber(stops[i].first) + " " + CssNumber(*stops[i].second * 100) + "%";
  }
  e.text += ")";
  *out = e;
  return true;
}
// Evaluation ported from Firefox's Servo style code (servo/components/style/
// bezier.rs, values/computed/easing.rs, piecewise_linear.rs) so progress values
// match Firefox bit for bit: control points are f32 widened to f64, epsilon 1e-7.
inline double CubicAt(const Easing& e, double x) {
  const double x1 = double(float(e.x1)), y1 = double(float(e.y1));
  const double x2 = double(float(e.x2)), y2 = double(float(e.y2));
  if (x1 == y1 && x2 == y2) return x;
  if (x == 0.0) return 0.0;
  if (x == 1.0) return 1.0;
  if (x < 0.0) {
    if (x1 > 0.0) return x * y1 / x1;
    if (y1 == 0.0 && x2 > 0.0) return x * y2 / x2;
    return 0.0;
  }
  if (x > 1.0) {
    if (x2 < 1.0) return 1.0 + (x - 1.0) * (y2 - 1.0) / (x2 - 1.0);
    if (y2 == 1.0 && x1 < 1.0) return 1.0 + (x - 1.0) * (y1 - 1.0) / (x1 - 1.0);
    return 1.0;
  }
  const double cx = 3. * x1, bx = 3. * (x2 - x1) - cx, ax = 1.0 - cx - bx;
  const double cy = 3. * y1, by = 3. * (y2 - y1) - cy, ay = 1.0 - cy - by;
  const auto sampleX = [&](double t) { return ((ax * t + bx) * t + cx) * t; };
  const double epsilon = 1e-7;
  double t = x;
  bool solved = false;
  for (int i = 0; i < 8; ++i) {
    const double x2t = sampleX(t);
    if (std::fabs(x2t - x) < epsilon) { solved = true; break; }
    const double dx = (3.0 * ax * t + 2.0 * bx) * t + cx;
    if (std::fabs(dx) < 1e-6) break;
    t -= (x2t - x) / dx;
  }
  if (!solved) {
    double lo = 0.0, hi = 1.0;
    t = x;
    while (lo < hi) {
      const double x2t = sampleX(t);
      if (std::fabs(x2t - x) < epsilon) break;
      if (x > x2t) lo = t; else hi = t;
      t = (hi - lo) / 2.0 + lo;
    }
  }
  return ((ay * t + by) * t + cy) * t;
}
inline double StepsAt(const Easing& e, double x, bool before) {
  int step = int(std::floor(x * double(e.steps)));
  if (e.position == StepPosition::Start || e.position == StepPosition::JumpStart ||
      e.position == StepPosition::JumpBoth)
    ++step;
  double remainder = std::fmod(x * double(e.steps), 1.0);
  if (remainder < 0) remainder += 1.0;  // rem_euclid
  if (before && std::fabs(remainder) < 1e-6) --step;
  if (x >= 0.0 && step < 0) step = 0;
  int jumps = e.steps;
  if (e.position == StepPosition::JumpBoth) jumps = e.steps + 1;
  else if (e.position == StepPosition::JumpNone) jumps = e.steps - 1;
  if (x <= 1.0 && step > jumps) step = jumps;
  return double(step) / double(jumps);
}
inline float Interpolate(float x, std::pair<float, float> prev, std::pair<float, float> next, float asymptote) {
  if (std::fabs(prev.first - next.first) < 1e-6f || std::fabs(prev.second - next.second) < 1e-6f) return asymptote;
  const float slope = (next.second - prev.second) / (next.first - prev.first);
  return slope * (x - prev.first) + prev.second;
}
inline double PointsAt(const Easing& e, double input) {
  const float x = float(input);
  if (!std::isfinite(x)) return x > 0 ? 1.0 : 0.0;
  std::vector<std::pair<float, float>> p;  // (x position, y output), f32 as in Servo
  for (const auto& point : e.points) p.push_back({float(point.second), float(point.first)});
  if (p.size() == 1) return p[0].second;
  if (x < p[0].first) return Interpolate(x, p[0], p[1], p[0].second);
  if (x >= p.back().first) return Interpolate(x, p[p.size() - 2], p.back(), p.back().second);
  for (size_t i = p.size() - 1; i-- > 0;)
    if (x >= p[i].first) return Interpolate(x, p[i], p[i + 1], p[i].second);
  return x;
}
}  // namespace detail

// The transformed progress for an input progress; `before` is the before flag.
inline double EvaluateEasing(const Easing& e, double x, bool before) {
  switch (e.kind) {
    case EasingKind::Linear: return x;
    case EasingKind::Cubic: return detail::CubicAt(e, x);
    case EasingKind::Steps: return detail::StepsAt(e, x, before);
    case EasingKind::Points: return detail::PointsAt(e, x);
  }
  return x;
}
}  // namespace ruyijs::domain::animation
#endif
