// The software rasterizer behind drawArrays: it runs the parsed vertex shader for
// every vertex of the strip, rasterizes each triangle with the D3D11 4x MSAA
// sample pattern (the ANGLE default framebuffer is antialias:true), shades once
// per pixel at the pixel centre the way the D3D11 rasterizer does, and resolves
// the four covered samples into the premultiplied RGBA8 drawing buffer.
#include <algorithm>
#include <cmath>
#include <cstring>

#include "web_graphics/draw_sample.h"
#include "web_graphics/draw_vertex.h"
#include "web_graphics/glsl.h"
#include "web_graphics/internal.h"

namespace ruyijs::web_graphics {
namespace {
using detail::kObjectDataSlot;
using detail::kObjectExtraSlot;
using detail::kObjectFlagSlot;
using draw_detail::Components;
using draw_detail::Coverage;
using draw_detail::Edge;
using draw_detail::Interpolate;
using draw_detail::ReadAttributes;
using draw_detail::Resolve8;
using draw_detail::Unorm8;

// The names one stage can resolve: the vertex stage's attributes, uniforms and
// varyings, and the fragment stage's uniforms and interpolated varyings.
struct ShadeEnv {
  Context* context = nullptr;
  std::vector<GlslDeclaration> attributes, uniforms, varyings;
  std::vector<GlslValue> attributeValues, varyingValues;
  GlslValue uniformValue, colour;
};

const GlslValue* Lookup(void* raw, const char* name, size_t length) {
  auto& env = *static_cast<ShadeEnv*>(raw);
  const std::string text(name, length);
  if (text == "gl_FragColor") return &env.colour;
  for (size_t i = 0; i < env.attributes.size(); ++i)
    if (env.attributes[i].name == text) return &env.attributeValues[i];
  for (size_t i = 0; i < env.varyings.size(); ++i)
    if (env.varyings[i].name == text) return &env.varyingValues[i];
  for (const GlslDeclaration& uniform : env.uniforms) {
    if (uniform.name != text) continue;
    env.uniformValue = GlslValue();
    env.uniformValue.count = uniform.type == "vec4" ? 4 : uniform.type == "vec3" ? 3 :
                             uniform.type == "vec2" ? 2 : 1;
    for (const UniformSlot& slot : env.context->uniforms)
      if (slot.name == text)
        for (int i = 0; i < 4; ++i) env.uniformValue.value[i] = slot.value[i];
    return &env.uniformValue;
  }
  return nullptr;
}

// Runs the stage's assignments, storing its varyings and returning the value
// assigned to gl_Position (vertex stage) or gl_FragColor (fragment stage).
bool RunStage(const GlslStage& stage, ShadeEnv& env, GlslValue* result) {
  for (const GlslStatement& statement : stage.statements) {
    GlslValue value;
    if (!GlslEvaluate(statement.expression, &Lookup, &env, &value)) return false;
    if (statement.target == "gl_Position" || statement.target == "gl_FragColor") {
      *result = value;
      continue;
    }
    bool stored = false;
    for (size_t i = 0; i < env.varyings.size(); ++i)
      if (env.varyings[i].name == statement.target) {
        env.varyingValues[i] = value;
        stored = true;
      }
    if (!stored) return false;
  }
  return true;
}
}  // namespace

void ResizeDrawingBuffer(Context& context, int32_t width, int32_t height) {
  const int32_t next = width > 0 ? width : 0, nextHeight = height > 0 ? height : 0;
  if (context.drawingWidth == next && context.drawingHeight == nextHeight) return;
  context.drawingWidth = next;
  context.drawingHeight = nextHeight;
  // A fresh buffer is all zeroes: WebGL's initial clear colour is (0,0,0,0).
  context.drawingBuffer.assign(size_t(next) * size_t(nextHeight) * 4u, 0);
}

bool DrawTriangleStrip(State& state, Context& context, int32_t first, int32_t count) {
  if (!context.currentProgram || count < 3 || first < 0) return true;
  JS::RootedObject program(state.cx, context.currentProgram);
  if (!JS::GetReservedSlot(program, kObjectFlagSlot).toBoolean()) return true;
  GlslStage vertexStage, fragmentStage;
  for (int stage = 0; stage < 2; ++stage) {
    JS::Value shader = JS::GetReservedSlot(
        program, stage == 0 ? kObjectDataSlot : kObjectExtraSlot);
    if (!shader.isObject()) return true;
    JS::RootedString source(state.cx, JS::GetReservedSlot(&shader.toObject(), kObjectDataSlot).toString());
    JS::UniqueChars text(JS_EncodeStringToUTF8(state.cx, source));
    if (!text || !GlslParse(text.get(), stage == 0 ? &vertexStage : &fragmentStage)) return true;
  }
  const int32_t width = context.drawingWidth, height = context.drawingHeight;
  if (width <= 0 || height <= 0) return true;
  ShadeEnv env;
  env.context = &context;
  env.attributes = GlslDeclarations(vertexStage, "attribute");
  env.uniforms = GlslDeclarations(vertexStage, "uniform");
  env.varyings = GlslDeclarations(vertexStage, "varying");
  // The fragment stage reads the interpolated varyings by name, so a shader pair
  // that declares them in a different order still interpolates the same values.
  std::vector<GlslDeclaration> colours = GlslDeclarations(fragmentStage, "varying");
  std::vector<size_t> colourSource(colours.size(), 0);
  for (size_t i = 0; i < colours.size(); ++i)
    for (size_t j = 0; j < env.varyings.size(); ++j)
      if (env.varyings[j].name == colours[i].name) colourSource[i] = j;
  const size_t varyings = env.varyings.size();
  const size_t vertices = size_t(count);
  std::vector<GlslValue> corners(vertices * varyings);
  std::vector<double> window(vertices * 2u), clipW(vertices);
  for (int32_t vertex = 0; vertex < count; ++vertex) {
    if (!ReadAttributes(state.cx, context, env.attributes, first, vertex, &env.attributeValues))
      return false;
    env.varyingValues.assign(varyings, GlslValue());
    GlslValue position;
    if (!RunStage(vertexStage, env, &position) || position.count != 4) return true;
    const double clip = position.value[3];
    clipW[size_t(vertex)] = clip;
    window[size_t(vertex) * 2] = (double(position.value[0]) / clip * 0.5 + 0.5) * width;
    window[size_t(vertex) * 2 + 1] = (double(position.value[1]) / clip * 0.5 + 0.5) * height;
    for (size_t i = 0; i < varyings; ++i)
      corners[size_t(vertex) * varyings + i] = env.varyingValues[i];
  }
  std::vector<GlslValue> shaded(varyings);
  for (int32_t vertex = 0; vertex + 2 < count; ++vertex) {
    const int32_t index[3] = {vertex, vertex + 1, vertex + 2};
    const double* a = &window[size_t(index[0]) * 2];
    const double* b = &window[size_t(index[1]) * 2];
    const double* c = &window[size_t(index[2]) * 2];
    if (clipW[size_t(index[0])] <= 0.0 || clipW[size_t(index[1])] <= 0.0 ||
        clipW[size_t(index[2])] <= 0.0) continue;
    const double area = Edge(a, b, c[0], c[1]);
    if (area == 0.0) continue;
    const double sign = area > 0.0 ? 1.0 : -1.0;
    const int32_t minX = std::max(0, int32_t(std::floor(std::min(a[0], std::min(b[0], c[0])))));
    const int32_t maxX = std::min(width - 1, int32_t(std::ceil(std::max(a[0], std::max(b[0], c[0])))));
    const int32_t minY = std::max(0, int32_t(std::floor(std::min(a[1], std::min(b[1], c[1])))));
    const int32_t maxY = std::min(height - 1, int32_t(std::ceil(std::max(a[1], std::max(b[1], c[1])))));
    for (int32_t y = minY; y <= maxY; ++y)
      for (int32_t x = minX; x <= maxX; ++x) {
        const int32_t covered = Coverage(a, b, c, x, y, sign);
        if (covered == 0) continue;
        const double cx = x + 0.5, cy = y + 0.5;
        const double weights[3] = {Edge(b, c, cx, cy) / area, Edge(c, a, cx, cy) / area,
                             Edge(a, b, cx, cy) / area};
        Interpolate(weights, corners, varyings, env.varyings, &shaded);
        ShadeEnv fragment;
        fragment.context = &context;
        fragment.uniforms = GlslDeclarations(fragmentStage, "uniform");
        fragment.varyings = colours;
        fragment.varyingValues.assign(colours.size(), GlslValue());
        for (size_t i = 0; i < colours.size(); ++i)
          fragment.varyingValues[i] = shaded[colourSource[i]];
        GlslValue colour;
        if (!RunStage(fragmentStage, fragment, &colour)) return true;
        // Every covered sample of the pixel carries the same fragment colour (the
        // D3D11 rasterizer shades once per pixel) and the 4x resolve averages the
        // four sample slots, so the sum is the colour times the covered count.
        uint8_t* pixel = context.drawingBuffer.data() +
                         (size_t(y) * size_t(width) + size_t(x)) * 4u;
        for (int32_t channel = 0; channel < 4; ++channel) {
          const int32_t sample = channel < colour.count ? Unorm8(colour.value[channel])
                                 : channel == 3 ? 255 : 0;
          pixel[channel] = uint8_t(Resolve8(sample * covered));
        }
      }
  }
  return true;
}
}  // namespace ruyijs::web_graphics
