#include "web_graphics/state.h"

namespace ruyijs::web_graphics {
bool GetParameterState(const Context& context, int32_t name,
                      ParameterStateValue* out) noexcept {
  for (int32_t i = 0; i < context.parameterStateCount; ++i) {
    if (context.parameterState[i].name != name) continue;
    *out = context.parameterState[i];
    return true;
  }
  return false;
}

void SetParameterState(Context& context, int32_t name, ParameterKind kind,
                       int count, const double* values) noexcept {
  int32_t index = 0;
  while (index < context.parameterStateCount && context.parameterState[index].name != name)
    ++index;
  if (index == context.parameterStateCount) {
    if (context.parameterStateCount == static_cast<int32_t>(context.parameterState.size())) return;
    ++context.parameterStateCount;
  }
  auto& state = context.parameterState[index];
  state.name = name;
  state.kind = kind;
  state.count = count > 4 ? 4 : count;
  for (int32_t i = 0; i < state.count; ++i) state.values[i] = values[i];
  for (int32_t i = state.count; i < 4; ++i) state.values[i] = 0;
}
}  // namespace ruyijs::web_graphics
