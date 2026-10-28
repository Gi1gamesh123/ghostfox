#ifndef RUYIJS_WEB_GRAPHICS_DRAW_VERTEX_H
#define RUYIJS_WEB_GRAPHICS_DRAW_VERTEX_H
#include <cstdint>
#include <vector>

#include "js/TypeDecls.h"
#include "web_graphics/glsl.h"
#include "web_graphics/state.h"

namespace ruyijs::web_graphics::draw_detail {
// The component count of a declared type ("vec2" -> 2, everything else 1).
int32_t Components(const std::string& type);
// Reads one vertex of the bound array buffer through the attribute pointers.
bool ReadAttributes(JSContext*, Context&, const std::vector<GlslDeclaration>& attributes,
                    int32_t first, int32_t vertex, std::vector<GlslValue>* out);
// The float32 varying at one point of the triangle, from its three corners.
void Interpolate(const double w[3], const std::vector<GlslValue>& corners, size_t count,
                 const std::vector<GlslDeclaration>& names, std::vector<GlslValue>* out);
}  // namespace ruyijs::web_graphics::draw_detail
#endif
