#include "web_geometry/font_backend.h"
#include <algorithm>
#include <cmath>

namespace ruyijs::web_geometry {
SystemFonts::SystemFonts() = default;
SystemFonts::~SystemFonts() = default;
namespace {
std::u16string Normalize(const std::u16string& text) {
  std::u16string result;
  bool space = true;
  for (char16_t ch : text) {
    const bool current = ch == u' ' || ch == u'\n' || ch == u'\t' || ch == u'\r';
    if (!current || !space) result += current ? u' ' : ch;
    space = current;
  }
  return result;
}
uint32_t Codepoint(const std::u16string& text, size_t* at) {
  uint32_t code = text[(*at)++];
  if (code >= 0xd800 && code <= 0xdbff && *at < text.size() &&
      text[*at] >= 0xdc00 && text[*at] <= 0xdfff)
    code = 0x10000 + ((code-0xd800)<<10) + text[(*at)++]-0xdc00;
  return code;
}
bool Continuation(uint32_t code) {
  return code == 0x200d || (code >= 0xfe00 && code <= 0xfe0f) ||
      (code >= 0xe0100 && code <= 0xe01ef) ||
      hb_unicode_combining_class(hb_unicode_funcs_get_default(), code) != 0;
}
}
bool SystemFonts::LineMetrics(const FontInput& input, double* content, double* normal) {
  *content = *normal = 0;
  if (!std::isfinite(input.size) || input.size <= 0) return true;
  if (!collection_) {
    auto collection = std::make_unique<FontCollection>();
    if (!collection->Initialize()) return false;
    collection_ = std::move(collection);
  }
  SelectedFont font;
  if (!collection_->Select(input, 0x20, &font)) return false;
#ifdef _WIN32
  DWRITE_FONT_METRICS metrics{}; font.face->GetMetrics(&metrics);
  if (!metrics.designUnitsPerEm) return false;
  const double size = float(std::round(input.size * 60) / 60);
  const float factor = float(size / metrics.designUnitsPerEm);
  const double height = std::round(metrics.ascent * factor) + std::round(metrics.descent * factor);
  // nsFontMetrics rounds each metric to whole pixels; GetNormalLineHeight uses
  // 1.2em only when the font has neither internal nor external leading.
  const double em = std::floor(size + .5);
  const double internal = std::floor(std::max(height - size, 0.0) + .5);
  const double external = std::floor(std::ceil(metrics.lineGap * factor) + .5);
  *content = height;
  *normal = internal == 0 && external == 0 ? std::floor(float(em * 60) * 1.2f + .5f) / 60
                                           : em + internal + external;
#endif
  return true;
}
bool SystemFonts::Measure(const std::u16string& source, const FontInput& input,
                          ruyijs::domain::geometry::SvgTextMeasurement* result) {
  *result = {};
  if (source.size() > 65536 || !std::isfinite(input.scale) || input.scale <= 0) return false;
  const auto text = Normalize(source);
  if (text.empty() || input.size == 0) return true;
  if (!collection_) {
    auto collection = std::make_unique<FontCollection>();
    if (!collection->Initialize()) return false;
    collection_ = std::move(collection);
  }
  const size_t visible = text.size() - (text.back() == u' ' ? 1 : 0);
  result->characters.resize(visible);
  ruyijs::domain::geometry::SvgBounds bounds, transformed;
  int total = 0, trailing = 0;
  size_t start = 0;
  while (start < text.size()) {
    size_t end = start;
    const auto first = Codepoint(text, &end);
    SelectedFont font;
    bool missing = false;
    if (!collection_->Select(input, first, &font, &missing) &&
        (!missing || !collection_->Select(input, 0x20, &font))) return false;
    while (!missing && end < text.size()) {
      size_t next = end;
      const auto code = Codepoint(text, &next);
      SelectedFont candidate;
      if (!Continuation(code)) {
        bool absent = false;
        if (!collection_->Select(input, code, &candidate, &absent)) {
          if (absent) break;
          return false;
        }
        if (font.face.Get() != candidate.face.Get() || font.syntheticItalic != candidate.syntheticItalic) break;
      }
      end = next;
    }
    FontInput local = input;
    local.x += total / 60.0 / input.scale;
    FontRun run;
    if (missing ? !MissingFontRun(font, local, first, end-start, &run)
                : !ShapeFont(font, local, text.substr(start, end-start), &run)) return false;
    bounds.Include(run.box);
    transformed.Include(input.transform.ApplyUnrounded(run.unrounded));
    int advance = 0;
    std::vector<int> offsets(run.advances.size());
    for (size_t i = 0; i < offsets.size(); ++i) { offsets[i] = advance; advance += run.advances[i]; }
    const float inverse = 1 / input.scale;
    for (size_t i = start; i < std::min(end, visible); ++i) {
      const int cluster = run.clusters[i-start];
      if (cluster < 0) continue;
      result->characters[i] = {float(input.x + (total+offsets[cluster])/60.0*inverse),
        float(input.y-run.ascent/60*inverse), float(run.advances[cluster]/60.0*inverse),
        float((run.ascent+run.descent)/60*inverse)};
    }
    if (end == text.size() && visible < text.size()) trailing = run.advances.back();
    total += run.advance; start = end;
  }
  result->box = bounds.box;
  result->transformed = transformed.box;
  result->length = float((total-trailing)/60.0/input.scale);
  return true;
}
}
