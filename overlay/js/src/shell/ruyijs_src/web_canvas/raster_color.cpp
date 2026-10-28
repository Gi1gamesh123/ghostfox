// The paint of one fill: the recorded fillStyle string becomes a colour, is
// scaled by globalAlpha and premultiplied the way gfx/2d hands it to Skia, and
// the globalCompositeOperation selects the Skia blend mode.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "domain/styling/css_color_value.h"
#include "web_canvas/raster_color.h"

namespace ruyijs::web_canvas::raster {
bool ParseCSSColor(const std::string& style, uint8_t rgba[4]) {
  // The styling owner owns the CSS colour grammar and its values; the canvas
  // only hands the string over and keeps the bytes.
  return domain::styling::CssColorRgba(style, rgba);
}

// nsStyleUtil::ColorComponentToFloat: the alpha a canvas reports, at two
// decimals when that round-trips and at three when it does not.
float ColorComponentToFloat(uint8_t alpha) {
  float rounded = std::round(float(alpha) * 100.0f / 255.0f) / 100.0f;
  const auto toComponent = [](float value) { return uint8_t(std::round(value * 255.0f)); };
  if (toComponent(rounded) != alpha) rounded = std::round(float(alpha) * 1000.0f / 255.0f) / 1000.0f;
  return rounded;
}

std::string CanvasColorStyle(const uint8_t rgba[4]) {
  if (rgba[3] == 255) {
    char text[8];
    std::snprintf(text, sizeof text, "#%02x%02x%02x", rgba[0], rgba[1], rgba[2]);
    return text;
  }
  // CanvasRenderingContext2D::StyleColorToString, with the alpha formatted the
  // way nsTSubstring::AppendFloat formats it (six significant digits).
  char alpha[32];
  std::snprintf(alpha, sizeof alpha, "%.6g", double(ColorComponentToFloat(rgba[3])));
  return "rgba(" + std::to_string(rgba[0]) + ", " + std::to_string(rgba[1]) + ", " +
         std::to_string(rgba[2]) + ", " + alpha + ")";
}

bool ParseComposite(const std::string& name, Blend* blend) {
  struct Entry {
    const char* name;
    Blend blend;
  };
  static const Entry kEntries[] = {
      {"source-over", Blend::kSourceOver},   {"source-in", Blend::kSourceIn},
      {"source-out", Blend::kSourceOut},     {"source-atop", Blend::kSourceATop},
      {"destination-over", Blend::kDestinationOver},
      {"destination-in", Blend::kDestinationIn},
      {"destination-out", Blend::kDestinationOut},
      {"destination-atop", Blend::kDestinationATop},
      {"xor", Blend::kXor},                  {"lighter", Blend::kPlus},
      {"copy", Blend::kCopy},                {"multiply", Blend::kMultiply},
      {"screen", Blend::kScreen},            {"overlay", Blend::kOverlay},
      {"darken", Blend::kDarken},            {"lighten", Blend::kLighten},
      {"difference", Blend::kDifference},    {"exclusion", Blend::kExclusion},
      {"hard-light", Blend::kHardLight},
  };
  for (const Entry& entry : kEntries) {
    if (name == entry.name) {
      *blend = entry.blend;
      return true;
    }
  }
  return false;
}

void PremultiplyColor(const uint8_t straight[4], uint8_t out[4]) {
  const unsigned alpha = straight[3];
  for (int i = 0; i < 3; ++i) out[i] = uint8_t((unsigned(straight[i]) * alpha + 127) / 255);
  out[3] = straight[3];
}

bool PaintFromStyle(const std::string& style, double globalAlpha, const std::string& composite,
                    Paint* paint) {
  uint8_t straight[4] = {0, 0, 0, 0};
  if (!ParseCSSColor(style, straight)) return false;
  if (!(globalAlpha >= 0.0)) globalAlpha = 0.0;
  if (globalAlpha > 1.0) globalAlpha = 1.0;
  straight[3] = uint8_t(std::lround(double(straight[3]) * globalAlpha));
  PremultiplyColor(straight, paint->rgba);
  if (!ParseComposite(composite, &paint->blend)) paint->blend = Blend::kSourceOver;
  return true;
}
}  // namespace ruyijs::web_canvas::raster
