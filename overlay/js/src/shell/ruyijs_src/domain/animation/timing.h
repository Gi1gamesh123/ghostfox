#ifndef RUYIJS_DOMAIN_ANIMATION_TIMING_H
#define RUYIJS_DOMAIN_ANIMATION_TIMING_H
// Web Animations timing model (web-animations-1 §4.5-4.9): effect timing,
// phases, active time and iteration progress. Engine-independent.
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include "domain/animation/easing.h"

namespace ruyijs::domain::animation {
enum class Fill { None, Forwards, Backwards, Both, Auto };
enum class Direction { Normal, Reverse, Alternate, AlternateReverse };
enum class Phase { Before, Active, After, Idle };
inline const char* const kFillNames[] = {"none", "forwards", "backwards", "both", "auto"};
inline const char* const kDirectionNames[] = {"normal", "reverse", "alternate", "alternate-reverse"};

struct EffectTiming {
  double delay = 0, endDelay = 0, iterationStart = 0, iterations = 1;
  bool durationAuto = true;  // "auto": an iteration duration of zero for a keyframe effect
  double duration = 0;
  Fill fill = Fill::Auto;
  Direction direction = Direction::Normal;
  Easing easing;
  double IterationDuration() const { return durationAuto ? 0 : duration; }
  double ActiveDuration() const {
    const double d = IterationDuration();
    return (d == 0 || iterations == 0) ? 0 : d * iterations;
  }
  double EndTime() const { return std::max(delay + ActiveDuration() + endDelay, 0.0); }
};

struct ComputedTiming {
  Phase phase = Phase::Idle;
  std::optional<double> localTime, activeTime, progress, currentIteration;
  Fill fill = Fill::None;  // "auto" resolved for a keyframe effect
};

// `localTime` is the animation's current time (null when unresolved);
// `backwards` is true when the playback rate is negative.
inline ComputedTiming Compute(const EffectTiming& t, std::optional<double> localTime, bool backwards) {
  ComputedTiming r;
  r.fill = t.fill == Fill::Auto ? Fill::None : t.fill;
  r.localTime = localTime;
  if (!localTime) return r;
  const double active = t.ActiveDuration(), end = t.EndTime(), lt = *localTime;
  const double beforeActive = std::max(std::min(t.delay, end), 0.0);
  const double activeAfter = std::max(std::min(t.delay + active, end), 0.0);
  if (lt < beforeActive || (backwards && lt == beforeActive)) r.phase = Phase::Before;
  else if (lt > activeAfter || (!backwards && lt == activeAfter)) r.phase = Phase::After;
  else r.phase = Phase::Active;
  const bool fillBackwards = r.fill == Fill::Backwards || r.fill == Fill::Both;
  const bool fillForwards = r.fill == Fill::Forwards || r.fill == Fill::Both;
  if (r.phase == Phase::Before) {
    if (fillBackwards) r.activeTime = std::max(lt - t.delay, 0.0);
  } else if (r.phase == Phase::Active) {
    r.activeTime = lt - t.delay;
  } else if (fillForwards) {
    r.activeTime = std::max(std::min(lt - t.delay, active), 0.0);
  }
  if (!r.activeTime) return r;
  const double duration = t.IterationDuration();
  double overall = duration == 0
      ? (r.phase == Phase::Before ? t.iterationStart : t.iterationStart + t.iterations)
      : *r.activeTime / duration + t.iterationStart;
  double simple = std::isinf(overall) ? std::fmod(t.iterationStart, 1.0) : std::fmod(overall, 1.0);
  if (simple == 0 && (r.phase == Phase::Active || r.phase == Phase::After) &&
      *r.activeTime == active && t.iterations != 0)
    simple = 1;
  double iteration;
  if (r.phase == Phase::After && std::isinf(t.iterations)) iteration = std::numeric_limits<double>::infinity();
  else if (simple == 1) iteration = std::floor(overall) - 1;
  else iteration = std::floor(overall);
  r.currentIteration = iteration;
  bool forwards = true;
  if (t.direction == Direction::Reverse) forwards = false;
  else if (t.direction == Direction::Alternate || t.direction == Direction::AlternateReverse) {
    const bool even = std::isinf(iteration) || std::fmod(iteration, 2.0) == 0;
    forwards = (t.direction == Direction::Alternate) == even;
  }
  const double directed = forwards ? simple : 1 - simple;
  const bool beforeFlag = (r.phase == Phase::Before && forwards) || (r.phase == Phase::After && !forwards);
  r.progress = EvaluateEasing(t.easing, directed, beforeFlag);
  return r;
}

// A relevant (current or in effect) animation, for getAnimations().
inline bool Relevant(const ComputedTiming& c, double playbackRate) {
  if (c.phase == Phase::Idle) return false;
  if (c.phase == Phase::Active || c.activeTime) return true;
  return (c.phase == Phase::Before && playbackRate > 0) || (c.phase == Phase::After && playbackRate < 0);
}
}  // namespace ruyijs::domain::animation
#endif
