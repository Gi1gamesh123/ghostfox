#ifndef RUYIJS_DOMAIN_ANIMATION_EASING_H
#define RUYIJS_DOMAIN_ANIMATION_EASING_H
// CSS <easing-function> (css-easing-2) for Web Animations: parse, serialize the
// way Firefox does, and evaluate. Product-authored, engine-independent.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace ruyijs::domain::animation {
enum class EasingKind { Linear, Cubic, Steps, Points };
enum class StepPosition { JumpStart, JumpEnd, JumpNone, JumpBoth, Start, End };
struct Easing {
  EasingKind kind = EasingKind::Linear;
  double x1 = 0, y1 = 0, x2 = 1, y2 = 1;
  int steps = 1;
  StepPosition position = StepPosition::End;
  std::vector<std::pair<double, double>> points;  // (output, input position 0..1)
  std::string text = "linear";                    // Firefox serialization
};

// Shortest decimal that round-trips the float32 value, as Firefox serializes CSS numbers.
inline std::string CssNumber(double value) {
  const float f = float(value);
  if (f == 0) return "0";
  char buffer[64];
  int digits = 1;
  for (; digits < 9; ++digits) {  // fewest significant digits that round-trip the float
    std::snprintf(buffer, sizeof buffer, "%.*g", digits, double(f));
    if (float(std::strtod(buffer, nullptr)) == f) break;
  }
  // Fixed notation with exactly those significant digits, trailing zeros removed.
  const int magnitude = int(std::floor(std::log10(std::fabs(double(f)))));
  const int decimals = digits - 1 - magnitude > 0 ? digits - 1 - magnitude : 0;
  std::snprintf(buffer, sizeof buffer, "%.*f", decimals, double(f));
  std::string text = buffer;
  if (text.find('.') != std::string::npos) {
    while (text.back() == '0') text.pop_back();
    if (text.back() == '.') text.pop_back();
  }
  return text == "-0" ? "0" : text;
}

namespace detail {
inline std::string Trim(const std::string& s) {
  const char* ws = " \t\n\f\r";
  const size_t b = s.find_first_not_of(ws);
  if (b == std::string::npos) return "";
  return s.substr(b, s.find_last_not_of(ws) - b + 1);
}
inline bool Number(const std::string& text, double* out) {
  const std::string s = Trim(text);
  size_t i = 0, digits = 0;
  if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
  while (i < s.size() && s[i] >= '0' && s[i] <= '9') { ++i; ++digits; }
  if (i < s.size() && s[i] == '.') {
    ++i;
    size_t fraction = 0;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') { ++i; ++fraction; }
    if (!fraction) return false;
    digits += fraction;
  }
  if (!digits) return false;
  if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
    size_t j = i + 1;
    if (j < s.size() && (s[j] == '+' || s[j] == '-')) ++j;
    size_t exponent = 0;
    while (j < s.size() && s[j] >= '0' && s[j] <= '9') { ++j; ++exponent; }
    if (exponent) i = j;
  }
  if (i != s.size()) return false;
  *out = std::strtod(s.c_str(), nullptr);
  return std::isfinite(*out);
}
inline bool Integer(const std::string& text, int* out) {
  const std::string s = Trim(text);
  size_t i = (!s.empty() && (s[0] == '+' || s[0] == '-')) ? 1 : 0;
  if (i == s.size()) return false;
  for (size_t j = i; j < s.size(); ++j) if (s[j] < '0' || s[j] > '9') return false;
  const double v = std::strtod(s.c_str(), nullptr);
  if (v > 2147483647.0 || v < -2147483648.0) return false;
  *out = int(v);
  return true;
}
inline std::vector<std::string> Split(const std::string& args) {
  std::vector<std::string> parts;
  size_t start = 0;
  for (size_t i = 0; i <= args.size(); ++i)
    if (i == args.size() || args[i] == ',') { parts.push_back(Trim(args.substr(start, i - start))); start = i + 1; }
  return parts;
}
inline Easing Cubic(double a, double b, double c, double d, const std::string& text) {
  Easing e; e.kind = EasingKind::Cubic; e.x1 = a; e.y1 = b; e.x2 = c; e.y2 = d; e.text = text; return e;
}
inline Easing Steps(int n, StepPosition p, const std::string& text) {
  Easing e; e.kind = EasingKind::Steps; e.steps = n; e.position = p; e.text = text; return e;
}
inline bool ParseLinearPoints(const std::vector<std::string>& parts, Easing* out);  // easing_points.h
}  // namespace detail

// Parses an easing; false for anything Firefox rejects.
inline bool ParseEasing(const std::string& input, Easing* out) {
  std::string s = detail::Trim(input);
  for (char& c : s) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
  if (s == "linear") { *out = Easing(); return true; }
  if (s == "ease") { *out = detail::Cubic(0.25, 0.1, 0.25, 1, "ease"); return true; }
  if (s == "ease-in") { *out = detail::Cubic(0.42, 0, 1, 1, "ease-in"); return true; }
  if (s == "ease-out") { *out = detail::Cubic(0, 0, 0.58, 1, "ease-out"); return true; }
  if (s == "ease-in-out") { *out = detail::Cubic(0.42, 0, 0.58, 1, "ease-in-out"); return true; }
  if (s == "step-start") { *out = detail::Steps(1, StepPosition::Start, "steps(1, start)"); return true; }
  if (s == "step-end") { *out = detail::Steps(1, StepPosition::End, "steps(1)"); return true; }
  const size_t open = s.find('(');
  if (open == std::string::npos || s.back() != ')') return false;
  const std::string name = s.substr(0, open);
  const auto parts = detail::Split(s.substr(open + 1, s.size() - open - 2));
  if (name == "cubic-bezier") {
    double v[4];
    if (parts.size() != 4) return false;
    for (int i = 0; i < 4; ++i) if (!detail::Number(parts[i], &v[i])) return false;
    if (v[0] < 0 || v[0] > 1 || v[2] < 0 || v[2] > 1) return false;
    std::string text = "cubic-bezier(";
    for (int i = 0; i < 4; ++i) text += (i ? ", " : "") + CssNumber(v[i]);
    *out = detail::Cubic(v[0], v[1], v[2], v[3], text + ")");
    return true;
  }
  if (name == "steps") {
    int n = 0;
    if (parts.empty() || parts.size() > 2 || !detail::Integer(parts[0], &n)) return false;
    StepPosition p = StepPosition::End;
    std::string keyword = parts.size() == 2 ? parts[1] : "end";
    if (keyword == "start") p = StepPosition::Start;
    else if (keyword == "end") p = StepPosition::End;
    else if (keyword == "jump-start") p = StepPosition::JumpStart;
    else if (keyword == "jump-end") p = StepPosition::JumpEnd;
    else if (keyword == "jump-none") p = StepPosition::JumpNone;
    else if (keyword == "jump-both") p = StepPosition::JumpBoth;
    else return false;
    if (n < 1 || (p == StepPosition::JumpNone && n < 2)) return false;
    const bool plain = p == StepPosition::End || p == StepPosition::JumpEnd;
    *out = detail::Steps(n, p, "steps(" + std::to_string(n) + (plain ? "" : ", " + keyword) + ")");
    return true;
  }
  if (name == "linear") return detail::ParseLinearPoints(parts, out);
  return false;
}
}  // namespace ruyijs::domain::animation
#include "domain/animation/easing_points.h"
#endif
