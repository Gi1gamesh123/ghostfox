// The value of a CSS colour: the syntaxes a canvas fillStyle accepts become the
// 8-bit sRGB bytes the paint is built from. The grammar is the one the styling
// owner already validates; this resolves it the way the CSS colour spec does.
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "domain/styling/css_color_value.h"
#include "domain/styling/css_components.h"

namespace ruyijs::domain::styling {
namespace {
enum ChannelKind { kNumber, kPercent, kAngle, kNotANumber };

// One numeric channel: a plain number, a percentage or (for a hue) an angle.
ChannelKind ReadChannel(const std::string& text, bool allowAngle, double* value) {
  if (text.empty()) return kNotANumber;
  char* end = nullptr;
  const double number = std::strtod(text.c_str(), &end);
  if (end == text.c_str() || !std::isfinite(number)) return kNotANumber;
  std::string unit = text.substr(size_t(end - text.c_str()));
  for (char& c : unit) {
    if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
  }
  *value = number;
  if (unit.empty()) return kNumber;
  if (unit == "%") return kPercent;
  if (!allowAngle) return kNotANumber;
  if (unit == "deg") return kAngle;
  if (unit == "grad") {
    *value = number * 360.0 / 400.0;
    return kAngle;
  }
  if (unit == "rad") {
    *value = number * 180.0 / 3.14159265358979323846;
    return kAngle;
  }
  if (unit == "turn") {
    *value = number * 360.0;
    return kAngle;
  }
  return kNotANumber;
}

// The byte a channel contributes: percentages scale the 0..255 range, and the
// result is clamped before it is rounded, as the CSS colour functions are.
uint8_t ToByte(ChannelKind kind, double value) {
  if (kind == kPercent) value = value * 255.0 / 100.0;
  if (value < 0.0) value = 0.0;
  if (value > 255.0) value = 255.0;
  return uint8_t(std::lround(value));
}

uint8_t AlphaToByte(ChannelKind kind, double value) {
  if (kind == kPercent) value /= 100.0;
  if (!(value > 0.0)) return 0;
  if (value > 1.0) value = 1.0;
  return uint8_t(std::lround(value * 255.0));
}

// The channels of "a, b, c[, d]" and of "a b c [/ d]": a legacy colour uses
// commas, the space form may carry the alpha after a slash.
bool SplitChannels(const std::string& body, std::vector<std::string>* channels,
                   std::string* alpha) {
  std::vector<std::string> slash = CssSplit(body, '/');
  if (slash.size() > 2) return false;
  if (slash.size() == 2) {
    if (slash[1].empty()) return false;
    *alpha = slash[1];
  }
  std::vector<std::string> comma = CssSplit(slash[0], ',');
  if (comma.size() > 1) {
    *channels = comma;
    return true;
  }
  for (size_t p = 0; p < slash[0].size();) {
    p = CssSkip(slash[0], p);
    if (p == slash[0].size()) break;
    size_t end = p;
    while (end < slash[0].size() && !CssSpace(slash[0][end])) ++end;
    channels->push_back(slash[0].substr(p, end - p));
    p = end;
  }
  return true;
}

// The hue helpers of the reference implementation: the hue is kept in degrees
// and normalised the same way, so half-way channels round to the same byte.
float NormalizeHue(float hue) { return hue - 360.0f * std::floor(hue / 360.0f); }

float HueToRgb(float first, float second, float hue) {
  const float h = NormalizeHue(hue);
  if (h * 6.0f < 360.0f) return first + (second - first) * h / 60.0f;
  if (h * 2.0f < 360.0f) return second;
  if (h * 3.0f < 720.0f) return first + (second - first) * (240.0f - h) / 60.0f;
  return first;
}

// The hue/saturation/lightness form as sRGB; |saturation| and |lightness| are
// fractions of 100%.
void HslToByte(float hue, float saturation, float lightness, uint8_t rgba[3]) {
  const float s = saturation < 0.0f ? 0.0f : saturation > 1.0f ? 1.0f : saturation;
  const float l = lightness < 0.0f ? 0.0f : lightness > 1.0f ? 1.0f : lightness;
  const float second = l <= 0.5f ? l * (s + 1.0f) : l + s - l * s;
  const float first = l * 2.0f - second;
  const float channels[3] = {HueToRgb(first, second, hue + 120.0f), HueToRgb(first, second, hue),
                             HueToRgb(first, second, hue - 120.0f)};
  for (int i = 0; i < 3; ++i) {
    const float value = channels[i] < 0.0f ? 0.0f : channels[i] > 1.0f ? 1.0f : channels[i];
    rgba[i] = uint8_t(std::lround(value * 255.0f));
  }
}

bool HexColor(const std::string& digits, uint8_t rgba[4]) {
  if (digits.size() != 3 && digits.size() != 4 && digits.size() != 6 && digits.size() != 8)
    return false;
  int values[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  for (size_t i = 0; i < digits.size(); ++i) {
    const char c = digits[i];
    if (c >= '0' && c <= '9') values[i] = c - '0';
    else if (c >= 'a' && c <= 'f') values[i] = c - 'a' + 10;
    else return false;
  }
  if (digits.size() <= 4) {
    // #rgb and #rgba repeat each digit.
    for (size_t i = 0; i < digits.size(); ++i) values[i] *= 17;
    rgba[0] = uint8_t(values[0]);
    rgba[1] = uint8_t(values[1]);
    rgba[2] = uint8_t(values[2]);
    rgba[3] = digits.size() == 4 ? uint8_t(values[3]) : 255;
    return true;
  }
  // #rrggbb and #rrggbbaa keep every byte as its own digit pair.
  for (size_t i = 0; i < 3; ++i) rgba[i] = uint8_t(values[i * 2] * 16 + values[i * 2 + 1]);
  rgba[3] = digits.size() == 8 ? uint8_t(values[6] * 16 + values[7]) : 255;
  return true;
}
}  // namespace

bool CssColorRgba(std::string_view text, uint8_t rgba[4]) {
  const std::string value = CssLower(CssClean(text));
  if (value.empty()) return false;
  if (value[0] == '#') return HexColor(value.substr(1), rgba);
  if (CssColorNameRgba(value, rgba)) return true;
  const size_t open = value.find('(');
  if (open == std::string::npos || value.back() != ')') return false;
  const std::string name = value.substr(0, open);
  const std::string body = value.substr(open + 1, value.size() - open - 2);
  std::vector<std::string> channels;
  std::string alpha;
  if (!SplitChannels(body, &channels, &alpha)) return false;
  // The legacy form may carry the alpha as its fourth comma argument.
  if (channels.size() == 4 && alpha.empty()) {
    alpha = channels[3];
    channels.resize(3);
  }
  if (channels.size() != 3) return false;
  rgba[3] = 255;
  if (!alpha.empty()) {
    double number = 0.0;
    const ChannelKind kind = ReadChannel(alpha, false, &number);
    if (kind == kNotANumber || kind == kAngle) return false;
    rgba[3] = AlphaToByte(kind, number);
  }
  if (name == "rgb" || name == "rgba") {
    for (int i = 0; i < 3; ++i) {
      double number = 0.0;
      const ChannelKind kind = ReadChannel(channels[size_t(i)], false, &number);
      if (kind == kNotANumber || kind == kAngle) return false;
      rgba[i] = ToByte(kind, number);
    }
    return true;
  }
  if (name == "hsl" || name == "hsla") {
    double hue = 0.0, saturation = 0.0, lightness = 0.0;
    const ChannelKind hueKind = ReadChannel(channels[0], true, &hue);
    const ChannelKind satKind = ReadChannel(channels[1], false, &saturation);
    const ChannelKind litKind = ReadChannel(channels[2], false, &lightness);
    if (hueKind == kNotANumber || satKind == kNotANumber || litKind == kNotANumber) return false;
    uint8_t rgb[3] = {0, 0, 0};
    HslToByte(float(hue), float(saturation / 100.0), float(lightness / 100.0), rgb);
    for (int i = 0; i < 3; ++i) rgba[i] = rgb[i];
    return true;
  }
  return false;
}
}  // namespace ruyijs::domain::styling
