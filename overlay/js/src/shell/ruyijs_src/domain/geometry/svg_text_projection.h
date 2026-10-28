/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// Projection of the donor's loose-ink and SVG run projection.
// References: gfxFont::Measure, nsTextFrame::RecomputeOverflow,
// TextRenderedRun::GetRunUserSpaceRect and SVGTextFrame::GetExtentOfChar.
#pragma once
#include <algorithm>
#include <cmath>

namespace ruyijs::domain::geometry {
struct InkBox {
  double left, top, right, bottom;
  void Include(double x, double y, double width, double height) {
    if (width <= 0 || height <= 0) return;
    left = std::min(left, x); top = std::min(top, y);
    right = std::max(right, x + width); bottom = std::max(bottom, y + height);
  }
};
struct Measurement {
  float box[4], first[4], transformed[4];
  double unrounded[4];
};
inline Measurement ProjectInk(InkBox ink, double advance, double firstAdvance,
    double ascent, double descent, float factor, bool italic, double x, double y) {
  const double height = ascent + descent;
  if (italic) {
    const float skew = float(std::tan(14.0 * 3.14159265358979323846 / 180));
    ink.left -= std::ceil(skew * ink.bottom);
    ink.right += std::ceil(skew * -ink.top);
  }
  // Frame ink overflow is retained independently of the rendered run's metric box.
  const double left = std::max(0.0, -std::floor(ink.left));
  const double right = std::max(0.0, std::ceil(ink.right) - advance);
  const double top = std::max(0.0, -std::floor(ink.top + ascent));
  const double bottom = std::max(0.0, std::ceil(ink.bottom + ascent) - height);
  const auto round = [](double v) { return std::floor(v + .5); };
  const double app[4] = {round(ink.left) - left, round(ink.top + ascent) - top,
    round(ink.right - ink.left) + left + right,
    round(ink.bottom - ink.top) + top + bottom};
  float fill[4];
  for (unsigned i = 0; i < 4; ++i) fill[i] = float(double(float(app[i] / 60)) / factor);
  const double translationY = y - ascent / 60 / double(factor);
  Measurement result{};
  result.unrounded[0] = fill[0] + x;
  result.unrounded[1] = fill[1] + translationY;
  result.unrounded[2] = fill[2]; result.unrounded[3] = fill[3];
  result.box[0] = float(fill[0] + x);
  result.box[1] = float(fill[1] + translationY);
  result.box[2] = fill[2]; result.box[3] = fill[3];
  // SVGTextFrame stores a float factor; 1 / factor is float before MatrixDouble.
  const float inverse = 1 / factor;
  result.first[0] = float(x);
  result.first[1] = float(y - ascent / 60 * inverse);
  result.first[2] = float(firstAdvance / 60 * inverse);
  result.first[3] = float(height / 60 * inverse);
  result.transformed[0] = float(fill[0] * double(factor) + x * factor);
  result.transformed[1] = float(fill[1] * double(factor) + translationY * factor);
  result.transformed[2] = float(fill[2] * double(factor));
  result.transformed[3] = float(fill[3] * double(factor));
  return result;
}

}
