// GPUTexture and GPUTextureView. A texture records its descriptor only; there
// is no GPU memory behind it, so createView() and destroy() are bookkeeping.
#include "web_gpu/internal.h"

#include "js/Realm.h"

namespace ruyijs::web_gpu {
namespace {

// Reads a numeric member with a default for a missing/undefined one.
bool Member(JSContext* cx, JS::HandleObject source, const char* name, double fallback,
            double* out) {
  JS::RootedValue value(cx);
  if (!JS_GetProperty(cx, source, name, &value)) return false;
  if (value.isUndefined()) {
    *out = fallback;
    return true;
  }
  return ReadDouble(cx, value, out);
}

}  // namespace

JSObject* NewTexture(State& state, JS::HandleObject device, JS::HandleObject size,
                     JS::HandleString format, uint32_t usage, uint32_t mip_level_count,
                     uint32_t sample_count, JS::HandleString dimension,
                     JS::HandleString label) {
  (void)device;  // The device is the owner, not a data source for the texture.
  JSContext* cx = state.cx;
  const JSAutoRealm realm(cx, state.kit.page);
  JS::RootedObject texture(cx, state.kit.New(&kTextureClass, state.texture.prototype));
  double width = 1.0, height = 1.0, depth = 1.0;
  if (!texture || !Member(cx, size, "width", 1.0, &width) ||
      !Member(cx, size, "height", 1.0, &height) ||
      !Member(cx, size, "depthOrArrayLayers", 1.0, &depth))
    return nullptr;
  JS::SetReservedSlot(texture, kTextureWidth, JS::DoubleValue(width));
  JS::SetReservedSlot(texture, kTextureHeight, JS::DoubleValue(height));
  JS::SetReservedSlot(texture, kTextureDepth, JS::DoubleValue(depth));
  JS::SetReservedSlot(texture, kTextureMipLevelCount, JS::NumberValue(mip_level_count));
  JS::SetReservedSlot(texture, kTextureSampleCount, JS::NumberValue(sample_count));
  JS::SetReservedSlot(texture, kTextureDimension, JS::StringValue(dimension));
  JS::SetReservedSlot(texture, kTextureFormat, JS::StringValue(format));
  JS::SetReservedSlot(texture, kTextureUsage, JS::NumberValue(usage));
  JS::SetReservedSlot(texture, kTextureDestroyed, JS::BooleanValue(false));
  JS::SetReservedSlot(texture, kTextureLabel, JS::StringValue(label));
  return texture.get();
}

JSObject* NewTextureView(State& state, JS::HandleString label) {
  JSContext* cx = state.cx;
  const JSAutoRealm realm(cx, state.kit.page);
  JS::RootedObject view(cx, state.kit.New(&kTextureViewClass, state.texture_view.prototype));
  if (!view) return nullptr;
  JS::SetReservedSlot(view, kTextureViewLabel, JS::StringValue(label));
  return view.get();
}

bool TextureProperty(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = static_cast<State*>(WebKit::Owner(args));
  (void)state;
  JS::RootedObject self(cx);
  if (!CheckTexture(cx, args.thisv(), &self)) return false;
  switch (WebKit::Slot(args)) {
    case kTextureDimension:
    case kTextureFormat:
    case kTextureLabel: {
      std::string text;
      if (!SlotString(cx, self, WebKit::Slot(args), &text)) return false;
      return StringValue(cx, text, args.rval());
    }
    default:
      args.rval().setDouble(JS::GetReservedSlot(self, WebKit::Slot(args)).toNumber());
      return true;
  }
}

bool TextureViewProperty(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!CheckTextureView(cx, args.thisv(), &self)) return false;
  std::string text;
  if (!SlotString(cx, self, kTextureViewLabel, &text)) return false;
  return StringValue(cx, text, args.rval());
}

bool TextureMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject self(cx);
  if (!CheckTexture(cx, args.thisv(), &self)) return false;
  switch (WebKit::Slot(args)) {
    case kTextureCreateView: {
      if (JS::GetReservedSlot(self, kTextureDestroyed).toBoolean())
        return ThrowOperation(*state, "the texture is destroyed");
      JS::RootedValue descriptor(cx, args.get(0));
      if (!descriptor.isUndefined() && !descriptor.isObject())
        return ThrowError(*state, "TypeError", nullptr,
                          "createView requires a descriptor dictionary");
      JS::RootedString label(cx, JS_NewStringCopyZ(cx, ""));
      if (!label) return false;
      if (descriptor.isObject()) {
        JS::RootedObject dictionary(cx, &descriptor.toObject());
        if (!ReadLabel(cx, dictionary, &label)) return false;
      }
      JS::RootedObject view(cx, NewTextureView(*state, label));
      if (!view) return false;
      args.rval().setObject(*view);
      return true;
    }
    default: {  // destroy
      JS::SetReservedSlot(self, kTextureDestroyed, JS::BooleanValue(true));
      args.rval().setUndefined();
      return true;
    }
  }
}

}  // namespace ruyijs::web_gpu
