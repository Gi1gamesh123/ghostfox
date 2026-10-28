#ifndef RUYIJS_WEB_CANVAS_RASTER_COLOR_H
#define RUYIJS_WEB_CANVAS_RASTER_COLOR_H
// fillStyle/globalCompositeOperation/globalAlpha become the paint of a fill.
#include <cstdint>
#include <string>

#include "web_canvas/raster_path.h"

namespace ruyijs::web_canvas::raster {
// A CSS colour: the styling owner resolves it (#rgb, #rgba, #rrggbb,
// #rrggbbaa, rgb()/rgba(), hsl()/hsla(), the colour names, transparent and
// currentcolor). Straight (non-premultiplied) bytes.
bool ParseCSSColor(const std::string& style, uint8_t rgba[4]);
// The same colour as the string a canvas attribute reports: "#rrggbb" for an
// opaque colour and "rgba(r, g, b, a)" otherwise.
std::string CanvasColorStyle(const uint8_t rgba[4]);
bool ParseComposite(const std::string& name, Blend* blend);
// Skia's premultiply for an 8-bit colour.
void PremultiplyColor(const uint8_t straight[4], uint8_t out[4]);
// The whole paint: colour, globalAlpha scaled in and premultiplied.
bool PaintFromStyle(const std::string& style, double globalAlpha, const std::string& composite,
                    Paint* paint);
}  // namespace ruyijs::web_canvas::raster
#endif
