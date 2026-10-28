// Binding state lives on the existing Context. Attachment storage is not yet
// implemented, so user framebuffers are empty and cannot touch the default buffer.
#include "web_graphics/internal.h"
#include "web_graphics/draw_calls.h"

namespace ruyijs::web_graphics {
bool FramebufferCall(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *detail::Self(args);
  const bool bind = (WebKit::Slot(args) & 0xffff) == 1;
  JS::RootedObject self(cx);
  if (!detail::ContextReceiver(cx, args, &self)) return false;
  if (argc < (bind ? 2u : 1u)) return WebKit::Fail(cx, "WebGL method requires more arguments");
  Context& context = *detail::ContextOf(self);
  int32_t target = 0;
  if (!detail::ToEnum(cx, args.get(0), &target)) return false;
  args.rval().setUndefined();
  if (!bind) args.rval().setInt32(0);
  if (target != 0x8D40 && !(context.version == 2 && (target == 0x8CA8 || target == 0x8CA9))) {
    detail::SetError(context, kInvalidEnum);
    return true;
  }
  if (!bind) {
    const bool bound = target == 0x8CA8 ? !!context.readFramebuffer : !!context.drawFramebuffer;
    args.rval().setInt32(bound ? 0x8CD7 : 0x8CD5);
    return true;
  }
  JS::RootedObject object(cx);
  if (!args.get(1).isNullOrUndefined()) {
    object = draw_call_detail::Owned(state, context, args.get(1), kFramebufferObject);
    if (!object) { detail::SetError(context, kInvalidOperation); return true; }
  }
  if (target != 0x8CA8) context.drawFramebuffer = object;
  if (target != 0x8CA9) context.readFramebuffer = object;
  return true;
}
}  // namespace ruyijs::web_graphics
