#ifndef RUYIJS_WEB_CANVAS_RASTER_WALK_H
#define RUYIJS_WEB_CANVAS_RASTER_WALK_H
// The two scanline walkers of Skia's analytic anti-aliasing and the coverage
// conversions they share.
#include <cstdint>

#include "web_canvas/raster_blitter.h"
#include "web_canvas/raster_edge.h"

namespace ruyijs::web_canvas::raster {
// get_partial_alpha(0xFF, dY): a partial row's coverage as an alpha byte.
inline uint8_t GetPartialAlpha(Fixed partialHeight) {
  return uint8_t((0xff * partialHeight + kFixedHalf) >> 16);
}
inline uint8_t FixedToAlpha(Fixed value) { return GetPartialAlpha(value); }

void WalkConvexEdges(Edge* head, AdditiveBlitter& blitter, int stopY, Fixed leftBound,
                     Fixed rightBound);
void WalkEdges(Edge* head, Edge* tail, bool evenOdd, AdditiveBlitter& blitter, int startY,
               int stopY, Fixed leftClip, Fixed rightClip, bool forceRLE, bool skipIntersect);
}  // namespace ruyijs::web_canvas::raster
#endif
