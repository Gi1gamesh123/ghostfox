// getParameter, getSupportedExtensions, getExtension and getError. Every value
// they answer comes from the model bridge (Firefox's masked and unmasked GL
// strings plus the imported numeric limits), so no string or limit is
// duplicated in this adapter.
#include "web_graphics/internal.h"

#include "js/PropertyAndElement.h"
#include "js/experimental/TypedData.h"

namespace ruyijs::web_graphics {
namespace {
bool StringValue(JSContext* cx, const std::string& text, JS::MutableHandleValue out) {
  JS::RootedString string(cx, JS_NewStringCopyN(cx, text.data(), text.size()));
  if (!string) return false;
  out.setString(string);
  return true;
}

bool ParameterStateResult(JSContext* cx, const ParameterStateValue& state,
                          JS::MutableHandleValue out) {
  if (state.kind == ParameterKind::Null) {
    out.setNull();
    return true;
  }
  if (state.kind == ParameterKind::Number) {
    out.setNumber(state.values[0]);
    return true;
  }
  if (state.kind == ParameterKind::Boolean) {
    out.setBoolean(state.values[0] != 0);
    return true;
  }
  JS::RootedObject array(cx, state.kind == ParameterKind::FloatArray
                                 ? JS_NewFloat32Array(cx, state.count)
                                 : state.kind == ParameterKind::IntArray
                                       ? JS_NewInt32Array(cx, state.count)
                                       : JS::NewArrayObject(cx, state.count));
  if (!array) return false;
  for (int i = 0; i < state.count; ++i) {
    JS::RootedValue value(cx, JS::NumberValue(state.values[i]));
    if (state.kind == ParameterKind::BoolArray)
      value.setBoolean(state.values[i] != 0);
    if (!JS_SetElement(cx, array, i, value)) return false;
  }
  out.setObject(*array);
  return true;
}

}  // namespace

bool GetParameter(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *detail::Self(args);
  JS::RootedObject self(cx);
  if (!detail::ContextReceiver(cx, args, &self)) return false;
  Context* context = detail::ContextOf(self);
  int32_t parameter = 0;
  if (!detail::ToEnum(cx, args.get(0), &parameter)) return false;
  const GlValues& values = state.model.values();
  // Extension enums answer only after getExtension enabled the extension;
  // otherwise Firefox reports INVALID_ENUM and returns null.
  const char* gate = parameter == kUnmaskedVendorWebGL || parameter == kUnmaskedRendererWebGL
                         ? "WEBGL_debug_renderer_info"
                         : parameter == kMaxTextureMaxAnisotropy ? "EXT_texture_filter_anisotropic" : nullptr;
  if (gate) {
    bool enabled = false;
    if (!ExtensionEnabled(state, *context, gate, &enabled)) return false;
    if (!enabled) {
      detail::SetError(*context, kInvalidEnum);
      args.rval().setNull();
      return true;
    }
  }
  // A profile-pinned limit answers before the built-in table, and only for the
  // pnames whose Firefox answer is a plain number (model.cpp validated that).
  double limit = 0;
  if (state.model.Limit(parameter, &limit)) {
    if (parameter != kMaxViewportDims) {
      args.rval().setNumber(limit);
      return true;
    }
    JS::RootedObject dims(cx, JS_NewInt32Array(cx, 2));
    if (!dims || !JS_SetElement(cx, dims, 0, int32_t(limit)) ||
        !JS_SetElement(cx, dims, 1, int32_t(limit))) return false;
    args.rval().setObject(*dims);
    return true;
  }
  ParameterStateValue parameterState;
  if (GetParameterState(*context, parameter, &parameterState))
    return ParameterStateResult(cx, parameterState, args.rval());
  switch (parameter) {
    case kVendor: return StringValue(cx, values.vendor, args.rval());
    case kRenderer: return StringValue(cx, values.renderer, args.rval());
    case kUnmaskedVendorWebGL:
      return StringValue(cx, values.unmasked_vendor, args.rval());
    case kUnmaskedRendererWebGL:
      return StringValue(cx, values.unmasked_renderer, args.rval());
    case kVersion: return StringValue(cx, context->version == 2 ? values.version2 : values.version, args.rval());
    case kShadingLanguageVersion:
      return StringValue(cx, context->version == 2 ? values.shading_language_version2 : values.shading_language_version, args.rval());
    case kMaxTextureSize: args.rval().setInt32(values.max_texture_size); return true;
    case kMaxRenderbufferSize:
      args.rval().setInt32(values.max_renderbuffer_size); return true;
    case kMaxVertexAttribs: args.rval().setInt32(values.max_vertex_attribs); return true;
    case kMaxTextureImageUnits:
      args.rval().setInt32(values.max_texture_image_units); return true;
    case kMaxTextureMaxAnisotropy:
      args.rval().setNumber(values.max_texture_max_anisotropy); return true;
    case kMaxViewportDims: {
      // MAX_VIEWPORT_DIMS is the one query that answers an Int32Array of two
      // elements; the specification's other MAX_* queries answer numbers.
      JS::RootedObject array(cx, JS_NewInt32Array(cx, 2));
      if (!array ||
          !JS_SetElement(cx, array, 0, values.max_viewport_dims_width) ||
          !JS_SetElement(cx, array, 1, values.max_viewport_dims_height))
        return false;
      args.rval().setObject(*array);
      return true;
    }
    default: {
      bool found = false;
      if (!DefaultParameter(state, *context, parameter, args.rval(), &found)) return false;
      if (found) return true;
      // An unknown pname is a WebGL error, never a JavaScript exception.
      if (context) detail::SetError(*context, kInvalidEnum);
      args.rval().setNull();
      return true;
    }
  }
}

bool GetSupportedExtensions(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *detail::Self(args);
  JS::RootedObject self(cx);
  if (!detail::ContextReceiver(cx, args, &self)) return false;
  const auto& values = state.model.values();
  const auto& names = detail::ContextOf(self)->version == 2 ? values.supported_extensions2 : values.supported_extensions;
  JS::RootedObject array(cx, JS::NewArrayObject(cx, names.size()));
  if (!array) return false;
  for (size_t index = 0; index < names.size(); ++index) {
    JS::RootedString name(cx, JS_NewStringCopyN(cx, names[index].data(), names[index].size()));
    if (!name || !JS_SetElement(cx, array, index, name)) return false;
  }
  args.rval().setObject(*array);
  return true;
}

bool GetExtension(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *detail::Self(args);
  JS::RootedObject self(cx);
  if (!detail::ContextReceiver(cx, args, &self)) return false;
  if (args.length() == 0) {
    args.rval().setNull();
    return true;
  }
  JS::RootedString text(cx, JS::ToString(cx, args.get(0)));
  if (!text) return false;
  return NewExtension(state, *detail::ContextOf(self), text, args.rval());
}

bool GetError(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!detail::ContextReceiver(cx, args, &self)) return false;
  Context* context = detail::ContextOf(self);
  if (!context) {
    args.rval().setInt32(kNoError);
    return true;
  }
  args.rval().setInt32(context->error);
  context->error = kNoError;
  return true;
}
}  // namespace ruyijs::web_graphics
