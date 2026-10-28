#ifndef RUYIJS_WEB_GRAPHICS_MODEL_H
#define RUYIJS_WEB_GRAPHICS_MODEL_H
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ruyijs::web_graphics {
// The six environment-profile fields the "webgl" group configures. The names
// track the donor fingerprint-file keys webgl.vendor / webgl.renderer /
// webgl.unmasked_vendor / webgl.unmasked_renderer, so one capture profile can
// feed both the donor Firefox and ruyijs. The numeric limits and the extension
// list are read-only source data.
inline constexpr int32_t kVendorField = 0;
inline constexpr int32_t kRendererField = 1;
inline constexpr int32_t kVersionField = 2;
inline constexpr int32_t kShadingField = 3;
inline constexpr int32_t kUnmaskedVendorField = 4;
inline constexpr int32_t kUnmaskedRendererField = 5;
// The WebGL2 VERSION / SHADING_LANGUAGE_VERSION pair and the
// WebGLContextAttributes dictionary are the other two profile fields of the
// same "webgl" group.
inline constexpr int32_t kVersion2Field = 6;
inline constexpr int32_t kShading2Field = 7;
// The "webgl" group's field order continues with the attribute dictionary and
// the two extension lists; none of them is one of the string overrides, which
// are kOverrideCount long.
inline constexpr int32_t kAttributeDictionaryField = 8;
inline constexpr int32_t kExtensionListField = 9;
inline constexpr int32_t kExtension2ListField = 10;
inline constexpr int32_t kLimitMapField = 11;
inline constexpr int32_t kOverrideCount = 8;
// getContextAttributes() members in WebIDL order; powerPreference is the only
// non-boolean one.
enum AttributeIndex { kAlpha = 0, kAntialias, kDepth, kFailIfMajorPerformanceCaveat,
                      kPowerPreference, kPremultipliedAlpha, kPreserveDrawingBuffer,
                      kStencil, kAttributeCount };
struct Attributes {
  bool alpha = true, antialias = true, depth = true,
       fail_if_major_performance_caveat = false, premultiplied_alpha = true,
       preserve_drawing_buffer = false, stencil = false;
  std::string power_preference = "default";
};

// The effective WebGL parameters. VENDOR and the version strings are Firefox's
// own constants; RENDERER / UNMASKED_RENDERER are the donor SanitizeRenderer
// output and UNMASKED_VENDOR the imported driver vendor, with the numeric
// limits from the imported platform_apis::GlParameters. This noexcept facade
// keeps the throwing imported header inside model.cpp, the single translation
// unit that includes it.
struct GlValues {
  std::string vendor, renderer, version, shading_language_version;
  std::string version2, shading_language_version2;
  std::string unmasked_vendor, unmasked_renderer;
  int32_t max_texture_size = 0, max_renderbuffer_size = 0;
  int32_t max_viewport_dims_width = 0, max_viewport_dims_height = 0;
  int32_t max_vertex_attribs = 0, max_texture_image_units = 0;
  double max_texture_max_anisotropy = 0.0;
  Attributes attributes;
  std::vector<std::string> supported_extensions;
  std::vector<std::string> supported_extensions2;
};
// Public ruyijs defaults for the additional query surface. Numeric arrays retain
// their GL result type; this table has no engine or JSON dependency. A
// non-null `extension` names the WebGL extension that must be enabled through
// getExtension before Firefox answers the pname (WebGLContextState.cpp
// GetParameter); WebGL2 contexts skip the gate when `core2` is set.
enum class ParameterKind { Number, Boolean, FloatArray, IntArray, BoolArray, Null };
struct ParameterDefault {
  int32_t name; ParameterKind kind; int count; double values[4];
  const char* extension = nullptr; bool core2 = false;
};
const ParameterDefault* PublicParameterDefault(int32_t name) noexcept;
const ParameterDefault* Webgl2ParameterDefault(int32_t name) noexcept;

class Model final {
 public:
  Model() noexcept;
  ~Model() noexcept;
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;
  bool Initialize() noexcept;
  bool Ready() const noexcept;
  const GlValues& values() const noexcept;
  // Applies one configured string field. The caller has already validated the
  // field index; allocation failure is reported as false, never as an exception.
  bool Override(int32_t field, const std::string& text) noexcept;
  // Applies one configured WebGLContextAttributes member; the profile schema
  // has already validated the type and the powerPreference allow-list.
  bool OverrideAttribute(int32_t index, bool value) noexcept;
  bool OverridePowerPreference(const std::string& text) noexcept;
  // Replaces one extension list (WebGL1 when `second` is false). Names are
  // taken verbatim: an unknown name simply never matches getExtension, which is
  // how a profile removes an extension from the reported surface.
  bool OverrideExtensions(bool second, const std::vector<std::string>& names) noexcept;
  // One getParameter override keyed by the decimal pname. Only a pname whose
  // built-in answer is a plain number is accepted: the array-valued state
  // queries keep their shape.
  bool OverrideLimit(const std::string& name, double value) noexcept;
  bool Limit(int32_t name, double* out) const noexcept;
  const std::map<int32_t, double>& limits() const noexcept;
 private:
  struct Impl;
  Impl* impl_ = nullptr;
};
}  // namespace ruyijs::web_graphics
#endif
