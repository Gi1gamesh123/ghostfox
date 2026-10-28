// The sampling half of the software rasterizer: the float -> mediump -> UNORM8
// conversion, the edge functions of the D3D11 fill rule and the 4x MSAA coverage
// of one pixel. draw.cpp uses these to fill the drawing buffer.
#include "web_graphics/draw_sample.h"

#include <cmath>
#include <cstring>

namespace ruyijs::web_graphics::draw_detail {
const double kSamples[4][2] = {{-2, -6}, {6, -2}, {-6, 2}, {2, 6}};

namespace {
// float32 -> binary16 -> float32: the fragment stage's mediump precision, which
// quantises the interpolated varying before it is packed to the drawing buffer.
uint16_t HalfOf(float value) {
  uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  const uint32_t sign = (bits >> 16) & 0x8000u;
  int32_t exponent = int32_t((bits >> 23) & 0xFFu) - 127;
  if (exponent < -14) return uint16_t(sign);
  if (exponent > 15) return uint16_t(sign | 0x7C00u);
  const uint32_t mantissa = bits & 0x7FFFFFu;
  const uint32_t rest = mantissa & 0x1FFFu;
  uint32_t base = mantissa >> 13;
  if (rest > 0x1000u || (rest == 0x1000u && (base & 1u) != 0)) ++base;
  if (base == 0x400u) {
    base = 0;
    if (++exponent > 15) return uint16_t(sign | 0x7C00u);
  }
  return uint16_t(sign | (uint32_t(exponent + 15) << 10) | base);
}
float FloatOf(uint16_t half) {
  const int32_t exponent = int32_t((half >> 10) & 0x1Fu) - 15;
  if (exponent == -15) return 0.0f;
  const uint32_t bits = (uint32_t(half & 0x8000u) << 16) |
                        (uint32_t(exponent + 127) << 23) | (uint32_t(half & 0x3FFu) << 13);
  float out = 0.0f;
  std::memcpy(&out, &bits, sizeof(out));
  return out;
}
}  // namespace

int32_t Unorm8(float component) {
  if (!(component > 0.0f)) return 0;
  if (component > 1.0f) component = 1.0f;
  const float mediump = FloatOf(HalfOf(component));
  const double scaled = FloatOf(HalfOf(mediump * 255.0f));
  if (!(scaled > 0.0)) return 0;
  const double rounded = std::ceil(scaled - 0.5);
  if (rounded <= 0.0) return 0;
  return rounded >= 255.0 ? 255 : int32_t(rounded);
}

// The D3D11 4x UNORM8 resolve, as measured on the reference NVIDIA path with a
// sweep of every (coverage, sample value): a tie rounds down, i.e. (sum+1)>>2,
// except that NVIDIA's 9-bit accumulator keeps the carry while sum+2 is one of
// the first seven multiples of 64 (64..448). From 512 the carry is lost and the
// tie rounds down again (sum = 510 -> 127, 702 -> 175).
int32_t Resolve8(int32_t sum) {
  const int32_t half_up = sum + 2;
  int32_t resolved = (sum + 1) >> 2;
  if ((half_up & 63) == 0 && half_up < 512) resolved = half_up >> 2;
  return resolved > 255 ? 255 : resolved;
}

double Edge(const double a[2], const double b[2], double x, double y) {
  return (b[0] - a[0]) * (y - a[1]) - (b[1] - a[1]) * (x - a[0]);
}

bool InsideEdge(double weight, const double a[2], const double b[2], double sign) {
  if (weight * sign > 0.0) return true;
  if (weight != 0.0) return false;
  const double dx = (b[0] - a[0]) * sign, dy = (b[1] - a[1]) * sign;
  return dy > 0.0 || (dy == 0.0 && dx < 0.0);
}

int32_t Coverage(const double* a, const double* b, const double* c, int32_t x, int32_t y,
                 double sign) {
  int32_t covered = 0;
  for (const auto& sample : kSamples) {
    const double sx = x + 0.5 + sample[0] / 16.0, sy = y + 0.5 + sample[1] / 16.0;
    if (!InsideEdge(Edge(a, b, sx, sy), a, b, sign) ||
        !InsideEdge(Edge(b, c, sx, sy), b, c, sign) ||
        !InsideEdge(Edge(c, a, sx, sy), c, a, sign)) continue;
    ++covered;
  }
  return covered;
}
}  // namespace ruyijs::web_graphics::draw_detail
