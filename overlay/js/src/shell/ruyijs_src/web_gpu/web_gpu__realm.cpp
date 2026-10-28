#include "web_gpu/state.h"
#include "web_gpu/value_objects.h"

#include "browser_environment/environment.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "js/ValueArray.h"
#include "runtime/runtime.h"

namespace ruyijs::web_gpu {
namespace {

const char* const kDeviceHandler = "uncapturederror";

bool Delegate(JSContext* cx, JS::HandleObject self, const char* method,
              const char* type, JS::HandleValue callback) {
  JS::RootedValue function(cx);
  if (!JS_GetProperty(cx, self, method, &function)) return false;
  if (!function.isObject() || !JS::IsCallable(&function.toObject())) return true;
  JS::RootedString name(cx, JS_NewStringCopyZ(cx, type));
  if (!name) return false;
  JS::RootedValueArray<2> arguments(cx);
  arguments[0].setString(name);
  arguments[1].set(callback);
  JS::RootedValue ignored(cx);
  return JS::Call(cx, self, function, arguments, &ignored);
}

bool Check(JSContext* cx, JS::HandleValue self, const JSClass* clasp,
           JS::MutableHandleObject out, const char* interface) {
  std::string message = std::string(interface) +
                        " member called on an incompatible receiver";
  return WebKit::Check(cx, self, clasp, out, message.c_str());
}

}  // namespace

// Every GPUBuffer/GPUTexture/... is created only by its owning device or its
// owner interface, so none of the interfaces below is globally constructible.
const JSClass kGpuClass = {"GPU", JSCLASS_HAS_RESERVED_SLOTS(1)};
const JSClass kAdapterClass = {"GPUAdapter", JSCLASS_HAS_RESERVED_SLOTS(kAdapterSlotCount)};
const JSClass kDeviceClass = {"GPUDevice", JSCLASS_HAS_RESERVED_SLOTS(kDeviceSlotCount)};
const JSClass kQueueClass = {"GPUQueue", JSCLASS_HAS_RESERVED_SLOTS(1)};
const JSClass kBufferClass = {"GPUBuffer", JSCLASS_HAS_RESERVED_SLOTS(kBufferSlotCount)};
const JSClass kTextureClass = {"GPUTexture", JSCLASS_HAS_RESERVED_SLOTS(kTextureSlotCount)};
const JSClass kTextureViewClass = {"GPUTextureView",
                                   JSCLASS_HAS_RESERVED_SLOTS(kTextureViewSlotCount)};
const JSClass kEncoderClass = {"GPUCommandEncoder", JSCLASS_HAS_RESERVED_SLOTS(kEncoderSlotCount)};
const JSClass kPassClass = {"GPURenderPassEncoder", JSCLASS_HAS_RESERVED_SLOTS(kPassSlotCount)};
const JSClass kCommandBufferClass = {"GPUCommandBuffer",
                                     JSCLASS_HAS_RESERVED_SLOTS(kCommandBufferSlotCount)};
const JSClass kFeaturesClass = {"GPUSupportedFeatures",
                                JSCLASS_HAS_RESERVED_SLOTS(kFeaturesSlotCount)};
const JSClass kWgslFeaturesClass = {"WGSLLanguageFeatures",
                                    JSCLASS_HAS_RESERVED_SLOTS(kFeaturesSlotCount)};
const JSClass kAdapterInfoClass = {"GPUAdapterInfo", JSCLASS_HAS_RESERVED_SLOTS(kAdapterInfoSlotCount)};
const JSClass kBindGroupClass = {"GPUBindGroup", JSCLASS_HAS_RESERVED_SLOTS(kLabelValueSlotCount)};
const JSClass kBindGroupLayoutClass = {"GPUBindGroupLayout", JSCLASS_HAS_RESERVED_SLOTS(kLabelValueSlotCount)};
const JSClass kCanvasContextClass = {"GPUCanvasContext", JSCLASS_HAS_RESERVED_SLOTS(kLabelValueSlotCount)};
const JSClass kCompilationInfoClass = {"GPUCompilationInfo", JSCLASS_HAS_RESERVED_SLOTS(kCompilationInfoSlotCount)};
const JSClass kCompilationMessageClass = {"GPUCompilationMessage", JSCLASS_HAS_RESERVED_SLOTS(kCompilationMessageSlotCount)};
const JSClass kComputePassEncoderClass = {"GPUComputePassEncoder", JSCLASS_HAS_RESERVED_SLOTS(kLabelValueSlotCount)};
const JSClass kComputePipelineClass = {"GPUComputePipeline", JSCLASS_HAS_RESERVED_SLOTS(kLabelValueSlotCount)};
const JSClass kExternalTextureClass = {"GPUExternalTexture", JSCLASS_HAS_RESERVED_SLOTS(kLabelValueSlotCount)};
const JSClass kPipelineErrorClass = {"GPUPipelineError", JSCLASS_HAS_RESERVED_SLOTS(kPipelineErrorSlotCount)};
const JSClass kPipelineLayoutClass = {"GPUPipelineLayout", JSCLASS_HAS_RESERVED_SLOTS(kLabelValueSlotCount)};
const JSClass kQuerySetClass = {"GPUQuerySet", JSCLASS_HAS_RESERVED_SLOTS(kQuerySetSlotCount)};
const JSClass kRenderBundleClass = {"GPURenderBundle", JSCLASS_HAS_RESERVED_SLOTS(kLabelValueSlotCount)};
const JSClass kRenderBundleEncoderClass = {"GPURenderBundleEncoder", JSCLASS_HAS_RESERVED_SLOTS(kLabelValueSlotCount)};
const JSClass kRenderPipelineClass = {"GPURenderPipeline", JSCLASS_HAS_RESERVED_SLOTS(kLabelValueSlotCount)};
const JSClass kSamplerClass = {"GPUSampler", JSCLASS_HAS_RESERVED_SLOTS(kLabelValueSlotCount)};
const JSClass kShaderModuleClass = {"GPUShaderModule", JSCLASS_HAS_RESERVED_SLOTS(kLabelValueSlotCount)};

State::State(JSContext* context, JS::HandleObject page, PageHelpers& helpers,
             interaction::Realm& events_, browser_environment::Environment& environment_)
    : cx(context),
      kit(context, page, helpers, this),
      model(),
      events(&events_),
      environment(&environment_),
      navigatorGpu(context),
      secure(context),
      gpu(context),
      adapter(context),
      device(context),
      queue(context),
      buffer(context),
      texture(context),
      texture_view(context),
      encoder(context),
      pass(context),
      command_buffer(context),
      features(context), wgsl_features(context), error(context), validation_error(context), memory_error(context),
      internal_error(context), limits(context), lost_info(context), adapter_info(context),
      bind_group(context), bind_group_layout(context), canvas_context(context),
      compilation_info(context), compilation_message(context), compute_pass_encoder(context),
      compute_pipeline(context), external_texture(context), pipeline_error(context),
      pipeline_layout(context), query_set(context), render_bundle(context),
      render_bundle_encoder(context), render_pipeline(context), sampler(context),
      shader_module(context) { RegisterState(*this); }

State::~State() { UnregisterState(*this); }

Realm::Realm(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
             interaction::Realm& events, browser_environment::Environment& environment)
    : state_(std::make_unique<State>(cx, page, helpers, events, environment)) {}
Realm::~Realm() = default;

State* Realm::state() const noexcept { return state_.get(); }

bool IllegalCreate(JSContext* cx, unsigned, JS::Value*) {
  return WebKit::Fail(cx, "Illegal constructor.");
}

bool SetString(JSContext* cx, JS::HandleObject target, int32_t slot, const char* ascii) {
  JS::RootedString text(cx, JS_NewStringCopyZ(cx, ascii));
  if (!text) return false;
  JS::SetReservedSlot(target, slot, JS::StringValue(text));
  return true;
}

bool SlotString(JSContext* cx, JS::HandleObject target, int32_t slot, std::string* out) {
  JS::RootedValue value(cx, JS::GetReservedSlot(target, slot));
  if (!value.isString()) {
    out->clear();
    return true;
  }
  JS::RootedString text(cx, value.toString());
  return ToUTF8(cx, text, out);
}

bool CheckGpu(JSContext* cx, JS::HandleValue self, JS::MutableHandleObject out) {
  return Check(cx, self, &kGpuClass, out, "GPU");
}
bool CheckAdapter(JSContext* cx, JS::HandleValue self, JS::MutableHandleObject out) {
  return Check(cx, self, &kAdapterClass, out, "GPUAdapter");
}
bool CheckDevice(JSContext* cx, JS::HandleValue self, JS::MutableHandleObject out) {
  return Check(cx, self, &kDeviceClass, out, "GPUDevice");
}
bool CheckBuffer(JSContext* cx, JS::HandleValue self, JS::MutableHandleObject out) {
  return Check(cx, self, &kBufferClass, out, "GPUBuffer");
}
bool CheckTexture(JSContext* cx, JS::HandleValue self, JS::MutableHandleObject out) {
  return Check(cx, self, &kTextureClass, out, "GPUTexture");
}
bool CheckTextureView(JSContext* cx, JS::HandleValue self, JS::MutableHandleObject out) {
  return Check(cx, self, &kTextureViewClass, out, "GPUTextureView");
}
bool CheckFeatures(JSContext* cx, JS::HandleValue self, JS::MutableHandleObject out) {
  return Check(cx, self, &kFeaturesClass, out, "GPUSupportedFeatures");
}
bool CheckQueue(JSContext* cx, JS::HandleValue self, JS::MutableHandleObject out) {
  return Check(cx, self, &kQueueClass, out, "GPUQueue");
}
bool CheckEncoder(JSContext* cx, JS::HandleValue self, JS::MutableHandleObject out) {
  return Check(cx, self, &kEncoderClass, out, "GPUCommandEncoder");
}
bool CheckPass(JSContext* cx, JS::HandleValue self, JS::MutableHandleObject out) {
  return Check(cx, self, &kPassClass, out, "GPURenderPassEncoder");
}
bool CheckCommandBuffer(JSContext* cx, JS::HandleValue self, JS::MutableHandleObject out) {
  return Check(cx, self, &kCommandBufferClass, out, "GPUCommandBuffer");
}

// The one error value factory: the value comes from the page realm's own
// DOMException/TypeError interface, so WebGPU owns no second error type.
bool MakeException(State& state, const char* interface, const char* name,
                   const char* message, JS::MutableHandleValue result) {
  JSContext* cx = state.cx;
  const char* key = (interface && interface[0]) ? interface : "DOMException";
  JS::RootedValue constructor(cx);
  if (!JS_GetProperty(cx, state.kit.page, key, &constructor) ||
      !constructor.isObject())
    return WebKit::Fail(cx, "WebGPU requires the page error interfaces");
  JS::RootedString text(cx, JS_NewStringCopyZ(cx, message ? message : ""));
  JS::RootedValueArray<2> arguments(cx);
  if (!text) return false;
  arguments[0].setString(text);
  if (name) {
    JS::RootedString label(cx, JS_NewStringCopyZ(cx, name));
    if (!label) return false;
    arguments[1].setString(label);
  } else {
    arguments[1].setUndefined();
  }
  JS::RootedObject error(cx);
  if (!JS::Construct(cx, constructor, arguments, &error)) return false;
  result.setObject(*error);
  return true;
}

bool ThrowError(State& state, const char* interface, const char* name,
                const char* message) {
  JS::RootedValue error(state.cx);
  if (!MakeException(state, interface, name, message, &error)) return false;
  JS_SetPendingException(state.cx, error);
  return false;
}

bool RejectError(State& state, const char* interface, const char* name,
                 const char* message, JS::MutableHandleValue result) {
  JS::RootedValue error(state.cx);
  if (!MakeException(state, interface, name, message, &error)) return false;
  return state.kit.RejectWith(error, result);
}

bool ThrowOperation(State& state, const char* message) {
  return ThrowError(state, "DOMException", "OperationError", message);
}

bool RejectOperation(State& state, const char* message, JS::MutableHandleValue result) {
  return RejectError(state, "DOMException", "OperationError", message, result);
}

bool HandlerGet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!CheckDevice(cx, args.thisv(), &self)) return false;
  args.rval().set(JS::GetReservedSlot(self, WebKit::Slot(args)));
  return true;
}

bool HandlerSet(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!CheckDevice(cx, args.thisv(), &self)) return false;
  const int32_t slot = WebKit::Slot(args);
  if (slot < kDeviceHandlerBase || slot >= kDeviceSlotCount)
    return WebKit::Fail(cx, "GPUDevice member called on an incompatible receiver");
  JS::RootedValue old(cx, JS::GetReservedSlot(self, slot));
  JS::RootedValue value(cx, args.get(0));
  const bool callable = value.isObject() && JS::IsCallable(&value.toObject());
  if (old.isObject() && !Delegate(cx, self, "removeEventListener", kDeviceHandler, old))
    return false;
  if (callable && !Delegate(cx, self, "addEventListener", kDeviceHandler, value))
    return false;
  if (!callable) value.setNull();
  JS::SetReservedSlot(self, slot, value);
  args.rval().setUndefined();
  return true;
}

}  // namespace ruyijs::web_gpu
