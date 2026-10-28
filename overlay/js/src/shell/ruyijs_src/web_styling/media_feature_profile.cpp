#include "web_styling/media_feature_profile.h"
#include "web_styling/realm.h"
#include "runtime/web_kit.h"
#include "jsapi.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include <mutex>
namespace ruyijs::web_styling {
namespace {
std::mutex& Lock() { static std::mutex lock; return lock; }
std::map<std::string, std::string>& Store() { static std::map<std::string, std::string> store; return store; }
// camelCase profile field -> CSS media feature name.
struct Field { const char* field; const char* feature; };
constexpr Field kFields[] = {
    {"prefersColorScheme", "prefers-color-scheme"}, {"prefersReducedMotion", "prefers-reduced-motion"},
    {"prefersContrast", "prefers-contrast"}, {"prefersReducedTransparency", "prefers-reduced-transparency"},
    {"forcedColors", "forced-colors"}, {"invertedColors", "inverted-colors"},
    {"colorGamut", "color-gamut"}, {"videoColorGamut", "video-color-gamut"},
    {"dynamicRange", "dynamic-range"}, {"videoDynamicRange", "video-dynamic-range"},
    {"displayMode", "display-mode"}, {"update", "update"}, {"scripting", "scripting"},
    {"pointer", "pointer"}, {"anyPointer", "any-pointer"}, {"hover", "hover"}, {"anyHover", "any-hover"},
    {"overflowBlock", "overflow-block"}, {"overflowInline", "overflow-inline"},
};
}  // namespace

std::map<std::string, std::string> MediaFeatureProfile() {
  std::lock_guard<std::mutex> guard(Lock());
  return Store();
}
void SetMediaFeatureProfile(std::map<std::string, std::string> profile) {
  std::lock_guard<std::mutex> guard(Lock());
  Store() = std::move(profile);
}

bool ConfigureProfileGroup(void* context, JS::HandleObject group) {
  JSContext* cx = static_cast<JSContext*>(context);
  std::map<std::string, std::string> profile;
  JS::RootedValue value(cx);
  for (const Field& f : kFields) {
    if (!JS_GetProperty(cx, group, f.field, &value)) return false;
    if (value.isUndefined()) continue;
    if (!value.isString())
      return WebKit::Fail(cx, (std::string("mediaFeatures.") + f.field + " must be a string").c_str());
    JS::RootedString string(cx, value.toString());
    JS::UniqueChars text = JS_EncodeStringToUTF8(cx, string);
    if (!text) return false;
    profile[f.feature] = text.get();
  }
  SetMediaFeatureProfile(std::move(profile));
  return true;
}
}  // namespace ruyijs::web_styling
