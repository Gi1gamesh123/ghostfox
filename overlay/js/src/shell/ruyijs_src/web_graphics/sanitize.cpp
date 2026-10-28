#include "web_graphics/sanitize.h"

#include <regex>
#include <string>

#include "web_graphics/sanitize_internal.h"

namespace ruyijs::web_graphics {
namespace {

using sanitize_detail::Contains;

// The donor answers these two generic cases before the vendor tables, then
// walks the two halves of the device table in order.
bool Device(const std::string& text, std::string* out) {
  if (text.find("llvmpipe") == 0) {
    *out = "llvmpipe";
    return true;
  }
  if (text.find("Apple") == 0) {
    *out = "Apple M1";
    return true;
  }
  return ReplaceDesktopDevice(text, out) || ReplaceMobileDevice(text, out);
}

}  // namespace

// The donor's SanitizeRenderer: parse the driver string, replace the device
// with a representative bucket and append Firefox's ", or similar" marker. An
// ANGLE-shaped string that cannot be parsed answers "Generic Renderer", exactly
// as the donor does (and without the marker).
std::string SanitizeRenderer(const std::string& raw_renderer) {
  const char* GENERIC_RENDERER = "Generic Renderer";
  std::smatch match;
  std::string device;

  // The donor pattern ends with ", .*[)]" because a real ANGLE D3D11 string
  // carries the trailing driver field ("..., D3D11-27.20.1020.2002"). The
  // imported GlParameters snapshot stores only the device part, so the trailing
  // field is optional here; every other group matches the donor exactly (D36).
  static const std::regex kReAngleDirect3D(
      "ANGLE [(]([^,]*), ([^,]*)( Direct3D[^,]*)(, .*)?[)]");
  static const std::regex kReAngleVulkan(
      "ANGLE [(]+(.*)[)]( on Vulkan) [0-9\\.]*[)]*");
  static const std::regex kReAngleMetal(
      "ANGLE [(]([^,]*), ANGLE Metal Renderer: ([^,]*), Version .*[)]");
  if (std::regex_match(raw_renderer, match, kReAngleDirect3D)) {
    const bool known = Device(match.str(2), &device);
    return "ANGLE (" + match.str(1) + ", " + (known ? device : GENERIC_RENDERER) +
           match.str(3) + "), or similar";
  }
  if (std::regex_match(raw_renderer, match, kReAngleVulkan)) {
    const bool known = Device(match.str(1), &device);
    return "ANGLE (" + (known ? device : GENERIC_RENDERER) + ")" + match.str(2) +
           ", or similar";
  }
  if (std::regex_match(raw_renderer, match, kReAngleMetal)) {
    const bool known = Device(match.str(2), &device);
    return "ANGLE (" + match.str(1) + ", ANGLE Metal Renderer: " +
           (known ? device : GENERIC_RENDERER) + "), or similar";
  }
  if (Contains(raw_renderer, "ANGLE")) return GENERIC_RENDERER;

  static const std::regex kReOpenglEngine("(.*) OpenGL Engine");
  static const std::regex kRePcieSse2("(.*)(/PCIe?/SSE2)");
  static const std::regex kReStandard("(.*)( [(].*[)])");
  if (std::regex_match(raw_renderer, match, kReOpenglEngine) ||
      std::regex_match(raw_renderer, match, kRePcieSse2)) {
    if (!Device(match.str(1), &device)) return GENERIC_RENDERER;
    return device + ", or similar";
  }
  if (std::regex_match(raw_renderer, match, kReStandard)) {
    if (!Device(match.str(1), &device)) return GENERIC_RENDERER;
    return device + ", or similar";
  }
  if (!Device(raw_renderer, &device)) return GENERIC_RENDERER;
  return device + ", or similar";
}

// The donor's SanitizeVendor: fold a raw vendor string into its bucket.
std::string SanitizeVendor(const std::string& raw_vendor) {
  if (Contains(raw_vendor, "NVIDIA")) return "NVIDIA Corporation";
  if (Contains(raw_vendor, "Intel")) return "Intel";
  if (Contains(raw_vendor, "AMD") || Contains(raw_vendor, "ATI") ||
      Contains(raw_vendor, "Advanced Micro Devices"))
    return "AMD";
  if (Contains(raw_vendor, "Qualcomm")) return "Qualcomm";
  if (Contains(raw_vendor, "ARM")) return "ARM";
  if (Contains(raw_vendor, "Apple")) return "Apple";
  if (Contains(raw_vendor, "Samsung")) return "Samsung";
  if (Contains(raw_vendor, "Mesa") || Contains(raw_vendor, "X.Org")) return "Mesa";
  if (Contains(raw_vendor, "Microsoft")) return "Microsoft";
  if (Contains(raw_vendor, "VMware")) return "VMware";
  if (Contains(raw_vendor, "Google")) return "Google";
  return "Other";
}

}  // namespace ruyijs::web_graphics
