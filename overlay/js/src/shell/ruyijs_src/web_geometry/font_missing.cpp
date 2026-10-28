/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// Layout missing-glyph metrics: gfxShapedText::SetMissingGlyph,
// gfxFontMissingGlyphs::GetDesiredMinWidth and gfxDWriteFont::ComputeMetrics.
#include "web_geometry/font_backend.h"
#include "web_geometry/font_tables.h"
#include "domain/geometry/svg_text_projection.h"
#include "mozilla/intl/UnicodeProperties.h"
#include <cmath>

namespace ruyijs::web_geometry {
bool MissingFontRun(const SelectedFont& font, const FontInput& input, uint32_t code,
                    size_t units, FontRun* result) {
  const double size = float(std::round(input.size * input.scale * 60) / 60);
  if (!std::isfinite(size) || size < 0 || size > 16384 || !units) return false;
  double average, ascent, descent;
#ifdef _WIN32
  DWRITE_FONT_METRICS metrics{};
  font.face->GetMetrics(&metrics);
  if (!metrics.designUnitsPerEm) return false;
  const float factor = float(size / metrics.designUnitsPerEm);
  average = 0;
  const void* data = nullptr; UINT32 length = 0; void* context = nullptr; BOOL exists = FALSE;
  if (SUCCEEDED(font.face->TryGetFontTable(_byteswap_ulong(HB_TAG('O','S','/','2')),
      &data, &length, &context, &exists)) && exists) {
    if (length >= 4) average = int16_t(Big16(static_cast<const uint8_t*>(data) + 2)) * factor;
    font.face->ReleaseFontTable(context);
  }
  if (average < 1) {
    FontRun x;
    if (!ShapeFont(font, input, u"x", &x)) return false;
    average = x.advance / 60.0;
    if (average < 1) average = metrics.xHeight * factor;
  }
  ascent = std::round(metrics.ascent * factor) * 60;
  descent = std::round(metrics.descent * factor) * 60;
#else
  // POSIX v1: no font backend metrics; a neutral 0.8/0.2 box, 0.5em advance.
  (void)font;
  average = size * 0.5;
  ascent = std::round(size * 0.8) * 60;
  descent = std::round(size * 0.2) * 60;
#endif
  // Two hex columns for BMP, three for supplementary code points. The donor's
  // border/inset/gap constants produce 13/17 CSS pixels, independent of size.
  const double minimum = (code < 0x10000 ? 13.0 : 17.0) * input.scale;
  using Unicode = mozilla::intl::UnicodeProperties;
  const bool ignorable = Unicode::HasBinaryProperty(code,
      Unicode::BinaryProperty::DefaultIgnorableCodePoint) || code == 0x200c || code == 0x200d;
  const int advance = ignorable ? 0 : int(std::max(average, minimum) * 60);
  *result = {};
  result->advance = advance;
  result->advances.assign(units, 0);
  result->advances[0] = advance;
  result->clusters.assign(units, 0);
  result->ascent = ascent;
  result->descent = descent;
  const ruyijs::domain::geometry::InkBox ink{
      0, -result->ascent, double(advance), result->descent};
  const auto measurement = ruyijs::domain::geometry::ProjectInk(ink, advance, advance,
      result->ascent, result->descent, input.scale, false, input.x, input.y);
  std::copy_n(measurement.box, 4, result->box.begin());
  std::copy_n(measurement.unrounded, 4, result->unrounded.begin());
  // gfxFont::Measure: a missing glyph's ink is its advance by the font box.
  if (advance > 0) result->ink = {0, -result->ascent, double(advance), result->ascent + result->descent};
  return true;
}
}
