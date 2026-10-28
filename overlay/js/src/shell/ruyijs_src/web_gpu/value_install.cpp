#include "web_gpu/value_objects.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "jsfriendapi.h"

#include <iterator>

namespace ruyijs::web_gpu {
namespace {
const JSClass kErrorClass = {"GPUError", 0};
// WebKit records constructor before members; move that one configurable property
// after the IDL attributes to preserve Firefox's observable own-key order.
bool FinishAttributes(State& state, WebFace& face) {
  return JS_DeleteProperty(state.cx, face.prototype, "constructor") &&
         JS_DefineProperty(state.cx, face.prototype, "constructor", face.constructor, 0);
}
bool ErrorInterface(State& state, const char* name, const JSClass* clasp, WebFace& face, int kind) {
  if (!state.kit.Define(name, clasp, state.error.prototype, ConstructGpuError, &face) ||
      !JS_SetPrototype(state.cx, face.constructor, state.error.constructor) ||
      !JS_DefineProperty(state.cx, face.constructor, "length", 1, JSPROP_READONLY)) return false;
  js::SetFunctionNativeReserved(face.constructor, 1, JS::Int32Value(kind));
  return true;
}

struct SurfaceMethod { const char* name; unsigned length; };

bool Methods(State& state, WebFace& face, const char* name,
             const SurfaceMethod* methods, size_t count) {
  for (size_t i = 0; i < count; ++i)
    if (!state.kit.Method(face, name, methods[i].name, ValueMethod,
                          methods[i].length, static_cast<int32_t>(i))) return false;
  return true;
}

bool LabelInterface(State& state, const char* name, const JSClass* clasp, WebFace& face) {
  JS::RootedObject plain(state.cx, JS::GetRealmObjectPrototype(state.cx));
  return state.kit.Define(name, clasp, plain,
                          IllegalCreate, &face) &&
         state.kit.Accessor(face, name, "label", LabelValueProperty,
                            LabelValueProperty, kLabelValue);
}

bool AdapterInfoInterface(State& state) {
  JS::RootedObject plain(state.cx, JS::GetRealmObjectPrototype(state.cx));
  const char* names[] = {"vendor", "architecture", "device", "description",
                         "subgroupMinSize", "subgroupMaxSize", "isFallbackAdapter"};
  if (!state.kit.Define("GPUAdapterInfo", &kAdapterInfoClass,
                        plain, IllegalCreate,
                        &state.adapter_info)) return false;
  for (int32_t i = 0; i < 7; ++i)
    if (!state.kit.Accessor(state.adapter_info, "GPUAdapterInfo", names[i],
                            AdapterInfoProperty, nullptr, i)) return false;
  return true;
}

bool CompilationInterfaces(State& state) {
  JS::RootedObject plain(state.cx, JS::GetRealmObjectPrototype(state.cx));
  if (!state.kit.Define("GPUCompilationInfo", &kCompilationInfoClass,
                        plain, IllegalCreate,
                        &state.compilation_info) ||
      !state.kit.Accessor(state.compilation_info, "GPUCompilationInfo", "messages",
                          CompilationInfoProperty, nullptr, kCompilationMessages) ||
      !state.kit.Define("GPUCompilationMessage", &kCompilationMessageClass,
                        plain, IllegalCreate,
                        &state.compilation_message)) return false;
  const char* names[] = {"message", "type", "lineNum", "linePos", "offset", "length"};
  for (int32_t i = 0; i < 6; ++i)
    if (!state.kit.Accessor(state.compilation_message, "GPUCompilationMessage", names[i],
                            CompilationMessageProperty, nullptr, i)) return false;
  return true;
}

bool PipelineErrorInterface(State& state) {
  JSContext* cx = state.cx;
  JS::RootedValue domException(cx);
  JS::RootedObject domConstructor(cx);
  JS::RootedObject parent(cx);
  if (!JS_GetProperty(cx, state.kit.page, "DOMException", &domException) ||
      !domException.isObject()) return WebKit::Fail(cx, "DOMException is required by GPUPipelineError");
  domConstructor = &domException.toObject();
  if (!JS_GetProperty(cx, domConstructor, "prototype",
                                                  &domException) ||
      !domException.isObject()) return WebKit::Fail(cx, "DOMException is required by GPUPipelineError");
  parent = &domException.toObject();
  if (!state.kit.Define("GPUPipelineError", &kPipelineErrorClass, parent,
                        PipelineErrorConstructor, &state.pipeline_error)) return false;
  if (!JS_DefineProperty(cx, state.pipeline_error.constructor, "length", 2,
                         JSPROP_READONLY) ||
      !state.kit.Accessor(state.pipeline_error, "GPUPipelineError", "reason",
                          PipelineErrorProperty, nullptr, kPipelineErrorReason)) return false;
  return true;
}
}
bool InstallValueInterfaces(State& state) {
  JS::RootedObject plain(state.cx, JS::GetRealmObjectPrototype(state.cx));
  if (!state.kit.Define("GPUError", &kErrorClass, plain, IllegalCreate, &state.error) ||
      !state.kit.Accessor(state.error, "GPUError", "message", GpuErrorMessage, nullptr, 0) ||
      !FinishAttributes(state, state.error) ||
      !ErrorInterface(state, "GPUValidationError", &kValidationErrorClass, state.validation_error, 0) ||
      !ErrorInterface(state, "GPUOutOfMemoryError", &kMemoryErrorClass, state.memory_error, 1) ||
      !ErrorInterface(state, "GPUInternalError", &kInternalErrorClass, state.internal_error, 2) ||
      !state.kit.Define("GPUDeviceLostInfo", &kLostInfoClass, plain, IllegalCreate, &state.lost_info) ||
      !state.kit.Accessor(state.lost_info, "GPUDeviceLostInfo", "reason", LostInfoProperty, nullptr, 0) ||
      !state.kit.Accessor(state.lost_info, "GPUDeviceLostInfo", "message", LostInfoProperty, nullptr, 1) ||
      !FinishAttributes(state, state.lost_info)) return false;
  if (!AdapterInfoInterface(state) || !CompilationInterfaces(state) ||
      !LabelInterface(state, "GPUBindGroup", &kBindGroupClass, state.bind_group) ||
      !LabelInterface(state, "GPUBindGroupLayout", &kBindGroupLayoutClass, state.bind_group_layout) ||
      !LabelInterface(state, "GPUExternalTexture", &kExternalTextureClass, state.external_texture) ||
      !LabelInterface(state, "GPUPipelineLayout", &kPipelineLayoutClass, state.pipeline_layout) ||
      !LabelInterface(state, "GPURenderBundle", &kRenderBundleClass, state.render_bundle) ||
      !LabelInterface(state, "GPUSampler", &kSamplerClass, state.sampler)) return false;
  constexpr SurfaceMethod kPipelineMethods[] = {{"getBindGroupLayout", 1}};
  if (!state.kit.Define("GPUComputePipeline", &kComputePipelineClass, plain,
                        IllegalCreate, &state.compute_pipeline) ||
      !Methods(state, state.compute_pipeline, "GPUComputePipeline", kPipelineMethods,
               std::size(kPipelineMethods)) ||
      !state.kit.Accessor(state.compute_pipeline, "GPUComputePipeline", "label",
                          LabelValueProperty, LabelValueProperty, kLabelValue) ||
      !state.kit.Define("GPURenderPipeline", &kRenderPipelineClass, plain,
                        IllegalCreate, &state.render_pipeline) ||
      !Methods(state, state.render_pipeline, "GPURenderPipeline", kPipelineMethods,
               std::size(kPipelineMethods)) ||
      !state.kit.Accessor(state.render_pipeline, "GPURenderPipeline", "label",
                          LabelValueProperty, LabelValueProperty, kLabelValue) ||
      !state.kit.Define("GPUShaderModule", &kShaderModuleClass, plain,
                        IllegalCreate, &state.shader_module)) return false;
  constexpr SurfaceMethod kShaderMethods[] = {{"getCompilationInfo", 0}};
  if (!Methods(state, state.shader_module, "GPUShaderModule", kShaderMethods,
               std::size(kShaderMethods)) ||
      !state.kit.Accessor(state.shader_module, "GPUShaderModule", "label",
                          LabelValueProperty, LabelValueProperty, kLabelValue)) return false;
  if (!state.kit.Define("GPUCanvasContext", &kCanvasContextClass,
                        plain, IllegalCreate, &state.canvas_context)) return false;
  constexpr SurfaceMethod kCanvasMethods[] = {
      {"configure", 1}, {"unconfigure", 0}, {"getConfiguration", 0},
      {"getCurrentTexture", 0}};
  if (!Methods(state, state.canvas_context, "GPUCanvasContext", kCanvasMethods,
               std::size(kCanvasMethods)) ||
      !state.kit.Accessor(state.canvas_context, "GPUCanvasContext", "canvas",
                          CanvasProperty, nullptr, 0)) return false;
  if (!state.kit.Define("GPUComputePassEncoder", &kComputePassEncoderClass,
                        plain, IllegalCreate, &state.compute_pass_encoder)) return false;
  constexpr SurfaceMethod kComputePassMethods[] = {
      {"setPipeline", 1}, {"dispatchWorkgroups", 1},
      {"dispatchWorkgroupsIndirect", 2}, {"end", 0}, {"setBindGroup", 2},
      {"pushDebugGroup", 1}, {"popDebugGroup", 0}, {"insertDebugMarker", 1}};
  if (!Methods(state, state.compute_pass_encoder, "GPUComputePassEncoder", kComputePassMethods,
               std::size(kComputePassMethods)) ||
      !state.kit.Accessor(state.compute_pass_encoder, "GPUComputePassEncoder", "label",
                          LabelValueProperty, LabelValueProperty, kLabelValue)) return false;
  if (!state.kit.Define("GPUQuerySet", &kQuerySetClass, plain, IllegalCreate,
                        &state.query_set) ||
      !state.kit.Method(state.query_set, "GPUQuerySet", "destroy", ValueMethod, 0, 0) ||
      !state.kit.Accessor(state.query_set, "GPUQuerySet", "type", ValueMethod, nullptr,
                          kQuerySetType) ||
      !state.kit.Accessor(state.query_set, "GPUQuerySet", "count", ValueMethod, nullptr,
                          kQuerySetCount) ||
      !state.kit.Accessor(state.query_set, "GPUQuerySet", "label", LabelValueProperty,
                          LabelValueProperty, kQuerySetLabel)) return false;
  constexpr SurfaceMethod kRenderBundleMethods[] = {
      {"finish", 0}, {"setBindGroup", 2}, {"pushDebugGroup", 1},
      {"popDebugGroup", 0}, {"insertDebugMarker", 1}, {"setPipeline", 1},
      {"setIndexBuffer", 2}, {"setVertexBuffer", 2}, {"draw", 1},
      {"drawIndexed", 1}, {"drawIndirect", 2}, {"drawIndexedIndirect", 2}};
  if (!state.kit.Define("GPURenderBundleEncoder", &kRenderBundleEncoderClass, plain,
                        IllegalCreate, &state.render_bundle_encoder) ||
      !Methods(state, state.render_bundle_encoder, "GPURenderBundleEncoder",
               kRenderBundleMethods, std::size(kRenderBundleMethods)) ||
      !state.kit.Accessor(state.render_bundle_encoder, "GPURenderBundleEncoder", "label",
                          LabelValueProperty, LabelValueProperty, kLabelValue) ||
      !PipelineErrorInterface(state)) return false;
  return InstallLimits(state);
}
}
