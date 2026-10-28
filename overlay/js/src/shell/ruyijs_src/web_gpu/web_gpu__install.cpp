#include "web_gpu/state.h"
#include "web_gpu/value_objects.h"

#include <iterator>
#include <string>

#include "browser_environment/environment.h"
#include "jsfriendapi.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/Symbol.h"
#include "runtime/page_helpers.h"

namespace ruyijs::web_gpu {
namespace {

struct Property {
  const char* name;
  int32_t slot;
};

const Property kAdapterProperties[] = {
    {"features", kAdapterFeatures}, {"limits", kAdapterLimits},
    {"info", kAdapterInfo},
};
const Property kDeviceProperties[] = {
    {"features", kDeviceFeatures}, {"limits", kDeviceLimits},
    {"queue", kDeviceQueue}, {"lost", kDeviceLost},
};
const Property kBufferProperties[] = {
    {"size", kBufferSize}, {"usage", kBufferUsage}, {"mapState", kBufferMapState},
    {"label", kBufferLabel},
};
const Property kTextureProperties[] = {
    {"width", kTextureWidth}, {"height", kTextureHeight},
    {"depthOrArrayLayers", kTextureDepth}, {"mipLevelCount", kTextureMipLevelCount},
    {"sampleCount", kTextureSampleCount}, {"dimension", kTextureDimension},
    {"format", kTextureFormat}, {"usage", kTextureUsage}, {"label", kTextureLabel},
};

struct Method {
  const char* name;
  JSNative native;
  unsigned length;
  int32_t slot;
};

const Method kGpuMethods[] = {
    {"requestAdapter", GpuMethod, 0, kGpuRequestAdapter},
    {"getPreferredCanvasFormat", GpuMethod, 0, kGpuPreferredCanvasFormat},
};
const Method kAdapterMethods[] = {
    {"requestDevice", AdapterMethod, 0, kAdapterRequestDevice},
};
const Method kDeviceMethods[] = {
    {"createBuffer", DeviceMethod, 1, kDeviceCreateBuffer},
    {"createTexture", DeviceMethod, 1, kDeviceCreateTexture},
    {"createCommandEncoder", DeviceMethod, 0, kDeviceCreateCommandEncoder},
    {"destroy", DeviceMethod, 0, kDeviceDestroy},
};
const Method kQueueMethods[] = {
    {"submit", QueueMethod, 1, kQueueSubmit},
    {"onSubmittedWorkDone", QueueMethod, 0, kQueueOnSubmittedWorkDone},
};
const Method kBufferMethods[] = {
    {"mapAsync", BufferMethod, 1, kBufferMapAsync},
    {"getMappedRange", BufferMethod, 0, kBufferGetMappedRange},
    {"unmap", BufferMethod, 0, kBufferUnmap},
    {"destroy", BufferMethod, 0, kBufferDestroy},
};
const Method kTextureMethods[] = {
    {"createView", TextureMethod, 0, kTextureCreateView},
    {"destroy", TextureMethod, 0, kTextureDestroy},
};
const Method kEncoderMethods[] = {
    {"beginRenderPass", EncoderMethod, 1, kEncoderBeginRenderPass},
    {"finish", EncoderMethod, 0, kEncoderFinish},
};
const Method kPassMethods[] = {
    {"end", PassMethod, 0, kPassEnd},
};
bool Methods(State& state, WebFace& face, const char* iface, const Method* methods,
             size_t count) {
  for (size_t index = 0; index < count; ++index)
    if (!state.kit.Method(face, iface, methods[index].name, methods[index].native,
                          methods[index].length, methods[index].slot))
      return false;
  return true;
}

bool Properties(State& state, WebFace& face, const char* iface, const Property* properties,
                size_t count, JSNative getter) {
  for (size_t index = 0; index < count; ++index)
    if (!state.kit.Accessor(face, iface, properties[index].name, getter, nullptr,
                            properties[index].slot))
      return false;
  return true;
}

// The WebGPU flag namespaces (GPUMapMode, GPUBufferUsage, GPUTextureUsage,
// GPUShaderStage, GPUColorWrite) are WebIDL namespace objects: an ordinary
// extensible object whose constants are read-only, enumerable and permanent,
// with a configurable @@toStringTag; the global binding is writable,
// non-enumerable and configurable. One installer for all five.
struct FlagConstant { const char* name; uint32_t value; };
bool InstallNamespace(State& state, const char* name, const FlagConstant* constants, size_t count) {
  JSContext* cx = state.cx;
  JS::RootedObject space(cx, JS_NewPlainObject(cx));
  if (!space) return false;
  for (size_t i = 0; i < count; ++i)
    if (!JS_DefineProperty(cx, space, constants[i].name, constants[i].value,
                           JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT)) return false;
  JS::RootedId tag(cx, JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(cx, JS::SymbolCode::toStringTag)));
  JS::RootedString text(cx, JS_NewStringCopyZ(cx, name));
  return text && JS_DefinePropertyById(cx, space, tag, text, JSPROP_READONLY) &&
         JS_DefineProperty(cx, state.kit.page, name, space, 0);
}
bool InstallNamespaces(State& state) {
  static const FlagConstant kMapMode[] = {{"READ", kMapModeRead}, {"WRITE", kMapModeWrite}};
  static const FlagConstant kBufferUsage[] = {
      {"MAP_READ", 0x1}, {"MAP_WRITE", 0x2}, {"COPY_SRC", 0x4}, {"COPY_DST", 0x8}, {"INDEX", 0x10},
      {"VERTEX", 0x20}, {"UNIFORM", 0x40}, {"STORAGE", 0x80}, {"INDIRECT", 0x100}, {"QUERY_RESOLVE", 0x200}};
  static const FlagConstant kTextureUsage[] = {
      {"COPY_SRC", 0x1}, {"COPY_DST", 0x2}, {"TEXTURE_BINDING", 0x4}, {"STORAGE_BINDING", 0x8},
      {"RENDER_ATTACHMENT", 0x10}, {"TRANSIENT_ATTACHMENT", 0x20}};
  static const FlagConstant kShaderStage[] = {{"VERTEX", 0x1}, {"FRAGMENT", 0x2}, {"COMPUTE", 0x4}};
  static const FlagConstant kColorWrite[] = {{"RED", 0x1}, {"GREEN", 0x2}, {"BLUE", 0x4}, {"ALPHA", 0x8}, {"ALL", 0xF}};
  return InstallNamespace(state, "GPUBufferUsage", kBufferUsage, std::size(kBufferUsage)) &&
         InstallNamespace(state, "GPUMapMode", kMapMode, std::size(kMapMode)) &&
         InstallNamespace(state, "GPUTextureUsage", kTextureUsage, std::size(kTextureUsage)) &&
         InstallNamespace(state, "GPUShaderStage", kShaderStage, std::size(kShaderStage)) &&
         InstallNamespace(state, "GPUColorWrite", kColorWrite, std::size(kColorWrite));
}

}  // namespace

bool Realm::Install() {
  State& s = *state_;
  const JSAutoRealm realm(s.cx, s.kit.page);
  JS::RootedObject objectProto(s.cx, JS::GetRealmObjectPrototype(s.cx));
  JS::RootedObject targetProto(s.cx, s.events->prototype());
  if (!objectProto || !targetProto) return false;
  // GPUDevice is the specification's EventTarget; every other interface is a
  // plain object interface. All of them are realm-local.
  if (!s.kit.Define("GPU", &kGpuClass, objectProto, IllegalCreate, &s.gpu) ||
      !s.kit.Define("GPUAdapter", &kAdapterClass, objectProto, IllegalCreate, &s.adapter) ||
      !s.kit.Define("GPUDevice", &kDeviceClass, targetProto, IllegalCreate, &s.device) ||
      !s.kit.Define("GPUQueue", &kQueueClass, objectProto, IllegalCreate, &s.queue) ||
      !s.kit.Define("GPUBuffer", &kBufferClass, objectProto, IllegalCreate, &s.buffer) ||
      !s.kit.Define("GPUTexture", &kTextureClass, objectProto, IllegalCreate, &s.texture) ||
      !s.kit.Define("GPUTextureView", &kTextureViewClass, objectProto, IllegalCreate,
                    &s.texture_view) ||
      !s.kit.Define("GPUCommandEncoder", &kEncoderClass, objectProto, IllegalCreate,
                    &s.encoder) ||
      !s.kit.Define("GPURenderPassEncoder", &kPassClass, objectProto, IllegalCreate,
                    &s.pass) ||
      !s.kit.Define("GPUCommandBuffer", &kCommandBufferClass, objectProto, IllegalCreate,
                    &s.command_buffer) || !InstallFeatureInterfaces(s))
    return false;
  if (!Properties(s, s.adapter, "GPUAdapter", kAdapterProperties,
                  std::size(kAdapterProperties), AdapterProperty) ||
      !Properties(s, s.device, "GPUDevice", kDeviceProperties,
                  std::size(kDeviceProperties), DeviceProperty) ||
      !Properties(s, s.buffer, "GPUBuffer", kBufferProperties,
                  std::size(kBufferProperties), BufferProperty) ||
      !Properties(s, s.texture, "GPUTexture", kTextureProperties,
                  std::size(kTextureProperties), TextureProperty) ||
      !s.kit.Accessor(s.texture_view, "GPUTextureView", "label", TextureViewProperty,
                      nullptr, kTextureViewLabel) ||
      !s.kit.Accessor(s.encoder, "GPUCommandEncoder", "label", EncoderProperty,
                      nullptr, kEncoderLabel) ||
      !s.kit.Accessor(s.pass, "GPURenderPassEncoder", "label", PassProperty,
                      nullptr, kPassLabel) ||
      !s.kit.Accessor(s.command_buffer, "GPUCommandBuffer", "label",
                      CommandBufferProperty, nullptr, kCommandBufferLabel) ||
      !s.kit.Accessor(s.device, "GPUDevice", "onuncapturederror", HandlerGet,
                      HandlerSet, kDeviceHandlerBase))
    return false;
  if (!Methods(s, s.gpu, "GPU", kGpuMethods, std::size(kGpuMethods)) ||
      !Methods(s, s.adapter, "GPUAdapter", kAdapterMethods, std::size(kAdapterMethods)) ||
      !Methods(s, s.device, "GPUDevice", kDeviceMethods, std::size(kDeviceMethods)) ||
      !Methods(s, s.queue, "GPUQueue", kQueueMethods, std::size(kQueueMethods)) ||
      !Methods(s, s.buffer, "GPUBuffer", kBufferMethods, std::size(kBufferMethods)) ||
      !Methods(s, s.texture, "GPUTexture", kTextureMethods, std::size(kTextureMethods)) ||
      !Methods(s, s.encoder, "GPUCommandEncoder", kEncoderMethods,
               std::size(kEncoderMethods)) ||
      !Methods(s, s.pass, "GPURenderPassEncoder", kPassMethods, std::size(kPassMethods)))
    return false;
  if (!InstallNamespaces(s) || !InstallValueInterfaces(s)) return false;
  // navigator.gpu stays with the browser_environment Navigator owner; this
  // domain appends one accessor to that realm's Navigator prototype and keeps
  // the GPU instance in its own state.
  JS::RootedObject navigatorPrototype(s.cx, s.environment->navigatorPrototype());
  if (!navigatorPrototype) return false;
  JS::RootedObject getter(s.cx, s.kit.Function(NavigatorGpu, "get gpu", 0, 0));
  JS::RootedObject noSetter(s.cx);
  if (!getter) return false;
  if (!JS_DefineProperty(s.cx, navigatorPrototype, "gpu", getter, noSetter,
                         JSPROP_ENUMERATE) ||
      !s.kit.helpers->Accessor("Navigator", navigatorPrototype, "gpu"))
    return false;
  s.environment->SetGpuPort(&GpuPort, &s);
  s.environment->SetGpuDescribe(&GpuDescribe, &s);
  // Register every global this owner defined (GPU*, WGSLLanguageFeatures) and
  // navigator.gpu as [SecureContext].
  JS::RootedIdVector keys(s.cx);
  if (!js::GetPropertyKeys(s.cx, s.kit.page, JSITER_OWNONLY | JSITER_HIDDEN, &keys)) return false;
  for (size_t i = 0; i < keys.length(); ++i) {
    if (!keys[i].isString()) continue;
    JS::RootedString key(s.cx, keys[i].toString());
    JS::UniqueChars name = JS_EncodeStringToUTF8(s.cx, key);
    if (!name) return false;
    const std::string text(name.get());
    if ((text.rfind("GPU", 0) == 0 || text == "WGSLLanguageFeatures") && !s.secure.Add(s.kit.page, name.get()))
      return false;
  }
  return s.secure.Add(navigatorPrototype, "gpu");
}

bool Realm::ConfigureExposure() {
  State& s = *state_;
  const JSAutoRealm realm(s.cx, s.kit.page);
  JS::RootedValue secure(s.cx);
  return JS_GetProperty(s.cx, s.kit.page, "isSecureContext", &secure) && s.secure.Apply(secure.isTrue());
}

}  // namespace ruyijs::web_gpu
