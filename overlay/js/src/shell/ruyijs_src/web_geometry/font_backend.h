#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef _WIN32
#include <windows.h>
#include <dwrite.h>
#include <wrl/client.h>
#endif
#include <hb.h>
#include <hb-ot.h>
#include <algorithm>
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include "web_geometry/system_font.h"

namespace ruyijs::web_geometry {
#ifdef _WIN32
template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;
struct SelectedFont {
  ComPtr<IDWriteFontFace> face;
  std::wstring family;
  unsigned weight = 400;
  bool italic = false, syntheticItalic = false;
};
// Lower-case family names the system collection does not list: substitutes map
// to another family name, legacy Win32 family names map to their faces.
struct FontAliases {
  std::unordered_map<std::wstring, std::wstring> substitutes;
  std::unordered_map<std::wstring, std::vector<ComPtr<IDWriteFont>>> faces;
};
struct FontCollection {
  ComPtr<IDWriteFactory> factory;
  std::unique_ptr<FontAliases> aliases;  // Read once, on the first unknown family.
  const FontAliases& Aliases();
  ComPtr<IDWriteFontCollection> fonts;
  std::vector<SelectedFont> selected;  // Maximum 32; no JS or layout results retained.
  // Memos of pure lookups (both bounded): the preference-expanded family list
  // per (family, language), and the platform fallback face per (codepoint,
  // weight, italic). Neither depends on page state; the system collection is
  // fixed for the process.
  std::unordered_map<std::string, std::vector<std::wstring>> families;
  std::unordered_map<uint64_t, SelectedFont> fallbacks;
  uint64_t config_generation = 0;  // font_config.h generation the memos were built under.
  bool Initialize();
  // `missing` distinguishes an exhausted character lookup from backend errors.
  bool Select(const FontInput&, uint32_t codepoint, SelectedFont*, bool* missing = nullptr);
};
ComPtr<IDWriteFont> AliasFace(const std::vector<ComPtr<IDWriteFont>>&, unsigned weight, bool italic);
#else
// POSIX v1: canvas glyph rasterization degrades. Inert shape twins keep
// every includer compiling; each font entry point reports "no font" and
// the canvas text paths tolerate empty selections.
template<class T>
struct ComPtr {
  T* ptr_ = nullptr;
  ComPtr() = default;
  T* Get() const { return ptr_; }
  T* operator->() const { return ptr_; }
  void Reset(T* value = nullptr) { ptr_ = value; }
};
struct IDWriteFontFace { unsigned dummy = 0; };
struct IDWriteFont { unsigned dummy = 0; };
struct IDWriteFactory { unsigned dummy = 0; };
struct IDWriteFactory2 { unsigned dummy = 0; };
struct IDWriteFontCollection { unsigned dummy = 0; };
struct SelectedFont {
  ComPtr<IDWriteFontFace> face;
  std::string family;
  unsigned weight = 400;
  bool italic = false, syntheticItalic = false;
};
struct FontAliases {
  std::unordered_map<std::string, std::string> substitutes;
  std::unordered_map<std::string, std::vector<ComPtr<IDWriteFont>>> faces;
};
ComPtr<IDWriteFont> AliasFace(const std::vector<ComPtr<IDWriteFont>>&, unsigned weight, bool italic);
// Memos of pure lookups (both bounded): the preference-expanded family list
// per (family, language), and the platform fallback face per (codepoint,
// weight, italic). Neither depends on page state; the system collection is
// fixed for the process.
struct FontCollection {
  ComPtr<IDWriteFactory> factory;
  std::unique_ptr<FontAliases> aliases;  // Read once, on the first unknown family.
  const FontAliases& Aliases();
  ComPtr<IDWriteFontCollection> fonts;
  std::vector<SelectedFont> selected;  // Maximum 32; no JS or layout results retained.
  std::unordered_map<std::string, std::vector<std::string>> families;
  std::unordered_map<uint64_t, SelectedFont> fallbacks;
  uint64_t config_generation = 0;  // font_config.h generation the memos were built under.
  bool Initialize();
  // `missing` distinguishes an exhausted character lookup from backend errors.
  bool Select(const FontInput&, uint32_t codepoint, SelectedFont*, bool* missing = nullptr);
};
#endif
// A rectangle with the semantics of the gfxRect Firefox measures text with:
// Union() recomputes the extents from the corners, which is why a reported ink
// edge is not exactly the last glyph's edge.
struct InkRect {
  double x = 0, y = 0, width = 0, height = 0;
  bool empty() const { return width <= 0 || height <= 0; }
  double x_most() const { return x + width; }
  double y_most() const { return y + height; }
  void Union(const InkRect& other) {
    if (empty()) { *this = other; return; }
    if (other.empty()) return;
    const double left = std::min(x, other.x), top = std::min(y, other.y);
    width = std::max(x_most(), other.x_most()) - left;
    height = std::max(y_most(), other.y_most()) - top;
    x = left;
    y = top;
  }
  void Scale(double factor) { x *= factor; y *= factor; width *= factor; height *= factor; }
};
struct FontRun {
  ruyijs::domain::geometry::SvgBox box{};
  std::array<double, 4> unrounded{};
  std::vector<int> advances, clusters;
  double ascent = 0, descent = 0;
  // The tight ink box of the run in app units (1/60 CSS pixel), relative to the
  // run origin on the alphabetic baseline, as gfxTextRun accumulates the glyph
  // bounds gfxDWriteFont::GetGlyphBounds reports. Empty when no glyph has ink.
  InkRect ink;
  // Shaped glyph data used by canvas painting. Values are in scaled DIPs;
  // the canvas path adapter divides them by FontInput::scale.
  std::vector<uint16_t> glyphs;
  std::vector<float> glyph_advances, glyph_offsets_x, glyph_offsets_y;
  // The HarfBuzz 16.16 advances, offsets and clusters, kept so the canvas can
  // round them the way gfxHarfBuzzShaper does (with its carried residual).
  std::vector<int32_t> raw_advances, raw_offsets_x, raw_offsets_y;
  std::vector<int> glyph_clusters;
  int advance = 0;
};
#ifdef _WIN32
std::vector<std::wstring> FontFamilies(const FontInput&);
#else
std::vector<std::string> FontFamilies(const FontInput&);
#endif
bool ShapeFont(const SelectedFont&, const FontInput&, const std::u16string&, FontRun*);
bool CanvasFontBaselines(const SelectedFont&, const FontInput&, CanvasTextMetrics*);
bool MissingFontRun(const SelectedFont&, const FontInput&, uint32_t codepoint,
                    size_t units, FontRun*);
}
