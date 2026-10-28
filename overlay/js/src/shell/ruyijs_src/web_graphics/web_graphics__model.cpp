// The single translation unit that includes the imported platform_apis header.
// It is compiled with exceptions enabled; every entry point below is noexcept,
// so no imported exception reaches the SpiderMonkey adapter.
#include "web_graphics/model.h"

#include "domain/platform_apis/platform_apis.h"
#include "web_graphics/sanitize.h"
#include <map>

namespace ruyijs::web_graphics {
namespace domain = ruyijs::domain::platform_apis;

namespace {
// The masked VENDOR Firefox reports before any fingerprint-file override and
// the per-version VERSION / SHADING_LANGUAGE_VERSION constants
// (ClientWebGLContext.cpp GetParameter).
const char* const kMaskedVendor = "Mozilla";
const char* const kVersion = "WebGL 1.0";
const char* const kShadingLanguageVersion = "WebGL GLSL ES 1.0";
const char* const kVersion2 = "WebGL 2.0";
const char* const kShadingLanguageVersion2 = "WebGL GLSL ES 3.00";

// The extension names Firefox 158 reports to content on Windows with an ANGLE
// D3D11 context (domtrace jscall-detail-desktop-20261019-230430,
// getSupportedExtensions on both context versions). The order is the
// getSupportedExtensions order, which sorts the names ASCII case-insensitively.
// Privileged (MOZ_debug, EXT_disjoint_timer_query) and draft
// (EXT_texture_norm16, WEBGL_explicit_present) ids stay absent.
const char* const kWebGL1Extensions[] = {
    "ANGLE_instanced_arrays", "EXT_blend_minmax", "EXT_color_buffer_half_float",
    "EXT_depth_clamp", "EXT_float_blend", "EXT_frag_depth",
    "EXT_polygon_offset_clamp", "EXT_shader_texture_lod", "EXT_sRGB",
    "EXT_texture_compression_bptc", "EXT_texture_compression_rgtc",
    "EXT_texture_filter_anisotropic", "OES_element_index_uint",
    "OES_fbo_render_mipmap", "OES_standard_derivatives", "OES_texture_float",
    "OES_texture_float_linear", "OES_texture_half_float",
    "OES_texture_half_float_linear", "OES_vertex_array_object",
    "WEBGL_color_buffer_float", "WEBGL_compressed_texture_s3tc",
    "WEBGL_compressed_texture_s3tc_srgb", "WEBGL_debug_renderer_info",
    "WEBGL_debug_shaders", "WEBGL_depth_texture", "WEBGL_draw_buffers",
    "WEBGL_lose_context", "WEBGL_provoking_vertex"};
const char* const kWebGL2Extensions[] = {
    "EXT_color_buffer_float", "EXT_depth_clamp", "EXT_float_blend",
    "EXT_polygon_offset_clamp", "EXT_texture_compression_bptc",
    "EXT_texture_compression_rgtc", "EXT_texture_filter_anisotropic",
    "OES_draw_buffers_indexed", "OES_texture_float_linear", "OVR_multiview2",
    "WEBGL_compressed_texture_s3tc", "WEBGL_compressed_texture_s3tc_srgb",
    "WEBGL_debug_renderer_info", "WEBGL_debug_shaders", "WEBGL_lose_context",
    "WEBGL_provoking_vertex"};
// Firefox 158 limits the ANGLE D3D11 viewport to the D3D11 resource limit
// (MAX_VIEWPORT_DIMS reported 32767 in the same trace).
const int32_t kAngleMaxViewportDim = 32767;
}  // namespace

struct Model::Impl {
  // The imported value object supplies the raw driver strings that the donor
  // sanitizers fold into the UNMASKED_* pair, plus the numeric limits. The
  // effective strings and the extension list are Firefox's own (deviation
  // D36); this adapter keeps no literal that duplicates the imported header.
  domain::GlParameters source;
  std::string overrides[kOverrideCount];
  bool set[kOverrideCount] = {};
  // The attribute dictionary's one string member, kept apart from the GL string
  // overrides because it has its own field in the profile group.
  std::string power_preference;
  bool power_preference_set = false;
  // A configured extension list replaces the built-in one (one list per
  // context version).
  std::vector<std::string> extensions[2];
  bool extensions_set[2] = {};
  // "webgl.limits": the configured getParameter answers, by pname.
  std::map<int32_t, double> limits;
  bool attributes[kAttributeCount] = {};
  bool attribute_set[kAttributeCount] = {};
  GlValues values;

  void Rebuild() {
    // ClientWebGLContext::GetParameter without RFP: VENDOR stays "Mozilla",
    // RENDERER and UNMASKED_RENDERER are SanitizeRenderer(driver renderer) and
    // UNMASKED_VENDOR is the driver vendor unchanged (SanitizeVendor only runs
    // under RFPTarget::WebGLVendorSanitize).
    const std::string sanitized = SanitizeRenderer(source.renderer);
    values.vendor = set[kVendorField] ? overrides[kVendorField] : kMaskedVendor;
    values.renderer = set[kRendererField] ? overrides[kRendererField] : sanitized;
    values.version = set[kVersionField] ? overrides[kVersionField] : kVersion;
    values.shading_language_version =
        set[kShadingField] ? overrides[kShadingField] : kShadingLanguageVersion;
    values.version2 = set[kVersion2Field] ? overrides[kVersion2Field] : kVersion2;
    values.shading_language_version2 = set[kShading2Field]
        ? overrides[kShading2Field] : kShadingLanguageVersion2;
    values.unmasked_vendor = set[kUnmaskedVendorField]
                                 ? overrides[kUnmaskedVendorField]
                                 : source.vendor;
    values.unmasked_renderer = set[kUnmaskedRendererField]
                                   ? overrides[kUnmaskedRendererField]
                                   : sanitized;
    values.max_texture_size = source.max_texture_size;
    values.max_renderbuffer_size = source.max_renderbuffer_size;
    values.max_viewport_dims_width = kAngleMaxViewportDim;
    values.max_viewport_dims_height = kAngleMaxViewportDim;
    values.max_vertex_attribs = source.max_vertex_attribs;
    values.max_texture_image_units = source.max_texture_image_units;
    values.max_texture_max_anisotropy = source.max_texture_max_anisotropy;
    values.supported_extensions.clear();
    for (const char* name : kWebGL1Extensions) {
      values.supported_extensions.push_back(name);
    }
    values.supported_extensions2.assign(std::begin(kWebGL2Extensions), std::end(kWebGL2Extensions));
    if (extensions_set[0]) values.supported_extensions = extensions[0];
    if (extensions_set[1]) values.supported_extensions2 = extensions[1];
    // getContextAttributes(): the dictionary Firefox reports for a context
    // created without arguments, with the profile's members applied.
    const Attributes defaults;
    const bool* members[] = {
        &defaults.alpha, &defaults.antialias, &defaults.depth,
        &defaults.fail_if_major_performance_caveat, nullptr,
        &defaults.premultiplied_alpha, &defaults.preserve_drawing_buffer,
        &defaults.stencil};
    bool* target[] = {
        &values.attributes.alpha, &values.attributes.antialias,
        &values.attributes.depth,
        &values.attributes.fail_if_major_performance_caveat, nullptr,
        &values.attributes.premultiplied_alpha,
        &values.attributes.preserve_drawing_buffer, &values.attributes.stencil};
    for (int32_t index = 0; index < kAttributeCount; ++index)
      if (target[index])
        *target[index] = attribute_set[index] ? attributes[index] : *members[index];
    values.attributes.power_preference =
        power_preference_set ? power_preference : defaults.power_preference;
  }
};

Model::Model() noexcept {
  try {
    impl_ = new Impl;
    impl_->Rebuild();
  } catch (...) {
    delete impl_;
    impl_ = nullptr;
  }
}
Model::~Model() noexcept { delete impl_; }
bool Model::Initialize() noexcept { return impl_ != nullptr; }
bool Model::Ready() const noexcept { return impl_ != nullptr; }
const GlValues& Model::values() const noexcept {
  static const GlValues kEmpty{};
  return impl_ ? impl_->values : kEmpty;
}
bool Model::OverrideAttribute(int32_t index, bool value) noexcept {
  if (!impl_ || index < 0 || index >= kAttributeCount) return false;
  try {
    impl_->attributes[index] = value;
    impl_->attribute_set[index] = true;
    impl_->Rebuild();
    return true;
  } catch (...) {
    return false;
  }
}
bool Model::OverrideExtensions(bool second, const std::vector<std::string>& names) noexcept {
  if (!impl_) return false;
  try {
    impl_->extensions[second ? 1 : 0] = names;
    impl_->extensions_set[second ? 1 : 0] = true;
    impl_->Rebuild();
    return true;
  } catch (...) {
    return false;
  }
}
bool Model::OverrideLimit(const std::string& name, double value) noexcept {
  if (!impl_) return false;
  try {
    // A pname is written in decimal in the profile; anything else is rejected.
    if (name.empty() || name.size() > 10) return false;
    int64_t parsed = 0;
    for (char digit : name) {
      if (digit < '0' || digit > '9') return false;
      parsed = parsed * 10 + (digit - '0');
    }
    if (parsed < 0 || parsed > 0xFFFFFFFFll) return false;
    const int32_t pname = int32_t(parsed);
    // The implementation limits getParameter answers from the imported value
    // object (MAX_VIEWPORT_DIMS pins both dimensions), or a numeric table entry.
    // MAX_TEXTURE_SIZE, MAX_RENDERBUFFER_SIZE, MAX_VERTEX_ATTRIBS,
    // MAX_TEXTURE_IMAGE_UNITS, MAX_TEXTURE_MAX_ANISOTROPY_EXT, MAX_VIEWPORT_DIMS.
    const bool core = pname == 0x0D33 || pname == 0x84E8 || pname == 0x8869 ||
        pname == 0x8872 || pname == 0x84FF || pname == 0x0D3A;
    const ParameterDefault* def = PublicParameterDefault(pname);
    if (!def) def = Webgl2ParameterDefault(pname);
    if (!core && (!def || def->kind != ParameterKind::Number)) return false;
    impl_->limits[pname] = value;
    return true;
  } catch (...) {
    return false;
  }
}
const std::map<int32_t, double>& Model::limits() const noexcept {
  static const std::map<int32_t, double> kEmpty;
  return impl_ ? impl_->limits : kEmpty;
}
bool Model::Limit(int32_t name, double* out) const noexcept {
  if (!impl_) return false;
  const auto entry = impl_->limits.find(name);
  if (entry == impl_->limits.end()) return false;
  *out = entry->second;
  return true;
}
bool Model::OverridePowerPreference(const std::string& text) noexcept {
  // The allow-list is the schema's; an unknown value never reaches the model.
  if (!impl_ || (text != "default" && text != "low-power" && text != "high-performance"))
    return false;
  try {
    impl_->power_preference = text;
    impl_->power_preference_set = true;
    impl_->Rebuild();
    return true;
  } catch (...) {
    return false;
  }
}
bool Model::Override(int32_t field, const std::string& text) noexcept {
  if (!impl_ || field < 0 || field >= kOverrideCount) return false;
  try {
    impl_->overrides[field] = text;
    impl_->set[field] = true;
    impl_->Rebuild();
    return true;
  } catch (...) {
    return false;
  }
}
}  // namespace ruyijs::web_graphics
