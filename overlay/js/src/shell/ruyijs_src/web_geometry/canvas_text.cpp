// Canvas text measurement for the font owner. A 2D context reports other numbers
// than SVG or layout text: the advance as a float of whole app units, the tight
// unrounded ink box of the glyphs, and the rounded font box of the primary font.
// The runs still come from the one shaping owner, so fonts are selected and
// advanced exactly like every other measurement in this domain.
#include "web_geometry/font_backend.h"
#include <algorithm>
#include <cmath>

namespace ruyijs::web_geometry {
namespace {
uint32_t Codepoint(const std::u16string& text, size_t* at) {
  uint32_t code = text[(*at)++];
  if (code >= 0xd800 && code <= 0xdbff && *at < text.size() &&
      text[*at] >= 0xdc00 && text[*at] <= 0xdfff)
    code = 0x10000 + ((code - 0xd800) << 10) + text[(*at)++] - 0xdc00;
  return code;
}

// Combining marks and joiners never start a run of their own.
bool Continuation(uint32_t code) {
  return code == 0x200d || (code >= 0xfe00 && code <= 0xfe0f) ||
      (code >= 0xe0100 && code <= 0xe01ef) ||
      hb_unicode_combining_class(hb_unicode_funcs_get_default(), code) != 0;
}
}  // namespace

bool SystemFonts::MeasureCanvas(const std::u16string& text, const FontInput& input,
                                CanvasTextMetrics* result) {
  *result = {};
  if (text.size() > 65536 || !std::isfinite(input.scale) || input.scale <= 0) return false;
  if (!std::isfinite(input.size) || input.size <= 0) return false;
  if (!collection_) {
    auto collection = std::make_unique<FontCollection>();
    if (!collection->Initialize()) return false;
    collection_ = std::move(collection);
  }
  // The font box is the one Firefox reports for the run's first valid font.
  SelectedFont primary;
  if (!collection_->Select(input, 0x20, &primary)) return false;
  DWRITE_FONT_METRICS metrics{};
  primary.face->GetMetrics(&metrics);
  if (!metrics.designUnitsPerEm) return false;
  // The size in app units, and the same in CSS pixels as Firefox's mAdjustedSize.
  const double app_units = std::round(input.size * input.scale * 60);
  const double size = float(app_units / 60);
  const double em_height = app_units / 60;
  const float factor = float(size / metrics.designUnitsPerEm);
  result->font_ascent = std::round(metrics.ascent * factor);
  result->font_descent = std::round(metrics.descent * factor);
  const double max_height = result->font_ascent + result->font_descent;
  if (max_height > 0) {
    result->em_ascent = em_height * result->font_ascent / max_height;
    result->em_descent = em_height - result->em_ascent;
  }
  if (!CanvasFontBaselines(primary, input, result)) return false;
  int64_t total = 0;
  InkRect ink;
  for (size_t start = 0; start < text.size();) {
    size_t end = start;
    const uint32_t first = Codepoint(text, &end);
    SelectedFont font;
    // A character no font covers is Firefox's missing-glyph box in the primary font.
    bool missing = false;
    if (!collection_->Select(input, first, &font, &missing) &&
        (!missing || !collection_->Select(input, 0x20, &font))) return false;
    while (!missing && end < text.size()) {
      size_t next = end;
      const uint32_t code = Codepoint(text, &next);
      if (!Continuation(code)) {
        SelectedFont candidate;
        bool absent = false;
        if (!collection_->Select(input, code, &candidate, &absent)) {
          if (absent) break;
          return false;
        }
        if (font.face.Get() != candidate.face.Get() ||
            font.syntheticItalic != candidate.syntheticItalic) break;
      }
      end = next;
    }
    FontInput local = input;
    local.x = 0;
    local.y = 0;
    FontRun run;
    if (missing ? !MissingFontRun(font, local, first, end - start, &run)
                : !ShapeFont(font, local, text.substr(start, end - start), &run)) return false;
    InkRect positioned = run.ink;
    positioned.x += double(total);  // the run starts at the accumulated advance
    ink.Union(positioned);
    total += run.advance;
    start = end;
  }
  // Firefox divides the app-unit total by the device ratio in float.
  result->width = float(total) / 60.0f / input.scale;
  result->ink = !ink.empty();
  if (result->ink) {
    // gfxRect::Scale(1.0 / mAppUnitsPerDevPixel) in the canvas processor, then
    // the edges of the scaled rectangle.
    ink.Scale(1.0 / 60.0);
    result->ink_left = ink.x / input.scale;
    result->ink_top = ink.y / input.scale;
    result->ink_right = ink.x_most() / input.scale;
    result->ink_bottom = ink.y_most() / input.scale;
  }
  return true;
}
}  // namespace ruyijs::web_geometry
