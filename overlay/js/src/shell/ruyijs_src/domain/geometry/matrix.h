#pragma once
namespace ruyijs::domain::geometry {
inline constexpr unsigned kAliases[] = {0, 1, 4, 5, 12, 13};
inline void MatrixIdentity(double values[16]) noexcept {
  for (unsigned i = 0; i < 16; ++i) values[i] = i % 5 == 0 ? 1 : 0;
}
inline bool MatrixIsIdentity(const double values[16]) noexcept {
  for (unsigned i = 0; i < 16; ++i)
    if (values[i] != (i % 5 == 0 ? 1 : 0)) return false;
  return true;
}
inline bool MatrixPreserves2D(unsigned index, double value) noexcept {
  for (unsigned alias : kAliases) if (index == alias) return true;
  return value == (index == 10 || index == 15 ? 1 : 0);
}
inline void MatrixTranslate(double values[16], const double delta[3]) noexcept {
  for (unsigned i = 0; i < 3; ++i) values[12 + i] += delta[i];
}
}
