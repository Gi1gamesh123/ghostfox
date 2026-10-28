#include "web_graphics/sanitize.h"

#include <regex>
#include <string>

#include "web_graphics/sanitize_internal.h"

namespace ruyijs::web_graphics {
namespace {
using sanitize_detail::Contains;
using sanitize_detail::ParseDigits;
}  // namespace

// The integrated and mobile vendors: Intel, Adreno, Mali, PowerVR, Xclipse,
// Vivante, VideoCore, Tegra, and the Direct3D WARP adapter. Ported
// decision-for-decision from the donor's ChooseDeviceReplacement.
bool ReplaceMobileDevice(const std::string& text, std::string* out) {
  std::smatch match;
  unsigned long modelNum = 0;

  if (Contains(text, "Intel")) {
    const char* HD_GRAPHICS = "Intel(R) HD Graphics";
    const char* HD_GRAPHICS_400 = "Intel(R) HD Graphics 400";
    const char* INTEL_945GM = "Intel 945GM";
    const char* DGPU_ARC = "Intel(R) Arc(TM) A750 Graphics";
    if (Contains(text, "Intel(R) Arc(TM)")) {
      *out = DGPU_ARC;
      return true;
    }
    static const std::regex kIntelHD("Intel.*Graphics( P?([0-9][0-9][0-9]+))?");
    if (std::regex_search(text, match, kIntelHD)) {
      if (match.str(1).empty()) {
        *out = HD_GRAPHICS;
        return true;
      }
      if (!ParseDigits(match.str(2), 10, &modelNum)) {
        *out = HD_GRAPHICS_400;
        return true;
      }
      if (modelNum >= 5000) *out = HD_GRAPHICS_400;
      else if (modelNum >= 1000) *out = HD_GRAPHICS;
      else *out = HD_GRAPHICS_400;
      return true;
    }
    *out = INTEL_945GM;
    return true;
  }

  static const std::regex kAdreno("Adreno.*?([A-Z]?[0-9]-?[0-9]+)");
  if (std::regex_search(text, match, kAdreno)) {
    const std::string model = match.str(1);
    if (model[0] == 'A') {
      *out = "Adreno (TM) A11";
      return true;
    }
    if (model[0] == 'X') {
      *out = "Adreno (TM) X1-45";
      return true;
    }
    if (!ParseDigits(model, 10, &modelNum)) {
      *out = "Adreno (TM) 225";
      return true;
    }
    if (modelNum >= 600) *out = "Adreno (TM) 650";
    else if (modelNum >= 500) *out = "Adreno (TM) 540";
    else if (modelNum >= 400) *out = "Adreno (TM) 430";
    else if (modelNum >= 300) *out = "Adreno (TM) 330";
    else *out = "Adreno (TM) 225";
    return true;
  }

  static const std::regex kMali("Mali.*?([0-9][0-9]+)");
  if (std::regex_search(text, match, kMali) &&
      ParseDigits(match.str(1), 10, &modelNum)) {
    if (modelNum >= 800) *out = "Mali-T880";
    else if (modelNum >= 700) *out = "Mali-T760";
    else if (modelNum >= 600) *out = "Mali-T628";
    else if (modelNum >= 400) *out = "Mali-400 MP";
    else *out = "Mali-G51";
    return true;
  }

  if (Contains(text, "PowerVR")) {
    *out = Contains(text, "Rogue") ? "PowerVR Rogue G6200" : "PowerVR SGX 540";
    return true;
  }
  if (Contains(text, "Samsung Xclipse")) {
    *out = "Samsung Xclipse 920";
    return true;
  }
  if (Contains(text, "Vivante")) {
    *out = "Vivante GC1000";
    return true;
  }
  if (Contains(text, "VideoCore")) {
    *out = "VideoCore IV HW";
    return true;
  }
  if (Contains(text, "Tegra")) {
    *out = "NVIDIA Tegra";
    return true;
  }
  // The Direct3D WARP adapter is already generic, so the donor keeps it as-is.
  if (Contains(text, "Microsoft Basic Render Driver")) {
    *out = text;
    return true;
  }
  return false;
}

}  // namespace ruyijs::web_graphics
