#include "web_graphics/model.h"
namespace ruyijs::web_graphics {
// Initial GL state and limits. Values match the Firefox 158 ANGLE D3D11 domtrace
// jscall-detail-desktop-20261019-230430 (and the earlier 182516 trace for the
// stencil masks); gates follow WebGLContextState.cpp / ClientWebGLContext.cpp.
const ParameterDefault* PublicParameterDefault(int32_t name) noexcept {
  using K = ParameterKind;
  static const ParameterDefault values[] = {
    {0x846E,K::FloatArray,2,{1,1}}, {0x846D,K::FloatArray,2,{1,1024}},
    {0x0D55,K::Number,1,{8}}, {0x0D54,K::Number,1,{8}},
    {0x0D53,K::Number,1,{8}}, {0x0D52,K::Number,1,{8}},
    {0x0BE2,K::Boolean,1,{0}}, {0x0B44,K::Boolean,1,{0}},
    {0x0B71,K::Boolean,1,{0}}, {0x0BD0,K::Boolean,1,{1}},
    {0x8037,K::Boolean,1,{0}}, {0x80A0,K::Boolean,1,{0}},
    {0x0C11,K::Boolean,1,{0}}, {0x0B90,K::Boolean,1,{0}},
    {0x0C22,K::FloatArray,4,{0,0,0,0}}, {0x0C23,K::BoolArray,4,{1,1,1,1}},
    {0x0B70,K::FloatArray,2,{0,1}}, {0x0D56,K::Number,1,{24}},
    {0x8192,K::Number,1,{4352}}, {0x8B4D,K::Number,1,{32}},
    {0x851C,K::Number,1,{16384}}, {0x8DFD,K::Number,1,{1024}},
    {0x8DFC,K::Number,1,{30}}, {0x8B4C,K::Number,1,{16}},
    {0x8DFB,K::Number,1,{4095}}, {0x0D05,K::Number,1,{4}},
    {0x0CF5,K::Number,1,{4}}, {0x80A9,K::Number,1,{4}},
    {0x80A8,K::Number,1,{1}}, {0x0D57,K::Number,1,{0}},
    {0x0D50,K::Number,1,{4}},
    {0x0BA2,K::IntArray,4,{0,0,300,150}}, {0x0C10,K::IntArray,4,{0,0,300,150}},
    {0x0B93,K::Number,1,{4294967295.0}}, {0x0B98,K::Number,1,{4294967295.0}},
    {0x8CA4,K::Number,1,{4294967295.0}}, {0x8CA5,K::Number,1,{4294967295.0}},
    // Blend, depth, stencil, polygon offset, sample coverage and unpack state.
    {0x8009,K::Number,1,{32774}}, {0x883D,K::Number,1,{32774}},
    {0x80C8,K::Number,1,{0}}, {0x80C9,K::Number,1,{1}},
    {0x80CA,K::Number,1,{0}}, {0x80CB,K::Number,1,{1}},
    {0x8005,K::FloatArray,4,{0,0,0,0}}, {0x0B21,K::Number,1,{1}},
    {0x0B45,K::Number,1,{1029}}, {0x0B46,K::Number,1,{2305}},
    {0x0B72,K::Boolean,1,{1}}, {0x0B73,K::Number,1,{1}}, {0x0B74,K::Number,1,{513}},
    {0x0B91,K::Number,1,{0}}, {0x0B92,K::Number,1,{519}},
    {0x0B94,K::Number,1,{7680}}, {0x0B95,K::Number,1,{7680}},
    {0x0B96,K::Number,1,{7680}}, {0x0B97,K::Number,1,{0}},
    {0x8800,K::Number,1,{519}}, {0x8801,K::Number,1,{7680}},
    {0x8802,K::Number,1,{7680}}, {0x8803,K::Number,1,{7680}},
    {0x8CA3,K::Number,1,{0}}, {0x2A00,K::Number,1,{0}}, {0x8038,K::Number,1,{0}},
    {0x80AA,K::Number,1,{1}}, {0x80AB,K::Boolean,1,{0}},
    {0x84E0,K::Number,1,{33984}}, {0x9240,K::Boolean,1,{0}},
    {0x9241,K::Boolean,1,{0}}, {0x9243,K::Number,1,{37444}},
    {0x8B9A,K::Number,1,{5121}}, {0x8B9B,K::Number,1,{6408}},
    // Unbound object bindings answer null without an error.
    {0x8B8D,K::Null,0,{}}, {0x8CA6,K::Null,0,{}}, {0x8CA7,K::Null,0,{}},
    {0x8069,K::Null,0,{}}, {0x8514,K::Null,0,{}},
    {0x85B5,K::Null,0,{},"OES_vertex_array_object",true},
    // Extension-gated state (core in WebGL2 where `true`).
    {0x8B8B,K::Number,1,{4352},"OES_standard_derivatives",true},
    {0x8CDF,K::Number,1,{8},"WEBGL_draw_buffers",true},
    {0x8824,K::Number,1,{8},"WEBGL_draw_buffers",true},
    {0x8825,K::Number,1,{1029},"WEBGL_draw_buffers",true},
    {0x8826,K::Number,1,{0},"WEBGL_draw_buffers",true},
    {0x8827,K::Number,1,{0},"WEBGL_draw_buffers",true},
    {0x8828,K::Number,1,{0},"WEBGL_draw_buffers",true},
    {0x8829,K::Number,1,{0},"WEBGL_draw_buffers",true},
    {0x882A,K::Number,1,{0},"WEBGL_draw_buffers",true},
    {0x882B,K::Number,1,{0},"WEBGL_draw_buffers",true},
    {0x882C,K::Number,1,{0},"WEBGL_draw_buffers",true},
    {0x8E4F,K::Number,1,{36430},"WEBGL_provoking_vertex"},
    {0x864F,K::Boolean,1,{0},"EXT_depth_clamp"},
    {0x8E1B,K::Number,1,{0},"EXT_polygon_offset_clamp"},
  };
  for (const auto& value : values) if (value.name == name) return &value;
  return nullptr;
}
// WebGL2-only parameters, values from the Firefox 182516 trace.
const ParameterDefault* Webgl2ParameterDefault(int32_t name) noexcept {
  using K = ParameterKind;
  static const ParameterDefault values[] = {
    {0x8073,K::Number,1,{2048}}, {0x80E8,K::Number,1,{2147483647}}, {0x80E9,K::Number,1,{2147483647}},
    {0x84FD,K::Number,1,{2}}, {0x88FF,K::Number,1,{2048}},
    {0x8904,K::Number,1,{-8}}, {0x8905,K::Number,1,{7}}, {0x8A2B,K::Number,1,{12}},
    {0x8A2D,K::Number,1,{12}}, {0x8A2E,K::Number,1,{24}}, {0x8A2F,K::Number,1,{24}},
    {0x8A30,K::Number,1,{65536}}, {0x8A31,K::Number,1,{212988}}, {0x8A33,K::Number,1,{200704}},
    {0x8A34,K::Number,1,{256}}, {0x8B49,K::Number,1,{4096}}, {0x8B4A,K::Number,1,{16380}},
    {0x8B4B,K::Number,1,{120}},
    {0x8C80,K::Number,1,{4}}, {0x8C8A,K::Number,1,{120}}, {0x8C8B,K::Number,1,{4}},
    {0x8D57,K::Number,1,{8}}, {0x8D6B,K::Number,1,{4294967294.0}},
    {0x9111,K::Number,1,{0}}, {0x9122,K::Number,1,{120}}, {0x9125,K::Number,1,{120}},
    {0x9247,K::Number,1,{1000000000}}
  };
  for (const auto& value : values) if (value.name == name) return &value;
  return nullptr;
}
}
