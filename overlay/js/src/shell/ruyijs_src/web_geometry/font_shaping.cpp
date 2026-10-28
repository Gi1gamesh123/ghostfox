/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "web_geometry/font_backend.h"
#include "web_geometry/font_tables.h"
#include "domain/geometry/svg_text_projection.h"
#include <algorithm>
#include <cmath>

namespace ruyijs::web_geometry {
namespace {
struct Table { IDWriteFontFace* face; void* context; };
void ReleaseTable(void* data) {
  auto* table = static_cast<Table*>(data);
  table->face->ReleaseFontTable(table->context); table->face->Release(); delete table;
}
hb_blob_t* ReadTable(hb_face_t*, hb_tag_t tag, void* data) {
  auto* face = static_cast<IDWriteFontFace*>(data);
  const void* bytes = nullptr; UINT32 length = 0; void* context = nullptr; BOOL exists = FALSE;
  if (FAILED(face->TryGetFontTable(_byteswap_ulong(tag), &bytes, &length, &context, &exists)) || !exists)
    return hb_blob_get_empty();
  face->AddRef();
  return hb_blob_create(static_cast<const char*>(bytes), length, HB_MEMORY_MODE_READONLY,
                       new Table{face, context}, ReleaseTable);
}
template<class T, void (*Destroy)(T*)> using HbPtr = std::unique_ptr<T, decltype(Destroy)>;
hb_bool_t CanvasExtents(hb_font_t*, void* data, hb_font_extents_t* extents, void*) {
  const auto& metrics = *static_cast<const CanvasTextMetrics*>(data);
  extents->ascender = hb_position_t(metrics.font_ascent * 65536);
  extents->descender = -hb_position_t(metrics.font_descent * 65536);
  extents->line_gap = 0;
  return true;
}
}
bool CanvasFontBaselines(const SelectedFont& selected, const FontInput& input,
                         CanvasTextMetrics* result) {
  // gfxFont::GetBaseline uses the same OpenType BASE lookup and fallback,
  // with gfxHarfBuzzShaper::HBGetHExtents providing rounded font extents.
  const double size = float(std::round(input.size * input.scale * 60) / 60);
  if (!std::isfinite(size) || size < 0 || size > 16384) return false;
  HbPtr<hb_face_t, hb_face_destroy> face(
      hb_face_create_for_tables(ReadTable, selected.face.Get(), nullptr), hb_face_destroy);
  HbPtr<hb_font_t, hb_font_destroy> font(hb_font_create(face.get()), hb_font_destroy);
  hb_ot_font_set_funcs(font.get());
  hb_font_set_scale(font.get(), int(std::round(size * 65536)), int(std::round(size * 65536)));
  hb_font_set_ppem(font.get(), unsigned(size), unsigned(size));
  HbPtr<hb_font_t, hb_font_destroy> measured(hb_font_create_sub_font(font.get()), hb_font_destroy);
  HbPtr<hb_font_funcs_t, hb_font_funcs_destroy> funcs(hb_font_funcs_create(), hb_font_funcs_destroy);
  hb_font_funcs_set_font_h_extents_func(funcs.get(), CanvasExtents, nullptr, nullptr);
  hb_font_set_funcs(measured.get(), funcs.get(), result, nullptr);
  const hb_ot_layout_baseline_tag_t tags[] = {HB_OT_LAYOUT_BASELINE_TAG_HANGING,
      HB_OT_LAYOUT_BASELINE_TAG_ROMAN, HB_OT_LAYOUT_BASELINE_TAG_IDEO_EMBOX_BOTTOM_OR_LEFT};
  double* values[] = {&result->hanging_baseline, &result->alphabetic_baseline,
                     &result->ideographic_baseline};
  for (size_t index = 0; index < 3; ++index) {
    hb_position_t position = 0;
    hb_ot_layout_get_baseline_with_fallback(measured.get(), tags[index], HB_DIRECTION_LTR,
        HB_OT_TAG_DEFAULT_SCRIPT, HB_OT_TAG_DEFAULT_LANGUAGE, &position);
    *values[index] = position / 65536.0;
  }
  return true;
}
bool ShapeFont(const SelectedFont& selected, const FontInput& input,
               const std::u16string& text, FontRun* result) {
  auto* face = selected.face.Get();
  DWRITE_FONT_METRICS metrics{}; face->GetMetrics(&metrics);
  if (!metrics.designUnitsPerEm) return false;
  HbPtr<hb_face_t, hb_face_destroy> hbface(hb_face_create_for_tables(ReadTable, face, nullptr), hb_face_destroy);
  HbPtr<hb_font_t, hb_font_destroy> font(hb_font_create(hbface.get()), hb_font_destroy);
  hb_ot_font_set_funcs(font.get());
  const double size = float(std::round(input.size * input.scale * 60) / 60);
  if (!std::isfinite(size) || size < 0 || size > 16384) return false;
  hb_font_set_scale(font.get(), int(std::round(size * 65536)), int(std::round(size * 65536)));
  hb_font_set_ppem(font.get(), unsigned(size), unsigned(size));
  const bool bitmap = CJKFont(hbface.get()) && BitmapSize(hbface.get(), unsigned(std::round(size)));
  HbPtr<hb_font_t, hb_font_destroy> shaping(bitmap ? hb_font_create_sub_font(font.get()) :
                                        hb_font_reference(font.get()), hb_font_destroy);
  BitmapAdvance bitmapAdvance{face, float(size), float(size / metrics.designUnitsPerEm)};
  if (bitmap) {
    auto* functions = hb_font_funcs_create();
    hb_font_funcs_set_glyph_h_advance_func(functions, GdiAdvance, nullptr, nullptr);
    hb_font_set_funcs(shaping.get(), functions, &bitmapAdvance, nullptr);
    hb_font_funcs_destroy(functions);
  }
  HbPtr<hb_buffer_t, hb_buffer_destroy> buffer(hb_buffer_create(), hb_buffer_destroy);
  std::vector<hb_glyph_info_t> info;
  std::vector<hb_glyph_position_t> positions;
  for (size_t start = 0; start < text.size();) {
    size_t end = start+1;
    if (text[start] != u' ') while (end < text.size() && text[end] != u' ') ++end;
    hb_buffer_clear_contents(buffer.get());
    hb_buffer_add_utf16(buffer.get(), reinterpret_cast<const uint16_t*>(text.data()+start),
                        int(end-start), 0, int(end-start));
    hb_buffer_guess_segment_properties(buffer.get());
    hb_buffer_set_cluster_level(buffer.get(), HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES);
    hb_shape(shaping.get(), buffer.get(), nullptr, 0);
    if (!hb_buffer_allocation_successful(buffer.get())) return false;
    unsigned count = 0;
    const auto* glyphs = hb_buffer_get_glyph_infos(buffer.get(), &count);
    const auto* offsets = hb_buffer_get_glyph_positions(buffer.get(), nullptr);
    for (unsigned i = 0; i < count; ++i) {
      auto glyph = glyphs[i]; glyph.cluster += unsigned(start);
      info.push_back(glyph); positions.push_back(offsets[i]);
    }
    start = end;
  }
  const float conv = float(size / metrics.designUnitsPerEm);
  // Canvas ink extents come from the size in app units, in double.
  const double ink_unit = (std::round(size * 60.0) / 60.0) / metrics.designUnitsPerEm;
  result->ascent = std::round(metrics.ascent * conv) * 60;
  result->descent = std::round(metrics.descent * conv) * 60;
  ruyijs::domain::geometry::InkBox ink{0, -result->ascent, 0, result->descent};
  result->glyphs.clear(); result->glyph_advances.clear();
  result->glyph_offsets_x.clear(); result->glyph_offsets_y.clear();
  result->glyphs.reserve(info.size()); result->glyph_advances.reserve(info.size());
  result->glyph_offsets_x.reserve(info.size()); result->glyph_offsets_y.reserve(info.size());
  result->raw_advances.clear(); result->raw_offsets_x.clear(); result->raw_offsets_y.clear();
  result->glyph_clusters.clear();
  result->advances.assign(text.size(), 0); result->clusters.assign(text.size(), -1);
  for (size_t i = 0; i < info.size(); ++i) {
    const auto cluster = info[i].cluster;
    if (cluster >= text.size()) return false;
    int width = int(std::floor(positions[i].x_advance * 60.0 / 65536 + .5));
    if (info[i].codepoint > 0xffff) return false;
    const UINT16 glyph = UINT16(info[i].codepoint);
    DWRITE_GLYPH_METRICS gm{};
    if (FAILED(face->GetDesignGlyphMetrics(&glyph, 1, &gm))) return false;
    if (text[cluster] == u' ' && !bitmap) {
      const int space = int(std::floor(float(gm.advanceWidth * float(size / metrics.designUnitsPerEm) * 60.0) + .5));
      if (space <= 0xfff) width = space;
    }
    result->glyphs.push_back(glyph);
    result->raw_advances.push_back(positions[i].x_advance);
    result->raw_offsets_x.push_back(positions[i].x_offset);
    result->raw_offsets_y.push_back(positions[i].y_offset);
    result->glyph_clusters.push_back(int(cluster));
    result->glyph_advances.push_back(float(width) / 60.0f);
    result->glyph_offsets_x.push_back(float(positions[i].x_offset) / 65536.0f);
    result->glyph_offsets_y.push_back(float(positions[i].y_offset) / 65536.0f);
    result->advances[cluster] += width;
    result->clusters[cluster] = int(cluster);
    const double unit = size / metrics.designUnitsPerEm * 60;
    ink.Include(result->advance + positions[i].x_offset * 60.0 / 65536 + gm.leftSideBearing * unit,
        -positions[i].y_offset * 60.0 / 65536 + (gm.topSideBearing - gm.verticalOriginY) * unit,
        (double(gm.advanceWidth)-gm.leftSideBearing-gm.rightSideBearing) * unit,
        (double(gm.advanceHeight)-gm.topSideBearing-gm.bottomSideBearing) * unit);
    // The same glyph bounds as canvas text metrics report them: the tight rect
    // gfxRect(lsb, tsb - voy, w, h) scaled by the size in app units over the
    // design units per em, in app units like the run's own pen positions. The
    // scaling stays in double so an ink edge is the exact product Firefox gets.
    const double origin_x = result->advance + positions[i].x_offset * 60.0 / 65536;
    const double origin_y = -positions[i].y_offset * 60.0 / 65536;
    InkRect bounds;
    bounds.x = origin_x + gm.leftSideBearing * ink_unit * 60.0;
    bounds.y = origin_y + (gm.topSideBearing - gm.verticalOriginY) * ink_unit * 60.0;
    bounds.width = (double(gm.advanceWidth) - gm.leftSideBearing - gm.rightSideBearing) *
                   ink_unit * 60.0;
    bounds.height = (double(gm.advanceHeight) - gm.topSideBearing - gm.bottomSideBearing) *
                    ink_unit * 60.0;
    result->ink.Union(bounds);
    result->advance += width;
  }
  for (size_t i = 1; i < text.size(); ++i)
    if (result->clusters[i] < 0) result->clusters[i] = result->clusters[i-1];
  ink.right = std::max(ink.right, double(result->advance));
  const auto measurement = ruyijs::domain::geometry::ProjectInk(ink, result->advance, 0,
      result->ascent, result->descent, input.scale, selected.syntheticItalic, input.x, input.y);
  std::copy_n(measurement.box, 4, result->box.begin());
  std::copy_n(measurement.unrounded, 4, result->unrounded.begin());
  return true;
}
}
