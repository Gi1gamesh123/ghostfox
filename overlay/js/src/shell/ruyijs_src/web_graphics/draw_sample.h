#ifndef RUYIJS_WEB_GRAPHICS_DRAW_SAMPLE_H
#define RUYIJS_WEB_GRAPHICS_DRAW_SAMPLE_H
#include <cstdint>

namespace ruyijs::web_graphics::draw_detail {
// The D3D11 standard 4x multisample positions, in 1/16 of a pixel from the centre.
extern const double kSamples[4][2];
// A fragment colour component straight to the attach format: the mediump rounding
// of the fragment stage, then the UNORM8 conversion with ties rounding down.
int32_t Unorm8(float component);
// One channel's 4x resolve: the covered sample slots all hold `sample` and the
// uncovered slots hold the clear zero, so their sum is sample*covered. D3D11's
// UNORM8 resolve rounds to nearest with ties down, except where the rounded sum
// lands on one of the low multiples of 64 (see below).
int32_t Resolve8(int32_t sum);
// The signed area of (a, b, p), the edge function of the rasterizer.
double Edge(const double a[2], const double b[2], double x, double y);
// The D3D11 top-left fill rule: a sample exactly on an edge belongs to the
// triangle only when that edge is a top or left edge for the triangle's winding.
bool InsideEdge(double weight, const double a[2], const double b[2], double sign);
// How many of the four samples of pixel (x, y) the triangle covers.
int32_t Coverage(const double* a, const double* b, const double* c, int32_t x, int32_t y,
                 double sign);
}  // namespace ruyijs::web_graphics::draw_detail
#endif
