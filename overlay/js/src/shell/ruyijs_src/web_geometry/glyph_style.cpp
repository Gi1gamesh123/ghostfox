// SkScalerContext_DW's choice of rendering mode, reduced to the case the canvas
// asks for: a grayscale A8 mask, normal hinting, no embedded bitmaps. The
// donor's bitmap branches therefore collapse, leaving the size test, the
// typeface's own mode, the gasp table and the hinting flag.
#include "web_geometry/glyph_mask.h"

#include <cmath>

#include "web_geometry/font_backend.h"

namespace ruyijs::web_geometry {
namespace {
bool Table(IDWriteFontFace* face, uint32_t tag, const void** data, uint32_t* bytes,
           void** context) {
  BOOL exists = FALSE;
  *data = nullptr;
  *bytes = 0;
  *context = nullptr;
  return SUCCEEDED(face->TryGetFontTable(tag, data, bytes, context, &exists)) && exists;
}
uint16_t Be16(const uint8_t* p) { return uint16_t((p[0] << 8) | p[1]); }

// SkOTTableGridAndScanProcedure: the range holding |ppem| and its version.
bool GaspRange(IDWriteFontFace* face, int ppem, uint16_t* flags, bool* version1) {
  const void* data = nullptr;
  uint32_t bytes = 0;
  void* context = nullptr;
  if (!Table(face, DWRITE_MAKE_OPENTYPE_TAG('g', 'a', 's', 'p'), &data, &bytes, &context))
    return false;
  const auto* table = static_cast<const uint8_t*>(data);
  bool found = false;
  if (bytes >= 4) {
    const uint16_t version = Be16(table), ranges = Be16(table + 2);
    if ((version == 0 || version == 1) && ranges <= 1024 && bytes >= 4 + 4u * ranges) {
      int min = -1;
      for (uint16_t i = 0; i < ranges && !found; ++i) {
        const uint8_t* range = table + 4 + 4u * i;
        const int max = Be16(range);
        if (min < ppem && ppem <= max) {
          *flags = Be16(range + 2);
          *version1 = version == 1;
          found = true;
        }
        min = max;
      }
    }
  }
  face->ReleaseFontTable(context);
  return found;
}

// is_hinted: a TrueType font that carries instructions, decided from maxp.
bool IsHinted(IDWriteFontFace* face) {
  const void* data = nullptr;
  uint32_t bytes = 0;
  void* context = nullptr;
  if (!Table(face, DWRITE_MAKE_OPENTYPE_TAG('m', 'a', 'x', 'p'), &data, &bytes, &context))
    return false;
  const auto* table = static_cast<const uint8_t*>(data);
  const bool hinted = bytes >= 28 && table[0] == 0 && table[1] == 1 && table[2] == 0 &&
                      table[3] == 0 && Be16(table + 26) != 0;
  face->ReleaseFontTable(context);
  return hinted;
}

// The system text settings gfxDWriteFont::UpdateClearTypeVars reads, minus the
// prefs the canvas never sees a page set: the platform rendering parameters,
// with the registry override only the ClearType tuner writes.
struct TextSettings {
  float gamma = 2.2f, contrast = 1.0f;
  int renderingMode = 0;  // DWRITE_RENDERING_MODE_DEFAULT
};
const TextSettings& Settings() {
  static const TextSettings settings = [] {
    TextSettings value;
    ComPtr<IDWriteFactory> factory;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                   reinterpret_cast<IUnknown**>(factory.GetAddressOf()))))
      return value;
    ComPtr<IDWriteRenderingParams> params;
    if (FAILED(factory->CreateRenderingParams(params.GetAddressOf())) || !params) return value;
    value.gamma = params->GetGamma();
    value.renderingMode = int(params->GetRenderingMode());
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Avalon.Graphics\\DISPLAY1", 0,
                      KEY_READ, &key) == ERROR_SUCCESS) {
      if (RegQueryValueExW(key, L"EnhancedContrastLevel", nullptr, nullptr, nullptr, nullptr) ==
          ERROR_SUCCESS)
        value.contrast = params->GetEnhancedContrast();
      RegCloseKey(key);
    }
    return value;
  }();
  return settings;
}
}  // namespace

float SystemTextGamma() { return Settings().gamma; }
float SystemTextContrast() { return Settings().contrast; }

GlyphRenderStyle GlyphStyle(IDWriteFontFace* face, float size) {
  GlyphRenderStyle style;
  style.em = size;
  // gdiTextSize: the size a GDI compatible request rounds to.
  const float gdi = std::round(size * 64.0f) / 64.0f;
  const int own = Settings().renderingMode;
  if (size > 20 || own == int(DWRITE_RENDERING_MODE_NATURAL) ||
      own == int(DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC)) {
    style.render = own == int(DWRITE_RENDERING_MODE_NATURAL) ? DWRITE_RENDERING_MODE_NATURAL
                                                             : DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC;
    return style;
  }
  uint16_t flags = 0;
  bool version1 = false;
  if (GaspRange(face, int(std::round(gdi)), &flags, &version1) && version1) {
    // Bit 3 of a version-1 gasp range is symmetric smoothing.
    style.render = (flags & 8) ? DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC
                               : DWRITE_RENDERING_MODE_NATURAL;
  } else if (IsHinted(face)) {
    style.render = DWRITE_RENDERING_MODE_NATURAL;
    style.em = gdi;
  }
  return style;
}
}  // namespace ruyijs::web_geometry
