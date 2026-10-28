// The vertex half of the software rasterizer: fetching one vertex of the bound
// array buffer through the attribute pointers, and interpolating the varyings a
// triangle carries at one point. draw.cpp walks the strip with these.
#include "web_graphics/draw_vertex.h"

#include <cstring>

#include "js/Object.h"
#include "js/experimental/TypedData.h"
#include "js/PropertyAndElement.h"
#include "web_graphics/internal.h"

namespace ruyijs::web_graphics::draw_detail {
int32_t Components(const std::string& type) {
  return type == "vec4" ? 4 : type == "vec3" ? 3 : type == "vec2" ? 2 : 1;
}

bool ReadAttributes(JSContext*, Context& context, const std::vector<GlslDeclaration>& attributes,
                    int32_t first, int32_t vertex, std::vector<GlslValue>* out) {
  std::vector<uint8_t> buffer;
  if (context.arrayBinding) {
    JS::Value data = JS::GetReservedSlot(context.arrayBinding, detail::kObjectDataSlot);
    if (data.isObject()) {
      const size_t length = JS_GetTypedArrayByteLength(&data.toObject());
      JS::AutoCheckCannotGC nogc;
      bool shared = false;
      const uint8_t* bytes = static_cast<const uint8_t*>(
          JS_GetArrayBufferViewData(&data.toObject(), &shared, nogc));
      if (bytes && length) buffer.assign(bytes, bytes + length);
    }
  }
  out->assign(attributes.size(), GlslValue());
  for (size_t i = 0; i < attributes.size(); ++i) {
    GlslValue& value = (*out)[i];
    value.count = Components(attributes[i].type);
    // A disabled array keeps WebGL's generic vertex attribute (0, 0, 0, 1).
    value.value[3] = 1.0f;
    if (i >= context.attributeArrays.size()) continue;
    const AttributeArray& pointer = context.attributeArrays[i];
    if (!pointer.enabled) continue;
    const size_t base = size_t(pointer.offset) + size_t(vertex + first) * size_t(pointer.stride);
    for (int32_t c = 0; c < pointer.size && c < 4; ++c) {
      const size_t offset = base + size_t(c) * 4u;
      if (offset + 4u > buffer.size()) break;
      float component = 0.0f;
      std::memcpy(&component, buffer.data() + offset, 4);
      value.value[c] = component;
    }
    if (pointer.size < 4) value.value[3] = 1.0f;
  }
  return true;
}

void Interpolate(const double w[3], const std::vector<GlslValue>& corners, size_t count,
                 const std::vector<GlslDeclaration>& names, std::vector<GlslValue>* out) {
  for (size_t i = 0; i < count; ++i) {
    GlslValue value;
    value.count = Components(names[i].type);
    for (int c = 0; c < value.count; ++c)
      value.value[c] = float(w[0] * corners[i].value[c] +
                             w[1] * corners[count + i].value[c] +
                             w[2] * corners[2 * count + i].value[c]);
    (*out)[i] = value;
  }
}
}  // namespace ruyijs::web_graphics::draw_detail
