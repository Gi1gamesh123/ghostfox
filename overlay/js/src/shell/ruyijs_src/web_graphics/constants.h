#ifndef RUYIJS_WEB_GRAPHICS_CONSTANTS_H
#define RUYIJS_WEB_GRAPHICS_CONSTANTS_H
#include <cstdint>

namespace ruyijs::web_graphics {
// The WebGL 1.0 enum identifiers this domain answers. These are specification
// constants, not environment data: every fingerprint-bearing value comes from
// the imported platform_apis::GlParameters through model.cpp.
inline constexpr int32_t kNoError = 0;
inline constexpr int32_t kInvalidEnum = 0x0500;
inline constexpr int32_t kInvalidValue = 0x0501;
inline constexpr int32_t kInvalidOperation = 0x0502;
inline constexpr int32_t kOutOfMemory = 0x0505;
inline constexpr int32_t kInvalidFramebufferOperation = 0x0506;
inline constexpr int32_t kContextLostWebGL = 0x9242;
inline constexpr int32_t kVendor = 0x1F00;
inline constexpr int32_t kRenderer = 0x1F01;
inline constexpr int32_t kVersion = 0x1F02;
inline constexpr int32_t kShadingLanguageVersion = 0x8B8C;
inline constexpr int32_t kMaxTextureSize = 0x0D33;
inline constexpr int32_t kMaxViewportDims = 0x0D3A;
inline constexpr int32_t kMaxRenderbufferSize = 0x84E8;
inline constexpr int32_t kMaxVertexAttribs = 0x8869;
inline constexpr int32_t kMaxTextureImageUnits = 0x8872;
inline constexpr int32_t kMaxTextureMaxAnisotropy = 0x84FF;
inline constexpr int32_t kUnmaskedVendorWebGL = 0x9245;
inline constexpr int32_t kUnmaskedRendererWebGL = 0x9246;
inline constexpr int32_t kArrayBuffer = 0x8892;
inline constexpr int32_t kElementArrayBuffer = 0x8893;
inline constexpr int32_t kVertexShader = 0x8B31;
inline constexpr int32_t kFragmentShader = 0x8B30;
}  // namespace ruyijs::web_graphics
#endif