// The shader and program half of the draw path: the sources, the compile/link
// flags, the current program, and the parameter queries the fingerprinting draw
// probe reads. No pixel work happens here; draw.cpp owns the rasterizer.
#include "web_graphics/draw_calls.h"

#include "web_graphics/internal.h"
#include "web_graphics/public_calls.h"

namespace ruyijs::web_graphics {
using namespace draw_call_detail;
namespace {
bool InvalidOperation(Context& context) {
  detail::SetError(context, kInvalidOperation);
  return true;
}
// The attached shaders of a program as a JS array, in attachment order.
bool AttachedArray(JSContext* cx, JSObject* program, JS::MutableHandleValue out) {
  JS::RootedObject array(cx, JS::NewArrayObject(cx, 0));
  if (!array) return false;
  uint32_t length = 0;
  for (int32_t stage = 0; stage < 2; ++stage) {
    JSObject* shader = StageOf(program, stage);
    if (!shader) continue;
    JS::RootedValue value(cx, JS::ObjectValue(*shader));
    if (!JS_SetElement(cx, array, length++, value)) return false;
  }
  out.setObject(*array);
  return true;
}
}  // namespace

bool ProgramOperation(JSContext* cx, const JS::CallArgs& args, State& state, Context& context,
                      int op) {
  switch (op) {
    case kGetProgramParameter: case kGetShaderParameter: {
      const int32_t kind = op == kGetProgramParameter ? kProgramObject : kShaderObject;
      JSObject* object = Owned(state, context, args.get(0), kind);
      int32_t pname = 0;
      if (!detail::ToEnum(cx, args.get(1), &pname)) return false;
      if (!object) return InvalidOperation(context);
      if (kind == kShaderObject) {
        const int32_t type = JS::GetReservedSlot(object, detail::kObjectExtraSlot).toInt32();
        const bool compiled = JS::GetReservedSlot(object, detail::kObjectFlagSlot).toBoolean();
        // SHADER_TYPE, DELETE_STATUS and COMPILE_STATUS are the shader's own
        // state; every other pname of this context is answered by getParameter.
        if (pname == 0x8B4F) { args.rval().setInt32(type); return true; }
        if (pname == 0x8B80) { args.rval().setBoolean(false); return true; }
        if (pname == 0x8B81) { args.rval().setBoolean(compiled); return true; }
        args.rval().setNull();
        return true;
      }
      // LINK_STATUS and VALIDATE_STATUS only report a link, and a program that
      // never linked reports false, as the specification requires.
      const bool linked = JS::GetReservedSlot(object, detail::kObjectFlagSlot).toBoolean();
      if (pname == 0x8B82 || pname == 0x8B83) { args.rval().setBoolean(linked); return true; }
      if (pname == 0x8B80) { args.rval().setBoolean(false); return true; }
      if (pname == 0x8B85) return AttachedArray(cx, object, args.rval());
      // ACTIVE_ATTRIBUTES and ACTIVE_UNIFORMS count the declarations of the
      // linked shaders; an unused uniform is not culled, which is the one
      // simplification this counter makes.
      if (pname == 0x8B89 || pname == 0x8B86) {
        std::vector<GlslDeclaration> declared;
        const char* kind = pname == 0x8B89 ? "attribute" : "uniform";
        for (int32_t stage = 0; stage < 2; ++stage) {
          std::vector<GlslDeclaration> stageDeclarations;
          StageDeclarations(cx, object, stage, kind, &stageDeclarations);
          for (const GlslDeclaration& entry : stageDeclarations) {
            bool seen = false;
            for (const GlslDeclaration& known : declared) seen = seen || known.name == entry.name;
            if (!seen) declared.push_back(entry);
          }
        }
        args.rval().setInt32(int32_t(declared.size()));
        return true;
      }
      args.rval().setNull();
      return true;
    }
    case kGetShaderSource: {
      JSObject* shader = Owned(state, context, args.get(0), kShaderObject);
      if (!shader) return InvalidOperation(context);
      std::string source;
      if (!ShaderSource(cx, shader, &source)) { args.rval().setNull(); return true; }
      JS::RootedString text(cx, JS_NewStringCopyN(cx, source.data(), source.size()));
      if (!text) return false;
      args.rval().setString(text);
      return true;
    }
    case kGetAttachedShaders: {
      JSObject* program = Owned(state, context, args.get(0), kProgramObject);
      if (!program) return InvalidOperation(context);
      return AttachedArray(cx, program, args.rval());
    }
    case kGetAttribLocation: {
      JSObject* program = Owned(state, context, args.get(0), kProgramObject);
      std::string name;
      if (!TextOf(cx, args.get(1), &name)) return false;
      if (!program) return InvalidOperation(context);
      std::vector<GlslDeclaration> attributes;
      StageDeclarations(cx, program, 0, "attribute", &attributes);
      int32_t location = -1;
      for (size_t i = 0; i < attributes.size(); ++i)
        if (attributes[i].name == name) location = int32_t(i);
      args.rval().setInt32(location);
      return true;
    }
    case kShaderSource: {
      JSObject* shader = Owned(state, context, args.get(0), kShaderObject);
      std::string source;
      if (!TextOf(cx, args.get(1), &source)) return false;
      if (!shader) return InvalidOperation(context);
      JS::RootedString text(cx, JS_NewStringCopyN(cx, source.data(), source.size()));
      if (!text) return false;
      JS::SetReservedSlot(shader, detail::kObjectDataSlot, JS::StringValue(text));
      return true;
    }
    case kCompileShader: {
      JSObject* shader = Owned(state, context, args.get(0), kShaderObject);
      if (!shader) return InvalidOperation(context);
      // A shader whose source this evaluator cannot read fails to compile, which
      // is what an unsupported construct has to look like.
      std::string source;
      GlslStage parsed;
      const bool compiled = ShaderSource(cx, shader, &source) && GlslParse(source, &parsed) &&
                            !parsed.statements.empty();
      JS::SetReservedSlot(shader, detail::kObjectFlagSlot, JS::BooleanValue(compiled));
      return true;
    }
    case kAttachShader: {
      JSObject* program = Owned(state, context, args.get(0), kProgramObject);
      JSObject* shader = Owned(state, context, args.get(1), kShaderObject);
      if (!program || !shader) return InvalidOperation(context);
      const int32_t type = JS::GetReservedSlot(shader, detail::kObjectExtraSlot).toInt32();
      JS::SetReservedSlot(program, type == kVertexShader ? detail::kObjectDataSlot
                                                        : detail::kObjectExtraSlot,
                          JS::ObjectValue(*shader));
      return true;
    }
    case kLinkProgram: {
      JSObject* program = Owned(state, context, args.get(0), kProgramObject);
      if (!program) return InvalidOperation(context);
      bool linked = true;
      for (int32_t stage = 0; stage < 2; ++stage) {
        JSObject* shader = StageOf(program, stage);
        linked = linked && shader &&
                 JS::GetReservedSlot(shader, detail::kObjectFlagSlot).toBoolean();
      }
      JS::SetReservedSlot(program, detail::kObjectFlagSlot, JS::BooleanValue(linked));
      return true;
    }
    case kUseProgram: {
      context.currentProgram = nullptr;
      if (args.get(0).isNull() || args.get(0).isUndefined()) return true;
      JSObject* program = Owned(state, context, args.get(0), kProgramObject);
      if (!program) return InvalidOperation(context);
      context.currentProgram = program;
      return true;
    }
    default:
      return true;
  }
}
}  // namespace ruyijs::web_graphics
