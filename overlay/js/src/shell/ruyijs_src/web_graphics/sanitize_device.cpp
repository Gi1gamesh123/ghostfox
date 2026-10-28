#include "web_graphics/sanitize.h"

#include <regex>
#include <string>

#include "web_graphics/sanitize_internal.h"

namespace ruyijs::web_graphics {
namespace {
using sanitize_detail::Contains;
using sanitize_detail::ParseDigits;
}  // namespace

// The discrete desktop vendors: AMD (Radeon/FirePro) then NVIDIA (GeForce,
// Quadro, Titan, and the Nouveau NVxxx names). Ported decision-for-decision
// from the donor's ChooseDeviceReplacement.
bool ReplaceDesktopDevice(const std::string& text, std::string* out) {
  std::smatch match;
  const char* RADEON_HD_3000 = "Radeon HD 3200 Graphics";
  const char* RADEON_HD_5850 = "Radeon HD 5850";
  const char* RADEON_R9_290 = "Radeon R9 200 Series";
  const char* RADEON_D3D_FL10_1 = "Radeon HD 3200 Graphics";
  const char* GEFORCE_8800 = "GeForce 8800 GTX";
  const char* GEFORCE_480 = "GeForce GTX 480";
  const char* GEFORCE_980 = "GeForce GTX 980";
  unsigned long modelNum = 0;

  if (Contains(text, "REMBRANDT") || Contains(text, "RENOIR") ||
      Contains(text, "Vega") || Contains(text, "VII") || Contains(text, "Fury")) {
    *out = RADEON_R9_290;
    return true;
  }
  static const std::regex kRadeon("Radeon.*?((R[579X]|HD) )?([0-9][0-9][0-9]+)");
  if (std::regex_search(text, match, kRadeon)) {
    const std::string rxOrHd = match.str(2);
    if (!ParseDigits(match.str(3), 10, &modelNum)) {
      *out = RADEON_D3D_FL10_1;
      return true;
    }
    if (rxOrHd == "HD") {
      if (modelNum >= 5000) *out = RADEON_HD_5850;
      else if (modelNum >= 3000) *out = RADEON_HD_3000;
      else *out = RADEON_D3D_FL10_1;
      return true;
    }
    *out = RADEON_R9_290;
    return true;
  }
  static const std::regex kFirePro("FirePro.*?([VDW])[0-9][0-9][0-9]+");
  if (std::regex_search(text, match, kFirePro)) {
    *out = match.str(1) == "V" ? RADEON_D3D_FL10_1 : RADEON_R9_290;
    return true;
  }
  if (Contains(text, "ARUBA")) {
    *out = RADEON_HD_5850;
    return true;
  }
  if (Contains(text, "AMD ") || Contains(text, "FirePro") || Contains(text, "Radeon")) {
    *out = RADEON_D3D_FL10_1;
    return true;
  }

  static const std::regex kGeForce("GeForce.*?([0-9][0-9][0-9]+)");
  static const std::regex kQuadro("Quadro.*?([KMPVT]?)[0-9][0-9][0-9]+");
  static const std::regex kTitan("TITAN( [BZXVR])?");
  if (Contains(text, "NVIDIA") || Contains(text, "GeForce") ||
      Contains(text, "Quadro")) {
    std::string device;
    if (std::regex_search(text, match, kGeForce) &&
        ParseDigits(match.str(1), 10, &modelNum)) {
      if (modelNum >= 8000) device = GEFORCE_8800;
      else if (modelNum >= 900) device = GEFORCE_980;
      else if (modelNum >= 400) device = GEFORCE_480;
      else device = GEFORCE_8800;
    } else if (std::regex_search(text, match, kQuadro)) {
      if (Contains(text, "RTX")) {
        device = GEFORCE_980;
      } else {
        const std::string architecture = match.str(1);
        if (architecture.empty()) device = GEFORCE_8800;
        else switch (architecture[0]) {
            case 'M':
            case 'P':
            case 'V':
            case 'T':
              device = GEFORCE_980;
              break;
            default:
              device = GEFORCE_480;
              break;
          }
      }
    } else if (std::regex_search(text, match, kTitan)) {
      const std::string suffix = match.str(1);
      const char letter = suffix.length() > 1 ? suffix[1] : ' ';
      device = (letter == ' ' || letter == 'B' || letter == 'Z') ? GEFORCE_480
                                                                : GEFORCE_980;
    } else {
      // Unknown, but NV: the donor answers the oldest bucket, and so do we.
      device = GEFORCE_8800;
    }
    if (text.find("NVIDIA") == 0) device = "NVIDIA " + device;
    *out = device;
    return true;
  }

  static const std::regex kNouveau("NV(1?[0-9A-F][0-9A-F])");
  if (std::regex_match(text, match, kNouveau) &&
      ParseDigits(match.str(1), 16, &modelNum)) {
    if (modelNum >= 0x120) *out = GEFORCE_980;
    else if (modelNum >= 0xC0) *out = GEFORCE_480;
    else *out = GEFORCE_8800;
    return true;
  }
  return false;
}

}  // namespace ruyijs::web_graphics
