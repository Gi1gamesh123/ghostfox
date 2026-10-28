#pragma once
#include <array>

namespace ruyijs::domain::webgpu {
// Firefox 157 capability baseline. Naga's ImplementedLanguageExtension order,
// with ImmediateAddressSpace excluded by gfx/wgpu_bindings/src/client.rs.
// Exposing this baseline describes the emulated environment, not a GPU backend.
inline constexpr std::array<const char*, 3> kWgslLanguageFeatures = {
    "readonly_and_readwrite_storage_textures",
    "packed_4x8_integer_dot_product",
    "pointer_composite_access",
};
}
