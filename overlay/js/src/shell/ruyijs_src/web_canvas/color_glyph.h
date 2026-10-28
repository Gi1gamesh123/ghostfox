#ifndef RUYIJS_WEB_CANVAS_COLOR_GLYPH_H
#define RUYIJS_WEB_CANVAS_COLOR_GLYPH_H
#include <cstdint>

#include "web_canvas/raster_path.h"
#include "web_geometry/glyph_mask.h"

namespace ruyijs::web_canvas::raster {
// color_glyph.cpp: paints one COLR glyph the way gfxFont::RenderColorGlyph does
// on a Skia canvas. |matrix| is the text transform, |global| the globalAlpha
// byte (DrawOptions::mAlpha) and |group| the opacity gfxTextRun::Draw's
// BufferAlphaColor group blends the colour-font run back with.
void DrawColorGlyph(Surface surface, const web_geometry::CanvasGlyphMask& mask,
                    const float matrix[6], uint8_t global, uint8_t group);
}  // namespace ruyijs::web_canvas::raster
#endif
