// The "canvas" profile group (the donor build's --fpfile canvas.* keys) read into
// the Context's FingerprintConfig. Validation follows the donor parser
// (dom/canvas/CanvasFingerprint.cpp): seed is a positive integer, strength
// 0..3, density 1..4096; a field the group leaves out keeps the donor default.
#include "web_canvas/realm.h"
#include "runtime/web_kit.h"
#include "jsapi.h"
#include "js/PropertyAndElement.h"
#include <cmath>

namespace ruyijs::web_canvas {
namespace {
bool Number(JSContext* cx, JS::HandleObject group, const char* name, double low, double high,
            bool* present, double* out) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, group, name, &value)) return false;
  *present = !value.isUndefined();
  if (!*present) return true;
  if (!value.isNumber() || !std::isfinite(value.toNumber()) || value.toNumber() < low ||
      value.toNumber() > high)
    return WebKit::Fail(cx, (std::string("canvas.") + name + " is out of range").c_str());
  *out = value.toNumber();
  return true;
}
bool Flag(JSContext* cx, JS::HandleObject group, const char* name, bool* out) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, group, name, &value)) return false;
  if (value.isBoolean()) *out = value.toBoolean();
  return true;
}
}  // namespace

bool ConfigureFingerprintGroup(void* context, JS::HandleObject group) {
  JSContext* cx = static_cast<JSContext*>(context);
  domain::canvas::FingerprintConfig config;
  JS::RootedValue mode(cx);
  if (!JS_GetProperty(cx, group, "mode", &mode)) return false;
  if (mode.isString()) {
    bool pixel = false;
    if (!JS_StringEqualsAscii(cx, mode.toString(), "pixel", &pixel)) return false;
    config.mode = pixel ? domain::canvas::FingerprintConfig::Mode::Pixel
                        : domain::canvas::FingerprintConfig::Mode::None;
  }
  bool present = false;
  double number = 0;
  if (!Number(cx, group, "seed", 1, 9007199254740991.0, &present, &number)) return false;
  if (present) {
    if (number != std::floor(number)) return WebKit::Fail(cx, "canvas.seed must be an integer");
    config.seed = static_cast<uint64_t>(number);
  }
  if (!Number(cx, group, "strength", 0, 3, &present, &number)) return false;
  if (present) config.strength = static_cast<uint8_t>(std::lround(number));
  if (!Number(cx, group, "density", 1, 4096, &present, &number)) return false;
  if (present) {
    if (number != std::floor(number)) return WebKit::Fail(cx, "canvas.density must be an integer");
    config.density = static_cast<uint32_t>(number);
  }
  if (!Flag(cx, group, "preserveAlpha", &config.preserveAlpha) ||
      !Flag(cx, group, "preserveWhitePoint", &config.preserveWhitePoint) ||
      !Flag(cx, group, "pngMetadata", &config.pngMetadata)) return false;
  ConfigureFingerprint(config);
  return true;
}
}  // namespace ruyijs::web_canvas
