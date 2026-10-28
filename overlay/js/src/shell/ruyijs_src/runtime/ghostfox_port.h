// ghostfox_port.h — the platform primitives the engine's transport layer
// needs, in one place: a monotonic millisecond clock. Windows keeps QPC;
// POSIX uses CLOCK_MONOTONIC. Everything else (pipes, process spawn) is
// branch-local to the file that owns it.
#ifndef RUYIJS_GHOSTFOX_PORT_H
#define RUYIJS_GHOSTFOX_PORT_H
#include <cstdint>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
namespace ruyijs {
using Ticks = ULONGLONG;
inline Ticks TicksNow() {
  LARGE_INTEGER counter, frequency;
  QueryPerformanceCounter(&counter);
  QueryPerformanceFrequency(&frequency);
  return static_cast<ULONGLONG>(counter.QuadPart * 1000ULL / frequency.QuadPart);
}
}  // namespace ruyijs
#else
#include <time.h>
namespace ruyijs {
using Ticks = unsigned long long;
inline Ticks TicksNow() {
  timespec now{};
  clock_gettime(CLOCK_MONOTONIC, &now);
  return static_cast<Ticks>(now.tv_sec) * 1000ULL + now.tv_nsec / 1000000ULL;
}
}  // namespace ruyijs
#endif
#endif
