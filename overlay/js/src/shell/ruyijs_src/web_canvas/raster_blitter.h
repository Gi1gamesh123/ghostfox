#ifndef RUYIJS_WEB_CANVAS_RASTER_BLITTER_H
#define RUYIJS_WEB_CANVAS_RASTER_BLITTER_H
// The two blitters Skia's analytic anti-aliasing talks to. Blitter writes
// coverage into the canvas byte store through the raster-pipeline blend the way
// SkRasterPipelineBlitter does; AdditiveBlitter accumulates the coverage of one
// scanline first, like RunBasedAdditiveBlitter and SafeRLEAdditiveBlitter.
#include <cstdint>
#include <vector>

#include "web_canvas/raster_fixed.h"
#include "web_canvas/raster_path.h"

namespace ruyijs::web_canvas::raster {
// SkAlphaRuns::CatchOverflow: 0..256 folded into 0..255.
inline uint8_t CatchOverflow(int value) { return uint8_t(value - (value >> 8)); }

class Blitter {
 public:
  Blitter(Surface surface, const Paint& paint, bool wrapped = false)
      : surface_(surface), paint_(paint), wrapped_(wrapped) {}
  void BlitH(int x, int y, int width);
  void BlitV(int x, int y, int height, uint8_t alpha);
  // One alpha per pixel: the row Skia hands over as runs of a single pixel.
  void BlitAlphas(int x, int y, const uint8_t* alphas, int count);
  void BlitAntiH2(int x, int y, uint8_t a0, uint8_t a1);
  // blitAntiRect: the left and right partial columns plus the full middle.
  void BlitAntiRect(int x, int y, int width, int height, uint8_t leftAlpha, uint8_t rightAlpha);
  void BlendPixel(int x, int y, uint8_t coverage);
  // raster_blend.cpp: one premultiplied channel through the lowp pipeline.
  uint8_t Mix(uint8_t src, uint8_t dst, uint8_t srcAlpha, uint8_t dstAlpha) const;
  uint8_t MixAlpha(uint8_t srcAlpha, uint8_t dstAlpha) const;
  // Skia paints source-over with the legacy 32-bit blitters
  // (SkARGB32_Opaque_Blitter / SkARGB32_Blitter) and every other mode through
  // the raster pipeline; the two round differently, so both are implemented.
  bool LegacySourceOver() const { return paint_.blend == Blend::kSourceOver; }
  // An opaque source-over paint takes SkARGB32_Opaque_Blitter, whose
  // blitAntiH2 lerps with SkFastFourByteInterp (scale = alpha + alpha/128).
  bool OpaqueSourceOver() const {
    return paint_.blend == Blend::kSourceOver && paint_.rgba[3] == 0xff;
  }
  // SkBlitter::Choose sends an opaque black paint to SkARGB32_Black_Blitter,
  // whose blitAntiH2 adds the coverage to the alpha instead of interpolating.
  bool OpaqueBlack() const {
    return paint_.blend == Blend::kSourceOver && paint_.rgba[3] == 0xff && paint_.rgba[0] == 0 &&
           paint_.rgba[1] == 0 && paint_.rgba[2] == 0;
  }
  static bool PreScaleCoverage(Blend blend);

 private:
  Surface surface_;
  Paint paint_;
  // SkScanClipper wraps the blitter (SkRectClipBlitter) whenever the path
  // bounds stick out past the clip horizontally; the wrapper has no
  // blitAntiH2, so the two-alphas of a corner take the run blitter instead.
  bool wrapped_ = false;
};

// raster_mask.cpp: SkARGB32_Blitter::blitMask for a flat source-over colour,
// used by glyph masks; every other blend falls back to the pipeline blitter.
void BlitMask(Surface surface, const Paint& paint, int left, int top, const uint8_t* coverage,
              int width, int height);
// The other half of the same file: a premultiplied RGBA sprite blended with the
// paint's own alpha, which is how a colour glyph mask reaches the device
// (SkARGB32_Blit32 of an ARGB32 mask).
void BlitSprite(Surface surface, const Paint& paint, int left, int top, const uint8_t* rgba,
                int width, int height);

class AdditiveBlitter {
 public:
  // RunBasedAdditiveBlitter: one scanline of runs, snapped and blitted per row.
  AdditiveBlitter(Blitter& real, int left, int width, int top, bool safe)
      : real_(real), left_(left), width_(width), top_(top), safe_(safe),
        currentY_(top - 1), row_(size_t(width > 0 ? width : 0), 0) {}
  // MaskAdditiveBlitter: a small path accumulates its alphas in an A8 mask that
  // is blitted once, so its values are never snapped to 0 or 255.
  AdditiveBlitter(Blitter& real, uint8_t* mask, int left, int top, int width, int height)
      : real_(real), left_(left), width_(width), top_(top), safe_(false),
        currentY_(top - 1), mask_(mask), height_(height) {}
  ~AdditiveBlitter() { Flush(); }
  void BlitAntiH(int x, int y, uint8_t alpha);
  void BlitAntiH(int x, int y, int width, uint8_t alpha);
  void BlitAntiH(int x, int y, const uint8_t* antialias, int len);
  // The rectangle forms: the convex walker calls these on the blitter it was
  // given, which sets the alphas of a mask and paints the pixels of a device.
  void BlitH(int x, int y, int width);
  void BlitV(int x, int y, int height, uint8_t alpha);
  void BlitAntiRect(int x, int y, int width, int height, uint8_t leftAlpha, uint8_t rightAlpha);
  // The trapezoid row forms: a device takes them through the real blitter, a
  // mask adds them to the alphas it already holds.
  void BlitRow(int x, int y, const uint8_t* alphas, int length);
  void BlitRowH(int x, int y, int width);
  // blit_two_alphas: the two coverages of a corner pixel pair.
  void BlitPair(int x, int y, uint8_t first, uint8_t second);
  void FlushIfYChanged(Fixed y, Fixed nextY) {
    if (FloorToInt(y) != FloorToInt(nextY)) Flush();
  }
  void Flush();

 private:
  void CheckY(int y) {
    if (y != currentY_) {
      Flush();
      currentY_ = y;
    }
  }
  uint8_t* MaskAt(int x, int y) {
    if (!mask_ || x < left_ || x >= left_ + width_ || y < top_ || y >= top_ + height_)
      return nullptr;
    return mask_ + size_t(y - top_) * size_t(width_) + size_t(x - left_);
  }
  void AddPixel(int x, int y, uint8_t alpha);
  void Add(int index, uint8_t alpha);
  Blitter& real_;
  int left_;
  int width_;
  int top_;
  bool safe_;
  int currentY_;
  std::vector<uint8_t> row_;
  uint8_t* mask_ = nullptr;
  int height_ = 0;
};
}  // namespace ruyijs::web_canvas::raster
#endif
