#include "web_geometry/font_backend.h"

#include <d2d1.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace ruyijs::web_geometry {
namespace {
constexpr size_t kPointLimit = 2u * 1024u * 1024u;

class OutlineSink final : public ID2D1SimplifiedGeometrySink {
 public:
  OutlineSink(CanvasTextPath* path, float x, float y, float scale, bool italic)
      : path_(path), x_(x), y_(y), inverse_(1.0f / scale), italic_(italic) {}
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
    if (!out) return E_POINTER;
    *out = nullptr;
    if (iid == __uuidof(IUnknown) || iid == __uuidof(ID2D1SimplifiedGeometrySink)) {
      *out = static_cast<ID2D1SimplifiedGeometrySink*>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
  ULONG STDMETHODCALLTYPE Release() override {
    const ULONG refs = --refs_;
    if (!refs) delete this;
    return refs;
  }
  void STDMETHODCALLTYPE SetFillMode(D2D1_FILL_MODE) override {}
  void STDMETHODCALLTYPE SetSegmentFlags(D2D1_PATH_SEGMENT) override {}
  void STDMETHODCALLTYPE BeginFigure(D2D1_POINT_2F point, D2D1_FIGURE_BEGIN) override {
    if (!Valid() || path_->points.size() >= kPointLimit) {
      valid_ = false;
      return;
    }
    const auto mapped = Map(point);
    path_->Move(mapped.x, mapped.y);
  }
  void STDMETHODCALLTYPE AddLines(const D2D1_POINT_2F* points, UINT32 count) override {
    if (!Valid() || !points || count > kPointLimit - path_->points.size()) {
      valid_ = false;
      return;
    }
    for (UINT32 i = 0; i < count; ++i) {
      const auto mapped = Map(points[i]);
      path_->Line(mapped.x, mapped.y);
    }
  }
  void STDMETHODCALLTYPE AddBeziers(const D2D1_BEZIER_SEGMENT* beziers, UINT32 count) override {
    if (!Valid() || !beziers || count > (kPointLimit - path_->points.size()) / 3) {
      valid_ = false;
      return;
    }
    for (UINT32 i = 0; i < count; ++i) {
      const auto& b = beziers[i];
      const auto p1 = Map(b.point1), p2 = Map(b.point2), p3 = Map(b.point3);
      path_->Cubic(p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
    }
  }
  void STDMETHODCALLTYPE EndFigure(D2D1_FIGURE_END end) override {
    if (Valid() && end == D2D1_FIGURE_END_CLOSED) path_->Close();
  }
  HRESULT STDMETHODCALLTYPE Close() override { return valid_ ? S_OK : E_OUTOFMEMORY; }
  bool Valid() const { return valid_ && path_ && path_->points.size() <= kPointLimit; }

 private:
  CanvasTextPath* path_;
  float x_, y_, inverse_;
  bool italic_, valid_ = true;
  ULONG refs_ = 1;
  D2D1_POINT_2F Map(D2D1_POINT_2F point) const {
    const float skew = italic_ ? std::tan(14.0f * 3.14159265358979323846f / 180.0f) : 0.0f;
    const float x = point.x - skew * point.y;
    return {x_ + x * inverse_, y_ + point.y * inverse_};
  }
};

uint32_t Codepoint(const std::u16string& text, size_t* at) {
  uint32_t code = text[(*at)++];
  if (code >= 0xd800 && code <= 0xdbff && *at < text.size() &&
      text[*at] >= 0xdc00 && text[*at] <= 0xdfff)
    code = 0x10000 + ((code - 0xd800) << 10) + text[(*at)++] - 0xdc00;
  return code;
}
bool Continuation(uint32_t code) {
  return code == 0x200d || (code >= 0xfe00 && code <= 0xfe0f) ||
      (code >= 0xe0100 && code <= 0xe01ef) ||
      hb_unicode_combining_class(hb_unicode_funcs_get_default(), code) != 0;
}
}  // namespace

bool SystemFonts::BuildCanvasTextPath(const std::u16string& text, const FontInput& input,
                                      CanvasTextPath* result) {
  if (!result) return false;
  *result = {};
  if (text.empty()) return true;
  if (text.size() > 65536 || !std::isfinite(input.scale) || input.scale <= 0 ||
      !std::isfinite(input.size) || input.size <= 0 || !std::isfinite(input.x) ||
      !std::isfinite(input.y)) return false;
  if (!collection_) {
    auto collection = std::make_unique<FontCollection>();
    if (!collection->Initialize()) return false;
    collection_ = std::move(collection);
  }
  const float emsize = float(std::round(input.size * input.scale * 60.0) / 60.0);
  if (!std::isfinite(emsize) || emsize <= 0 || emsize > 16384) return false;
  int pen = 0;
  for (size_t start = 0; start < text.size();) {
    size_t end = start;
    SelectedFont font;
    const uint32_t first = Codepoint(text, &end);
    if (!collection_->Select(input, first, &font)) return false;
    while (end < text.size()) {
      size_t next = end;
      const uint32_t code = Codepoint(text, &next);
      if (!Continuation(code)) {
        SelectedFont candidate;
        if (!collection_->Select(input, code, &candidate)) return false;
        if (font.face.Get() != candidate.face.Get() ||
            font.syntheticItalic != candidate.syntheticItalic)
          break;
      }
      end = next;
    }
    FontInput local = input;
    local.x = local.y = 0;
    FontRun run;
    if (!ShapeFont(font, local, text.substr(start, end - start), &run)) return false;
    if (!run.glyphs.empty()) {
      std::vector<DWRITE_GLYPH_OFFSET> offsets(run.glyphs.size());
      for (size_t i = 0; i < offsets.size(); ++i) {
        offsets[i].advanceOffset = run.glyph_offsets_x[i];
        offsets[i].ascenderOffset = run.glyph_offsets_y[i];
      }
      const float x = float(input.x + double(pen) / 60.0 / input.scale);
      auto* sink = new OutlineSink(result, x, float(input.y), float(input.scale),
                                   font.syntheticItalic);
      const HRESULT hr = font.face->GetGlyphRunOutline(
          emsize, run.glyphs.data(), run.glyph_advances.data(), offsets.data(),
          UINT32(run.glyphs.size()), FALSE, FALSE, sink);
      const bool valid = SUCCEEDED(hr) && sink->Valid();
      sink->Release();
      if (!valid) return false;
    }
    pen += run.advance;
    start = end;
  }
  return true;
}
}  // namespace ruyijs::web_geometry
