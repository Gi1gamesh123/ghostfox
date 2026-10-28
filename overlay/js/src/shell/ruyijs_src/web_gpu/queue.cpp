// GPUQueue: submit() validates that every element is a GPUCommandBuffer and
// discards the work (there is no GPU), and onSubmittedWorkDone() settles at
// once. Both are honest for a stub device, per deviation D39.
#include "web_gpu/internal.h"

#include "js/Array.h"

namespace ruyijs::web_gpu {

bool QueueMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = static_cast<State*>(WebKit::Owner(args));
  JS::RootedObject self(cx);
  if (!CheckQueue(cx, args.thisv(), &self)) return false;
  switch (WebKit::Slot(args)) {
    case kQueueSubmit: {
      JS::RootedValue buffers(cx, args.get(0));
      bool isArray = false;
      if (!buffers.isObject() || !JS::IsArrayObject(cx, buffers, &isArray) || !isArray)
        return ThrowError(*state, "TypeError", nullptr,
                          "submit requires a sequence of command buffers");
      JS::RootedObject array(cx, &buffers.toObject());
      uint32_t length = 0;
      if (!JS::GetArrayLength(cx, array, &length)) return false;
      for (uint32_t index = 0; index < length; ++index) {
        JS::RootedValue element(cx);
        JS::RootedObject buffer(cx);
        if (!JS_GetElement(cx, array, index, &element) ||
            !CheckCommandBuffer(cx, element, &buffer))
          return false;
      }
      args.rval().setUndefined();
      return true;
    }
    default:  // onSubmittedWorkDone
      return Resolve(*state, JS::UndefinedValue(), args.rval());
  }
}

}  // namespace ruyijs::web_gpu
