#ifndef RUYIJS_WEB_CANVAS_RASTER_TRAPEZOID_H
#define RUYIJS_WEB_CANVAS_RASTER_TRAPEZOID_H
// The coverage computations of SkScan_AAAPath.cpp: one trapezoid row becomes
// per-pixel alpha values, which the additive blitter accumulates.
#include <cstdint>

#include "web_canvas/raster_blitter.h"
#include "web_canvas/raster_fixed.h"

namespace ruyijs::web_canvas::raster {
// blit_aaa_trapezoid_row: the coverage of one row between two edge positions.
void BlitAaaTrapezoidRow(AdditiveBlitter& blitter, int y, Fixed ul, Fixed ur, Fixed ll, Fixed lr,
                         Fixed leftDy, Fixed rightDy, uint8_t fullAlpha, bool noRealBlitter);
// blit_trapezoid_row: the same, with the full-coverage rectangle between the
// corner triangles blitted separately because it can be much wider.
void BlitTrapezoid(AdditiveBlitter& blitter, int y, Fixed ul, Fixed ur, Fixed ll, Fixed lr,
                   Fixed leftDy, Fixed rightDy, uint8_t fullAlpha, bool noRealBlitter);
}  // namespace ruyijs::web_canvas::raster
#endif
