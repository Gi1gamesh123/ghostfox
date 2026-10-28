// SkScan_AAAPath's two additive coverage blitters, reduced to per-pixel alphas:
// RunBasedAdditiveBlitter accumulates one scanline of runs and blits it when the
// row changes, MaskAdditiveBlitter accumulates a whole small path in an A8 mask
// that the fill blits once, so its alphas are never snapped to 0 or 255.
#include <algorithm>

#include "web_canvas/raster_blitter.h"

namespace ruyijs::web_canvas::raster {
namespace {
// RunBasedAdditiveBlitter::flush snaps alphas close to opaque or clear.
inline uint8_t SnapAlpha(uint8_t alpha) { return alpha > 247 ? 0xff : alpha < 8 ? 0x00 : alpha; }
}  // namespace

void AdditiveBlitter::AddPixel(int x, int y, uint8_t alpha) {
  if (uint8_t* value = MaskAt(x, y)) *value = CatchOverflow(int(*value) + alpha);
}

void AdditiveBlitter::Add(int index, uint8_t alpha) {
  if (index < 0 || index >= width_) return;
  if (mask_) {
    AddPixel(left_ + index, currentY_, alpha);
    return;
  }
  uint8_t& value = row_[size_t(index)];
  const unsigned sum = unsigned(value) + alpha;
  value = safe_ ? uint8_t(sum > 255 ? 255 : sum) : CatchOverflow(int(sum));
}

void AdditiveBlitter::BlitH(int x, int y, int width) {
  if (mask_) {
    // MaskAdditiveBlitter sets the alphas it is handed; nothing accumulates
    // here because the walker only sends fully covered stretches.
    for (int i = 0; i < width; ++i)
      if (uint8_t* value = MaskAt(x + i, y)) *value = 0xff;
    return;
  }
  real_.BlitH(x, y, width);
}

void AdditiveBlitter::BlitV(int x, int y, int height, uint8_t alpha) {
  if (mask_) {
    if (alpha == 0) return;
    for (int i = 0; i < height; ++i) {
      if (uint8_t* value = MaskAt(x, y + i)) *value = alpha;
    }
    return;
  }
  real_.BlitV(x, y, height, alpha);
}

void AdditiveBlitter::BlitAntiRect(int x, int y, int width, int height, uint8_t leftAlpha,
                                   uint8_t rightAlpha) {
  if (mask_) {
    BlitV(x, y, height, leftAlpha);
    BlitV(x + 1 + width, y, height, rightAlpha);
    for (int i = 0; i < height; ++i) BlitH(x + 1, y + i, width);
    return;
  }
  real_.BlitAntiRect(x, y, width, height, leftAlpha, rightAlpha);
}

void AdditiveBlitter::BlitRow(int x, int y, const uint8_t* alphas, int length) {
  if (mask_) {
    // A trapezoid row adds its coverages to the mask, as Skia's maskRow does.
    for (int i = 0; i < length; ++i) AddPixel(x + i, y, alphas[i]);
    return;
  }
  // A single alpha keeps Skia's own blitV formulation, which rounds
  // differently from the pipeline.
  if (length == 1) real_.BlitV(x, y, 1, alphas[0]);
  else real_.BlitAlphas(x, y, alphas, length);
}

void AdditiveBlitter::BlitPair(int x, int y, uint8_t first, uint8_t second) {
  if (mask_) {
    AddPixel(x, y, first);
    AddPixel(x + 1, y, second);
    return;
  }
  real_.BlitAntiH2(x, y, first, second);
}

void AdditiveBlitter::BlitRowH(int x, int y, int width) {
  if (mask_) {
    for (int i = 0; i < width; ++i) AddPixel(x + i, y, 0xff);
    return;
  }
  real_.BlitH(x, y, width);
}

void AdditiveBlitter::BlitAntiH(int x, int y, uint8_t alpha) {
  CheckY(y);
  x -= left_;
  if (x < 0 || x >= width_) return;
  Add(x, alpha);
}

void AdditiveBlitter::BlitAntiH(int x, int y, int width, uint8_t alpha) {
  CheckY(y);
  x -= left_;
  for (int i = 0; i < width; ++i) Add(x + i, alpha);
}

void AdditiveBlitter::BlitAntiH(int x, int y, const uint8_t* antialias, int len) {
  CheckY(y);
  x -= left_;
  if (x < 0) {
    len += x;
    antialias -= x;
    x = 0;
  }
  if (len > width_ - x) len = width_ - x;
  for (int i = 0; i < len; ++i) Add(x + i, antialias[i]);
}

void AdditiveBlitter::Flush() {
  // A mask is blitted whole once the walk is over, so a row change flushes
  // nothing and no alpha is ever snapped.
  if (mask_ || currentY_ < top_) return;
  bool any = false;
  for (int i = 0; i < width_; ++i) {
    row_[size_t(i)] = SnapAlpha(row_[size_t(i)]);
    if (row_[size_t(i)] != 0) any = true;
  }
  if (any) real_.BlitAlphas(left_, currentY_, row_.data(), width_);
  std::fill(row_.begin(), row_.end(), uint8_t(0));
  currentY_ = top_ - 1;
}
}  // namespace ruyijs::web_canvas::raster
