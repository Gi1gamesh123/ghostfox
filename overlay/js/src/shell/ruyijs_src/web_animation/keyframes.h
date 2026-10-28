#ifndef RUYIJS_WEB_ANIMATION_KEYFRAMES_H
#define RUYIJS_WEB_ANIMATION_KEYFRAMES_H
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace ruyijs::web_animation {
// One processed keyframe (web-animations-1 §5.10.3).
struct Frame {
  std::optional<double> offset;
  std::string easing = "linear", composite = "auto";
  std::vector<std::pair<std::string, std::string>> values;  // sorted property name -> value
};
// Reads the keyframes argument (list or property-indexed form); keyframes.cpp.
bool ReadFrames(JSContext* cx, JS::HandleValue input, const std::string& prefix, std::vector<Frame>* frames);
}
#endif
