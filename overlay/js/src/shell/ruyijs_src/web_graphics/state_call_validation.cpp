#include "web_graphics/internal.h"
#include "web_graphics/public_calls.h"
#include "web_graphics/state_call_validation.h"

#include <initializer_list>

namespace ruyijs::web_graphics::state_call_detail {
namespace {
bool Has(int32_t value, std::initializer_list<int32_t> values) {
  for (int32_t candidate : values) if (candidate == value) return true;
  return false;
}
}

unsigned RequiredArguments(int op) noexcept {
  switch (op) {
    case kEnable: case kDisable: case kIsEnabled: case kBlendEquation:
    case kCullFace: case kFrontFace: case kLineWidth: case kDepthFunc:
    case kDepthMask: case kClearDepth: case kClearStencil: case kStencilMask:
      return 1;
    case kBlendEquationSeparate: case kBlendFunc: case kDepthRange:
    case kPolygonOffset: case kStencilMaskSeparate: case kPixelStorei:
    case kHint: case kSampleCoverage:
      return 2;
    case kStencilFunc: case kStencilOp:
      return 3;
    case kBlendColor: case kClearColor: case kColorMask: case kScissor:
    case kViewport: case kBlendFuncSeparate: case kStencilFuncSeparate:
    case kStencilOpSeparate:
      return 4;
    default:
      return 0;
  }
}

bool ValidateCapability(State& state, Context& context, int32_t value,
                        bool* valid) {
  *valid = value == 0x0B44 || value == 0x0BE2 || value == 0x0B71 ||
           value == 0x0BD0 || value == 0x0C11 || value == 0x0B90 ||
           value == 0x8037 || value == 0x809E || value == 0x80A0 ||
           (value == 0x8C89 && context.version == 2);
  if (*valid || value != 0x864F) return true;
  return ExtensionEnabled(state, context, "EXT_depth_clamp", valid);
}

bool ValidateBlendEquation(State& state, Context& context, int32_t mode,
                           bool* valid) {
  if (mode == 0x8006 || mode == 0x800A || mode == 0x800B) {
    *valid = true;
    return true;
  }
  *valid = false;
  if (mode != 0x8007 && mode != 0x8008) return true;
  if (context.version == 2) {
    *valid = true;
    return true;
  }
  return ExtensionEnabled(state, context, "EXT_blend_minmax", valid);
}

bool ValidBlendFactor(int32_t value) noexcept {
  return Has(value, {0,1,0x0300,0x0301,0x0302,0x0303,0x0304,0x0305,
                     0x0306,0x0307,0x0308,0x8001,0x8002,0x8003,0x8004});
}
bool ValidStencilOp(int32_t value) noexcept {
  return Has(value, {0,0x1E00,0x1E01,0x1E02,0x1E03,0x150A,0x8507,0x8508});
}
bool ValidFace(int32_t value) noexcept {
  return Has(value, {0x0404,0x0405,0x0408});
}
bool ValidCompare(int32_t value) noexcept {
  return value >= 0x0200 && value <= 0x0207;
}
}
