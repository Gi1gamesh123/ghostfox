// The WebGL object interfaces and the members that create, delete and query
// them. Every created object carries its owner context and kind in reserved
// slots, so the context recognises its own objects without a second registry.
#include "web_graphics/internal.h"

#include "js/Realm.h"
#include "runtime/native_constructor_trace.h"
#include "runtime/page_helpers.h"

namespace ruyijs::web_graphics {
using detail::kObjectContextSlot;
using detail::kObjectDeletedSlot;
using detail::kObjectKindSlot;
using detail::kObjectSlotCount;
const JSClass kBufferClass = {"WebGLBuffer", JSCLASS_HAS_RESERVED_SLOTS(kObjectSlotCount)};
const JSClass kTextureClass = {"WebGLTexture", JSCLASS_HAS_RESERVED_SLOTS(kObjectSlotCount)};
const JSClass kFramebufferClass = {"WebGLFramebuffer", JSCLASS_HAS_RESERVED_SLOTS(kObjectSlotCount)};
const JSClass kRenderbufferClass = {"WebGLRenderbuffer", JSCLASS_HAS_RESERVED_SLOTS(kObjectSlotCount)};
const JSClass kProgramClass = {"WebGLProgram", JSCLASS_HAS_RESERVED_SLOTS(kObjectSlotCount)};
const JSClass kShaderClass = {"WebGLShader", JSCLASS_HAS_RESERVED_SLOTS(kObjectSlotCount)};
const JSClass kUniformLocationClass = {"WebGLUniformLocation", JSCLASS_HAS_RESERVED_SLOTS(kObjectSlotCount)};

namespace {
const JSClass* const kClasses[] = {&kBufferClass, &kTextureClass, &kFramebufferClass,
    &kRenderbufferClass, &kProgramClass, &kShaderClass, &kUniformLocationClass};
const char* const kNames[] = {"WebGLBuffer", "WebGLTexture", "WebGLFramebuffer",
    "WebGLRenderbuffer", "WebGLProgram", "WebGLShader", "WebGLUniformLocation",
    "WebGLQuery", "WebGLSampler", "WebGLSync", "WebGLTransformFeedback", "WebGLVertexArrayObject"};
const JSClass kQueryClass = {"WebGLQuery", JSCLASS_HAS_RESERVED_SLOTS(kObjectSlotCount)};
const JSClass kSamplerClass = {"WebGLSampler", JSCLASS_HAS_RESERVED_SLOTS(kObjectSlotCount)};
const JSClass kSyncClass = {"WebGLSync", JSCLASS_HAS_RESERVED_SLOTS(kObjectSlotCount)};
const JSClass kFeedbackClass = {"WebGLTransformFeedback", JSCLASS_HAS_RESERVED_SLOTS(kObjectSlotCount)};
const JSClass kVertexArrayClass = {"WebGLVertexArrayObject", JSCLASS_HAS_RESERVED_SLOTS(kObjectSlotCount)};

bool ObjectCtor(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  NativeConstructorTrace trace(cx, args);
  // Every WebGL object interface has an illegal constructor in the spec.
  return trace.Finish(WebKit::Fail(cx, "Illegal constructor."));
}

bool Receiver(JSContext* cx, const JS::CallArgs& args, State** state, Context** context) {
  *state = detail::Self(args);
  JS::RootedObject self(cx);
  if (!detail::ContextReceiver(cx, args, &self)) return false;
  *context = detail::ContextOf(self);
  return true;
}

bool Create(JSContext* cx, unsigned argc, JS::Value* vp, int32_t kind) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = nullptr;
  Context* context = nullptr;
  if (!Receiver(cx, args, &state, &context)) return false;
  JS::RootedObject object(cx);
  if (!NewObject(*state, kind, *context, &object)) return false;
  args.rval().setObject(*object);
  return true;
}

bool Delete(JSContext* cx, unsigned argc, JS::Value* vp, int32_t kind) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = nullptr;
  Context* context = nullptr;
  if (!Receiver(cx, args, &state, &context)) return false;
  args.rval().setUndefined();
  JS::HandleValue value = args.get(0);
  if (value.isUndefined() || value.isNull()) return true;
  if (!value.isObject() || JS::GetClass(&value.toObject()) != state->Face(kind).clasp ||
      detail::ObjectContext(&value.toObject()) != context) {
    detail::SetError(*context, kInvalidOperation);
    return true;
  }
  JS::SetReservedSlot(&value.toObject(), kObjectDeletedSlot, JS::BooleanValue(true));
  if (context->arrayBinding == &value.toObject()) context->arrayBinding = nullptr;
  if (context->elementBinding == &value.toObject()) context->elementBinding = nullptr;
  if (context->drawFramebuffer == &value.toObject()) context->drawFramebuffer = nullptr;
  if (context->readFramebuffer == &value.toObject()) context->readFramebuffer = nullptr;
  return true;
}

bool Is(JSContext* cx, unsigned argc, JS::Value* vp, int32_t kind) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = nullptr;
  Context* context = nullptr;
  if (!Receiver(cx, args, &state, &context)) return false;
  JS::HandleValue value = args.get(0);
  bool result = false;
  if (value.isObject()) {
    JSObject* object = &value.toObject();
    result = JS::GetClass(object) == state->Face(kind).clasp && detail::ObjectContext(object) == context &&
             !JS::GetReservedSlot(object, kObjectDeletedSlot).toBoolean();
  }
  args.rval().setBoolean(result);
  return true;
}
}  // namespace

JSObject* NewObject(State& state, int32_t kind, Context& context, JS::MutableHandleObject out) {
  JSContext* cx = state.cx;
  WebFace& face = state.Face(kind);
  JS::RootedObject object(cx, state.kit.New(face.clasp, face.prototype));
  if (!object) return nullptr;
  JS::SetReservedSlot(object, kObjectContextSlot, JS::PrivateValue(&context));
  JS::SetReservedSlot(object, kObjectKindSlot, JS::Int32Value(kind));
  JS::SetReservedSlot(object, kObjectDeletedSlot, JS::BooleanValue(false));
  // The per-kind payload slots start empty; the setters below fill what they own.
  for (int32_t slot = detail::kObjectDataSlot; slot < detail::kObjectSlotCount; ++slot)
    JS::SetReservedSlot(object, slot, JS::UndefinedValue());
  if (!state.kit.helpers->Object(kNames[kind], object)) return nullptr;
  out.set(object);
  return object;
}

bool BindBuffer(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = nullptr;
  Context* context = nullptr;
  if (!Receiver(cx, args, &state, &context)) return false;
  args.rval().setUndefined();
  int32_t target = 0;
  if (!detail::ToEnum(cx, args.get(0), &target)) return false;
  if (target != kArrayBuffer && target != kElementArrayBuffer) {
    detail::SetError(*context, kInvalidEnum);
    return true;
  }
  JS::PersistentRootedObject* binding =
      target == kArrayBuffer ? &context->arrayBinding : &context->elementBinding;
  JS::HandleValue value = args.get(1);
  if (value.isUndefined() || value.isNull()) {
    *binding = nullptr;
    return true;
  }
  if (!value.isObject() || JS::GetClass(&value.toObject()) != &kBufferClass ||
      detail::ObjectContext(&value.toObject()) != context ||
      JS::GetReservedSlot(&value.toObject(), kObjectDeletedSlot).toBoolean()) {
    detail::SetError(*context, kInvalidOperation);
    return true;
  }
  *binding = &value.toObject();
  return true;
}

bool GetUniformLocation(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = nullptr;
  Context* context = nullptr;
  if (!Receiver(cx, args, &state, &context)) return false;
  JS::HandleValue program = args.get(0);
  if (!program.isObject() || JS::GetClass(&program.toObject()) != &kProgramClass ||
      detail::ObjectContext(&program.toObject()) != context || detail::ObjectDeleted(&program.toObject())) {
    detail::SetError(*context, kInvalidOperation);
    args.rval().setNull();
    return true;
  }
  if (args.length() < 2) {
    args.rval().setNull();
    return true;
  }
  JS::RootedString text(cx, JS::ToString(cx, args.get(1)));
  if (!text) return false;
  JS::RootedObject object(cx);
  if (!NewObject(*state, kUniformLocationObject, *context, &object)) return false;
  // A location is a handle on the uniform's name: the value lives in the context.
  JS::SetReservedSlot(object, detail::kObjectDataSlot, JS::StringValue(text));
  args.rval().setObject(*object);
  return true;
}

bool CreateBuffer(JSContext* cx, unsigned argc, JS::Value* vp) { return Create(cx, argc, vp, kBufferObject); }
bool CreateTexture(JSContext* cx, unsigned argc, JS::Value* vp) { return Create(cx, argc, vp, kTextureObject); }
bool CreateFramebuffer(JSContext* cx, unsigned argc, JS::Value* vp) { return Create(cx, argc, vp, kFramebufferObject); }
bool CreateRenderbuffer(JSContext* cx, unsigned argc, JS::Value* vp) { return Create(cx, argc, vp, kRenderbufferObject); }
bool CreateProgram(JSContext* cx, unsigned argc, JS::Value* vp) { return Create(cx, argc, vp, kProgramObject); }
bool CreateShader(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = nullptr;
  Context* context = nullptr;
  if (!Receiver(cx, args, &state, &context)) return false;
  int32_t type = 0;
  if (!detail::ToEnum(cx, args.get(0), &type) || (type != kVertexShader && type != kFragmentShader)) {
    detail::SetError(*context, kInvalidEnum);
    args.rval().setNull();
    return true;
  }
  JS::RootedObject object(cx);
  if (!NewObject(*state, kShaderObject, *context, &object)) return false;
  // The stage a shader belongs to is fixed at creation and cannot change.
  JS::SetReservedSlot(object, detail::kObjectExtraSlot, JS::Int32Value(type));
  args.rval().setObject(*object);
  return true;
}
bool DeleteBuffer(JSContext* cx, unsigned argc, JS::Value* vp) { return Delete(cx, argc, vp, kBufferObject); }
bool DeleteTexture(JSContext* cx, unsigned argc, JS::Value* vp) { return Delete(cx, argc, vp, kTextureObject); }
bool DeleteFramebuffer(JSContext* cx, unsigned argc, JS::Value* vp) { return Delete(cx, argc, vp, kFramebufferObject); }
bool DeleteRenderbuffer(JSContext* cx, unsigned argc, JS::Value* vp) { return Delete(cx, argc, vp, kRenderbufferObject); }
bool DeleteProgram(JSContext* cx, unsigned argc, JS::Value* vp) { return Delete(cx, argc, vp, kProgramObject); }
bool DeleteShader(JSContext* cx, unsigned argc, JS::Value* vp) { return Delete(cx, argc, vp, kShaderObject); }
bool IsBuffer(JSContext* cx, unsigned argc, JS::Value* vp) { return Is(cx, argc, vp, kBufferObject); }
bool IsTexture(JSContext* cx, unsigned argc, JS::Value* vp) { return Is(cx, argc, vp, kTextureObject); }
bool IsFramebuffer(JSContext* cx, unsigned argc, JS::Value* vp) { return Is(cx, argc, vp, kFramebufferObject); }
bool IsRenderbuffer(JSContext* cx, unsigned argc, JS::Value* vp) { return Is(cx, argc, vp, kRenderbufferObject); }
bool IsProgram(JSContext* cx, unsigned argc, JS::Value* vp) { return Is(cx, argc, vp, kProgramObject); }
bool IsShader(JSContext* cx, unsigned argc, JS::Value* vp) { return Is(cx, argc, vp, kShaderObject); }

bool ExtraObjectCall(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  int op = WebKit::Slot(args) & 0xffff, kind = op / 3;
  switch (op % 3) {
    case 0: return Create(cx, argc, vp, kind);
    case 1: return Delete(cx, argc, vp, kind);
    default: return Is(cx, argc, vp, kind);
  }
}

bool InstallObjects(State& state) {
  WebKit& kit = state.kit;
  JSContext* cx = state.cx;
  JS::RootedObject objectProto(cx, JS::GetRealmObjectPrototype(cx));
  if (!objectProto) return false;
  const JSClass* extra[] = {&kQueryClass, &kSamplerClass, &kSyncClass, &kFeedbackClass, &kVertexArrayClass};
  for (int32_t kind = 0; kind < kObjectKindCount; ++kind)
    if (!kit.Define(kNames[kind], kind < 7 ? kClasses[kind] : extra[kind-7], objectProto, ObjectCtor,
                    &state.Face(kind))) return false;
  return true;
}
}  // namespace ruyijs::web_graphics
