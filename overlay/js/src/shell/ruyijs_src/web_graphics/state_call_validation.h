#ifndef RUYIJS_WEB_GRAPHICS_STATE_CALL_VALIDATION_H
#define RUYIJS_WEB_GRAPHICS_STATE_CALL_VALIDATION_H
#include <cstdint>
namespace ruyijs::web_graphics {
struct Context;
struct State;
namespace state_call_detail {
unsigned RequiredArguments(int op) noexcept;
bool ValidateCapability(State&, Context&, int32_t value, bool* valid);
bool ValidateBlendEquation(State&, Context&, int32_t mode, bool* valid);
bool ValidBlendFactor(int32_t value) noexcept;
bool ValidStencilOp(int32_t value) noexcept;
bool ValidFace(int32_t value) noexcept;
bool ValidCompare(int32_t value) noexcept;
}
}
#endif
