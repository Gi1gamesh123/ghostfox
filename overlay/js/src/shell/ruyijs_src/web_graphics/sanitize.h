#ifndef RUYIJS_WEB_GRAPHICS_SANITIZE_H
#define RUYIJS_WEB_GRAPHICS_SANITIZE_H
#include <string>

namespace ruyijs::web_graphics {
// Ports of the donor's dom/canvas/SanitizeRenderer.cpp. Firefox applies these
// two rules to the driver's raw GL_VENDOR/GL_RENDERER when the renderer
// information is exposed, and they are the only place a fingerprint string is
// rewritten (T16d). The default unmasked strings come from them; the four
// masked strings are Firefox's own constants.
//
// The device decision table is split by vendor family so each translation unit
// stays inside the 200-line gate. Both halves answer false when the string is
// not recognised, which the caller reports as "Generic Renderer".
bool ReplaceDesktopDevice(const std::string& text, std::string* out);
bool ReplaceMobileDevice(const std::string& text, std::string* out);

std::string SanitizeRenderer(const std::string& raw_renderer);
std::string SanitizeVendor(const std::string& raw_vendor);
}  // namespace ruyijs::web_graphics
#endif
