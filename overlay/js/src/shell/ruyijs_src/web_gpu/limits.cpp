#include "web_gpu/internal.h"
#include "web_gpu/value_objects.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include <string_view>

namespace ruyijs::web_gpu {
const JSClass kLimitsClass = {"GPUSupportedLimits", 0};
namespace {
struct LimitMember { const char* name; const char* source; };
#define LIMIT(name) {#name, #name}
// Firefox SupportedLimits.cpp projects InVertex/InFragment to PerShader
// (bug 2006720). Values remain in the original capability aggregate.
const LimitMember kMembers[] = {
  LIMIT(maxTextureDimension1D), LIMIT(maxTextureDimension2D), LIMIT(maxTextureDimension3D),
  LIMIT(maxTextureArrayLayers), LIMIT(maxBindGroups), LIMIT(maxBindGroupsPlusVertexBuffers),
  LIMIT(maxBindingsPerBindGroup), LIMIT(maxDynamicUniformBuffersPerPipelineLayout),
  LIMIT(maxDynamicStorageBuffersPerPipelineLayout), LIMIT(maxSampledTexturesPerShaderStage),
  LIMIT(maxSamplersPerShaderStage),
  {"maxStorageBuffersInVertexStage", "maxStorageBuffersPerShaderStage"},
  {"maxStorageBuffersInFragmentStage", "maxStorageBuffersPerShaderStage"},
  LIMIT(maxStorageBuffersPerShaderStage),
  {"maxStorageTexturesInVertexStage", "maxStorageTexturesPerShaderStage"},
  {"maxStorageTexturesInFragmentStage", "maxStorageTexturesPerShaderStage"},
  LIMIT(maxStorageTexturesPerShaderStage), LIMIT(maxUniformBuffersPerShaderStage),
  LIMIT(maxUniformBufferBindingSize), LIMIT(maxStorageBufferBindingSize),
  LIMIT(minUniformBufferOffsetAlignment), LIMIT(minStorageBufferOffsetAlignment),
  LIMIT(maxVertexBuffers), LIMIT(maxBufferSize), LIMIT(maxVertexAttributes),
  LIMIT(maxVertexBufferArrayStride), LIMIT(maxInterStageShaderVariables),
  LIMIT(maxColorAttachments), LIMIT(maxColorAttachmentBytesPerSample),
  LIMIT(maxComputeWorkgroupStorageSize), LIMIT(maxComputeInvocationsPerWorkgroup),
  LIMIT(maxComputeWorkgroupSizeX), LIMIT(maxComputeWorkgroupSizeY),
  LIMIT(maxComputeWorkgroupSizeZ), LIMIT(maxComputeWorkgroupsPerDimension)
};
#undef LIMIT
bool LimitProperty(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject object(cx);
  State* state = ValueReceiver(cx, args.thisv(), &kLimitsClass, &object);
  if (!state) return false;
  args.rval().setDouble(Limit(*state, kMembers[WebKit::Slot(args)].source));
  return true;
}
}

bool InstallLimits(State& state) {
  JS::RootedObject plain(state.cx, JS::GetRealmObjectPrototype(state.cx));
  if (!state.kit.Define("GPUSupportedLimits", &kLimitsClass, plain, IllegalCreate, &state.limits))
    return false;
  for (unsigned i = 0; i < sizeof(kMembers) / sizeof(kMembers[0]); ++i)
    if (!state.kit.Accessor(state.limits, "GPUSupportedLimits", kMembers[i].name,
                            LimitProperty, nullptr, int32_t(i))) return false;
  return JS_DeleteProperty(state.cx, state.limits.prototype, "constructor") &&
         JS_DefineProperty(state.cx, state.limits.prototype, "constructor", state.limits.constructor, 0);
}
bool BuildLimits(State& state, JS::MutableHandleObject out) {
  {
    JSAutoRealm realm(state.cx, state.kit.page);
    out.set(state.kit.New(&kLimitsClass, state.limits.prototype));
    if (!out) return false;
  }
  return JS_WrapObject(state.cx, out);
}

double Limit(State& state, const char* name) {
  for (const auto& member : kMembers) {
    if (std::string_view(name) == member.name) { name = member.source; break; }
  }
  for (const auto& limit : state.model.limits())
    if (limit.name == name) return limit.value;
  return 0.0;
}

}  // namespace ruyijs::web_gpu
