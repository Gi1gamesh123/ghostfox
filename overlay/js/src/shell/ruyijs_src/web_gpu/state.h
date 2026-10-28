#ifndef RUYIJS_WEB_GPU_STATE_H
#define RUYIJS_WEB_GPU_STATE_H
#include "runtime/secure_slot.h"
#include <cstdint>
#include <string>
#include "interaction/realm.h"
#include "js/GCVector.h"
#include "js/RootingAPI.h"
#include "jsapi.h"
#include "runtime/web_kit.h"
#include "web_gpu/model.h"
#include "web_gpu/realm.h"

namespace ruyijs {
class PageHelpers;
namespace browser_environment { class Environment; }
namespace web_gpu {

// GPUDevice is an EventTarget with exactly one event-handler IDL attribute.
inline constexpr int32_t kDeviceHandlerCount = 1;
// GPUMapMode's specification values, used by GPUBuffer.mapAsync.
inline constexpr int32_t kMapModeRead = 1;
inline constexpr int32_t kMapModeWrite = 2;
// GPU.getPreferredCanvasFormat's single supported format.
inline constexpr const char* kPreferredCanvasFormat = "bgra8unorm";

enum AdapterSlot : int32_t { kAdapterFeatures = 0, kAdapterLimits = 1,
                             kAdapterFallback = 2, kAdapterInfo = 3,
                             kAdapterSlotCount = 4 };
enum AdapterInfoSlot : int32_t {
  kAdapterInfoVendor = 0, kAdapterInfoArchitecture, kAdapterInfoDevice,
  kAdapterInfoDescription, kAdapterInfoSubgroupMin, kAdapterInfoSubgroupMax,
  kAdapterInfoFallback, kAdapterInfoSlotCount,
};
enum LabelValueSlot : int32_t { kLabelValue = 0, kLabelValueSlotCount = 1 };
enum CompilationInfoSlot : int32_t { kCompilationMessages = 0, kCompilationInfoSlotCount = 1 };
enum CompilationMessageSlot : int32_t {
  kCompilationMessageText = 0, kCompilationMessageType,
  kCompilationMessageLine, kCompilationMessagePosition,
  kCompilationMessageOffset, kCompilationMessageLength,
  kCompilationMessageSlotCount,
};
enum PipelineErrorSlot : int32_t { kPipelineErrorReason = 0, kPipelineErrorSlotCount = 1 };
enum QuerySetSlot : int32_t {
  kQuerySetType = 0, kQuerySetCount, kQuerySetLabel, kQuerySetDestroyed,
  kQuerySetSlotCount,
};
enum DeviceSlot : int32_t {
  kDeviceFeatures = 0, kDeviceLimits = 1, kDeviceQueue = 2, kDeviceLost = 3,
  kDeviceDestroyed = 4, kDeviceHandlerBase = 5,
  kDeviceSlotCount = kDeviceHandlerBase + kDeviceHandlerCount,
};
enum BufferSlot : int32_t {
  kBufferSize = 0, kBufferUsage = 1, kBufferMapState = 2, kBufferDestroyed = 3,
  kBufferLabel = 4, kBufferMapped = 5, kBufferSlotCount = 6,
};
enum TextureSlot : int32_t {
  kTextureWidth = 0, kTextureHeight = 1, kTextureDepth = 2, kTextureMipLevelCount = 3,
  kTextureSampleCount = 4, kTextureDimension = 5, kTextureFormat = 6,
  kTextureUsage = 7, kTextureDestroyed = 8, kTextureLabel = 9,
  kTextureSlotCount = 10,
};
enum TextureViewSlot : int32_t { kTextureViewLabel = 0, kTextureViewSlotCount = 1 };
enum EncoderSlot : int32_t { kEncoderFinished = 0, kEncoderPassActive = 1,
                             kEncoderLabel = 2, kEncoderSlotCount = 3 };
enum PassSlot : int32_t { kPassEncoder = 0, kPassEnded = 1, kPassLabel = 2,
                          kPassSlotCount = 3 };
enum CommandBufferSlot : int32_t { kCommandBufferLabel = 0, kCommandBufferSlotCount = 1 };
enum FeaturesSlot : int32_t { kFeaturesSource = 0, kFeaturesSlotCount = 1 };
inline constexpr int32_t kWgslFeaturesSource = 2;
inline constexpr int32_t kWgslMemberBase = 100;

struct State {
  State(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
        interaction::Realm& events, browser_environment::Environment& environment);
  ~State();
  State* previous = nullptr;
  JSContext* cx;
  WebKit kit;
  Model model;
  interaction::Realm* events = nullptr;
  browser_environment::Environment* environment = nullptr;
  // The stub adapter is opt-in through the "webgpu" profile group; with the
  // default profile requestAdapter() honestly resolves null.
  bool enabled = false;
  // Adapter identity and fallback flag from the "webgpu" profile group.
  std::string info_vendor, info_architecture, info_device, info_description;
  bool fallback = false;
  JS::PersistentRootedObject navigatorGpu;
  // Every WebGPU interface/namespace and navigator.gpu is [SecureContext]
  // (dom/webidl/WebGPU.webidl); applied by Realm::ConfigureExposure.
  SecureSlots secure;
  WebFace gpu, adapter, device, queue, buffer, texture, texture_view, encoder,
      pass, command_buffer, features, wgsl_features;
  WebFace error, validation_error, memory_error, internal_error, limits, lost_info;
  WebFace adapter_info, bind_group, bind_group_layout, canvas_context,
      compilation_info, compilation_message, compute_pass_encoder, compute_pipeline,
      external_texture, pipeline_error, pipeline_layout, query_set, render_bundle,
      render_bundle_encoder, render_pipeline, sampler, shader_module;
};

extern const JSClass kGpuClass, kAdapterClass, kDeviceClass, kQueueClass,
    kBufferClass, kTextureClass, kTextureViewClass, kEncoderClass, kPassClass,
    kCommandBufferClass, kFeaturesClass, kWgslFeaturesClass,
    kAdapterInfoClass, kBindGroupClass, kBindGroupLayoutClass,
    kCanvasContextClass, kCompilationInfoClass, kCompilationMessageClass,
    kComputePassEncoderClass, kComputePipelineClass, kExternalTextureClass,
    kPipelineErrorClass, kPipelineLayoutClass, kQuerySetClass,
    kRenderBundleClass, kRenderBundleEncoderClass, kRenderPipelineClass,
    kSamplerClass, kShaderModuleClass;

enum GpuMethodId : int32_t { kGpuRequestAdapter = 0, kGpuPreferredCanvasFormat };
enum AdapterMethodId : int32_t { kAdapterRequestDevice = 0 };
enum DeviceMethodId : int32_t {
  kDeviceCreateBuffer = 0, kDeviceCreateTexture, kDeviceCreateCommandEncoder,
  kDeviceDestroy,
};
enum QueueMethodId : int32_t { kQueueSubmit = 0, kQueueOnSubmittedWorkDone };
enum BufferMethodId : int32_t {
  kBufferMapAsync = 0, kBufferGetMappedRange, kBufferUnmap, kBufferDestroy,
};
enum TextureMethodId : int32_t { kTextureCreateView = 0, kTextureDestroy };
enum EncoderMethodId : int32_t { kEncoderBeginRenderPass = 0, kEncoderFinish };
enum PassMethodId : int32_t { kPassEnd = 0 };

bool IllegalCreate(JSContext*, unsigned, JS::Value*);
bool GpuMethod(JSContext*, unsigned, JS::Value*);
bool AdapterProperty(JSContext*, unsigned, JS::Value*);
bool AdapterMethod(JSContext*, unsigned, JS::Value*);
bool DeviceProperty(JSContext*, unsigned, JS::Value*);
bool DeviceMethod(JSContext*, unsigned, JS::Value*);
bool QueueMethod(JSContext*, unsigned, JS::Value*);
bool BufferProperty(JSContext*, unsigned, JS::Value*);
bool BufferMethod(JSContext*, unsigned, JS::Value*);
bool TextureProperty(JSContext*, unsigned, JS::Value*);
bool TextureMethod(JSContext*, unsigned, JS::Value*);
bool TextureViewProperty(JSContext*, unsigned, JS::Value*);
bool EncoderProperty(JSContext*, unsigned, JS::Value*);
bool EncoderMethod(JSContext*, unsigned, JS::Value*);
bool PassProperty(JSContext*, unsigned, JS::Value*);
bool PassMethod(JSContext*, unsigned, JS::Value*);
bool CommandBufferProperty(JSContext*, unsigned, JS::Value*);
bool FeaturesProperty(JSContext*, unsigned, JS::Value*);
bool FeaturesMethod(JSContext*, unsigned, JS::Value*);
bool WgslFeaturesProperty(JSContext*, unsigned, JS::Value*);
bool InstallFeatureInterfaces(State&);
bool HandlerGet(JSContext*, unsigned, JS::Value*);
bool HandlerSet(JSContext*, unsigned, JS::Value*);
bool NavigatorGpu(JSContext*, unsigned, JS::Value*);
// The "webgpu" environment-profile group ports, bound into the browser
// environment Navigator owner: configure carries the stub-adapter opt-in, and
// describe reports the effective flag from this owner.
bool GpuPort(void* context, JS::HandleObject group);
bool GpuDescribe(void* context, JS::MutableHandleValue out);

// One error factory for the domain: the page realm's own DOMException (or
// TypeError), never a second error type. Throw* sets the pending exception;
// Reject* settles a promise, for the promise-returning operations.
bool MakeException(State& state, const char* interface, const char* name,
                   const char* message, JS::MutableHandleValue result);
bool ThrowError(State& state, const char* interface, const char* name,
                const char* message);
bool RejectError(State& state, const char* interface, const char* name,
                 const char* message, JS::MutableHandleValue result);
bool ThrowOperation(State& state, const char* message);
bool RejectOperation(State& state, const char* message, JS::MutableHandleValue result);

// Realm object factories.
JSObject* NewAdapter(State& state);
JSObject* NewDevice(State& state, JS::HandleObject proto);
JSObject* NewBuffer(State& state, JS::HandleObject device, double size, uint32_t usage,
                    JS::HandleString label);
JSObject* NewTexture(State& state, JS::HandleObject device, JS::HandleObject size,
                     JS::HandleString format, uint32_t usage, uint32_t mip_level_count,
                     uint32_t sample_count, JS::HandleString dimension, JS::HandleString label);
JSObject* NewTextureView(State& state, JS::HandleString label);
JSObject* NewCommandEncoder(State& state, JS::HandleString label);
JSObject* NewRenderPass(State& state, JS::HandleObject encoder, JS::HandleString label);
JSObject* NewCommandBuffer(State& state, JS::HandleString label);
JSObject* NewFeatures(State& state, int32_t source);
JSObject* NewAdapterInfo(State& state);

// The limits plain object, built from the imported table for adapter and device.
bool BuildLimits(State& state, JS::MutableHandleObject out);
// One limit's value from the imported table; 0 for an unknown name.
double Limit(State& state, const char* name);
// Resolves a mapped-range ArrayBuffer for one buffer, creating it once.
bool MappedRange(State& state, JS::HandleObject buffer, JS::MutableHandleObject out);

}  // namespace web_gpu
}  // namespace ruyijs
#endif
