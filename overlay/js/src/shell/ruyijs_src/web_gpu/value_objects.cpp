#include "web_gpu/value_objects.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/PropertyDescriptor.h"
#include "js/Realm.h"
#include "js/String.h"
#include "js/Wrapper.h"
#include "runtime/runtime.h"

#include <cstring>

namespace ruyijs::web_gpu {
namespace {
// A non-owning directory of the existing realm owners, never an instance cache.
thread_local State* active = nullptr;
}
void RegisterState(State& state) { state.previous = active; active = &state; }
void UnregisterState(State& state) {
  State** link = &active;
  while (*link && *link != &state) link = &(*link)->previous;
  if (*link) *link = state.previous;
}
State* StateForGlobal(JSObject* global) {
  for (State* state = active; state; state = state->previous)
    if (state->kit.page == global) return state;
  return nullptr;
}
State* ValueReceiver(JSContext* cx, JS::HandleValue value, const JSClass* clasp,
                     JS::MutableHandleObject object) {
  object.set(value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  State* state = object && JS::GetClass(object) == clasp
                     ? StateForGlobal(JS::GetNonCCWObjectGlobal(object)) : nullptr;
  if (!state) WebKit::Fail(cx, "WebGPU member called on an incompatible receiver");
  return state;
}

const JSClass kLostInfoClass = {"GPUDeviceLostInfo", JSCLASS_HAS_RESERVED_SLOTS(2)};
bool LostReason(State& state, JS::MutableHandleValue out) {
  JSContext* cx = state.cx;
  {
    JSAutoRealm realm(cx, state.kit.page);
    JS::RootedObject info(cx, state.kit.New(&kLostInfoClass, state.lost_info.prototype));
    JS::RootedString reason(cx, JS_NewStringCopyZ(cx, "destroyed"));
    JS::RootedString message(cx, JS_NewStringCopyZ(cx, "the device was destroyed"));
    if (!info || !reason || !message) return false;
    JS::SetReservedSlot(info, 0, JS::StringValue(reason));
    JS::SetReservedSlot(info, 1, JS::StringValue(message));
    out.setObject(*info);
  }
  return JS_WrapValue(cx, out);
}
bool LostInfoProperty(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject object(cx);
  if (!ValueReceiver(cx, args.thisv(), &kLostInfoClass, &object)) return false;
  args.rval().set(JS::GetReservedSlot(object, WebKit::Slot(args)));
  return JS_WrapValue(cx, args.rval());
}

JSObject* NewAdapterInfo(State& state) {
  JSContext* cx = state.cx;
  JS::RootedObject info(cx, state.kit.New(&kAdapterInfoClass, state.adapter_info.prototype));
  if (!info) return nullptr;
  const std::string* strings[] = {&state.info_vendor, &state.info_architecture,
                                  &state.info_device, &state.info_description};
  for (int32_t slot = 0; slot < 4; ++slot) {
    JS::RootedString value(cx, JS_NewStringCopyZ(cx, strings[slot]->c_str()));
    if (!value) return nullptr;
    JS::SetReservedSlot(info, slot, JS::StringValue(value));
  }
  JS::SetReservedSlot(info, kAdapterInfoSubgroupMin, JS::Int32Value(0));
  JS::SetReservedSlot(info, kAdapterInfoSubgroupMax, JS::Int32Value(0));
  JS::SetReservedSlot(info, kAdapterInfoFallback, JS::BooleanValue(state.fallback));
  return info.get();
}

bool AdapterInfoProperty(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject object(cx);
  if (!ValueReceiver(cx, args.thisv(), &kAdapterInfoClass, &object)) return false;
  args.rval().set(JS::GetReservedSlot(object, WebKit::Slot(args)));
  return JS_WrapValue(cx, args.rval());
}

bool IsLabelClass(const JSClass* clasp) {
  return clasp == &kBindGroupClass || clasp == &kBindGroupLayoutClass ||
         clasp == &kComputePassEncoderClass || clasp == &kComputePipelineClass ||
         clasp == &kExternalTextureClass || clasp == &kPipelineLayoutClass ||
         clasp == &kRenderBundleClass || clasp == &kRenderBundleEncoderClass ||
         clasp == &kRenderPipelineClass || clasp == &kSamplerClass ||
         clasp == &kShaderModuleClass || clasp == &kCanvasContextClass;
}

bool LabelValueProperty(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject object(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (!object || !IsLabelClass(JS::GetClass(object)))
    return WebKit::Fail(cx, "WebGPU member called on an incompatible receiver");
  if (argc > 0) {
    JS::RootedString value(cx, JS::ToString(cx, args.get(0)));
    if (!value) return false;
    JS::SetReservedSlot(object, kLabelValue, JS::StringValue(value));
    args.rval().setUndefined();
    return true;
  }
  args.rval().set(JS::GetReservedSlot(object, kLabelValue));
  return JS_WrapValue(cx, args.rval());
}

bool CanvasProperty(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject object(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  if (!object || JS::GetClass(object) != &kCanvasContextClass)
    return WebKit::Fail(cx, "GPUCanvasContext member called on an incompatible receiver");
  args.rval().setNull();
  return true;
}

bool CompilationInfoProperty(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject object(cx);
  if (!ValueReceiver(cx, args.thisv(), &kCompilationInfoClass, &object)) return false;
  args.rval().set(JS::GetReservedSlot(object, kCompilationMessages));
  return JS_WrapValue(cx, args.rval());
}

bool CompilationMessageProperty(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject object(cx);
  if (!ValueReceiver(cx, args.thisv(), &kCompilationMessageClass, &object)) return false;
  args.rval().set(JS::GetReservedSlot(object, WebKit::Slot(args)));
  return JS_WrapValue(cx, args.rval());
}

bool PipelineErrorProperty(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject object(cx);
  object.set(args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  State* state = object ? StateForGlobal(JS::GetNonCCWObjectGlobal(object)) : nullptr;
  if (!state) return WebKit::Fail(cx, "GPUPipelineError member called on an incompatible receiver");
  bool branded = JS::GetClass(object) == &kPipelineErrorClass;
  for (JS::RootedObject current(cx, object); !branded && current;) {
    if (current == state->pipeline_error.prototype) branded = true;
    JS::RootedObject next(cx);
    if (!JS_GetPrototype(cx, current, &next)) return false;
    current = next;
  }
  if (!branded) return WebKit::Fail(cx, "GPUPipelineError member called on an incompatible receiver");
  JS::Rooted<mozilla::Maybe<JS::PropertyDescriptor>> descriptor(cx);
  if (!JS_GetOwnPropertyDescriptor(cx, object, "reason", &descriptor)) return false;
  if (descriptor.isNothing() || !descriptor->isDataDescriptor())
    return WebKit::Fail(cx, "GPUPipelineError member called on an incompatible receiver");
  args.rval().set(descriptor->value());
  return JS_WrapValue(cx, args.rval());
}

bool ValueMethod(JSContext* cx, unsigned, JS::Value*) {
  return WebKit::Fail(cx, "WebGPU operation is not implemented");
}

bool PipelineErrorConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  if (!args.isConstructing()) return WebKit::Fail(cx, "GPUPipelineError constructor requires new");
  State& state = *static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject target(cx, &args.newTarget().toObject());
  JS::RootedValue prototypeValue(cx);
  if (!JS_GetProperty(cx, target, "prototype", &prototypeValue)) return false;
  JS::RootedObject prototype(cx, prototypeValue.isObject()
      ? &prototypeValue.toObject() : state.pipeline_error.prototype);
  JS::RootedValue domConstructor(cx);
  if (!JS_GetProperty(cx, state.kit.page, "DOMException", &domConstructor) ||
      !domConstructor.isObject()) return WebKit::Fail(cx, "DOMException is required by GPUPipelineError");
  JS::RootedValue message(cx);
  if (argc) {
    message.set(args[0]);
  } else {
    JS::RootedString empty(cx, JS_NewStringCopyZ(cx, ""));
    if (!empty) return false;
    message.setString(empty);
  }
  if (argc < 2 || !args[1].isObject())
    return WebKit::Fail(cx, "GPUPipelineError requires a reason option");
  JS::RootedObject options(cx, &args[1].toObject());
  JS::RootedValue reasonValue(cx);
  if (!JS_GetProperty(cx, options, "reason", &reasonValue) ||
      reasonValue.isUndefined()) return WebKit::Fail(cx, "GPUPipelineError requires a reason option");
  JS::RootedString reasonText(cx, JS::ToString(cx, reasonValue));
  std::string reason;
  if (!reasonText || !ToUTF8(cx, reasonText, &reason) ||
      (reason != "validation" && reason != "internal"))
    return WebKit::Fail(cx, "GPUPipelineError reason is not a valid value");
  JS::RootedString name(cx, JS_NewStringCopyZ(cx, "GPUPipelineError"));
  if (!name) return false;
  JS::RootedValueArray<2> values(cx);
  values[0].set(message);
  values[1].setString(name);
  JS::RootedObject object(cx);
  if (!JS::Construct(cx, domConstructor, values, &object) || !JS_SetPrototype(cx, object, prototype))
    return false;
  JS::RootedString reasonString(cx, JS_NewStringCopyZ(cx, reason.c_str()));
  if (!reasonString || !JS_DefineProperty(cx, object, "reason", reasonString,
                                    JSPROP_READONLY | JSPROP_PERMANENT)) return false;
  args.rval().setObject(*object);
  return true;
}
}
