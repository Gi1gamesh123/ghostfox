#include "web_audio/internal.h"

#include "js/Array.h"
#include "js/Object.h"

// The AudioNode graph: each node owns one outgoing-edge array in its reserved
// slot, so a connection is observable (disconnect rejects a destination that is
// not connected, and duplicate connects collapse to one edge) and cross-context
// connects are rejected at the boundary.
namespace ruyijs::web_audio {
namespace {

using detail::ContextOf;
using detail::Self;

bool Connections(JSContext* cx, JS::HandleObject node, JS::MutableHandleObject array,
                 bool create) {
  JS::Value value = JS::GetReservedSlot(node, kNodeConnectionsSlot);
  if (value.isObject()) {
    array.set(&value.toObject());
    return true;
  }
  if (!create) return true;
  JS::RootedObject created(cx, JS::NewArrayObject(cx, 0));
  if (!created) return false;
  JS::SetReservedSlot(node, kNodeConnectionsSlot, JS::ObjectValue(*created));
  array.set(created);
  return true;
}

bool FindConnection(JSContext* cx, JS::HandleObject array, JSObject* target,
                    uint32_t* length, bool* found) {
  if (!JS::GetArrayLength(cx, array, length)) return false;
  *found = false;
  for (uint32_t index = 0; index < *length; ++index) {
    JS::RootedValue element(cx);
    if (!JS_GetElement(cx, array, index, &element)) return false;
    if (element.isObject() && &element.toObject() == target) {
      *found = true;
      return true;
    }
  }
  return true;
}

}  // namespace

bool NodeConnect(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  // Owner() reads the callee, which asserts after rval() is touched.
  State* state = detail::Self(args);
  JS::RootedObject self(cx);
  if (!detail::NodeReceiver(cx, args, kGenericNode, &self)) return false;
  args.rval().setUndefined();
  if (!args.length()) return true;
  if (!args[0].isObject())
    return WebKit::Fail(cx, "AudioNode.connect requires an AudioNode or AudioParam");
  JS::RootedObject dest(cx, &args[0].toObject());
  const JSClass* destClass = JS::GetClass(dest);
  if (destClass != &kNodeClass && destClass != &kParamClass)
    return WebKit::Fail(cx, "AudioNode.connect requires an AudioNode or AudioParam");
  JSObject* from = ContextOf(self);
  // An AudioParam carries no node context slot, so only nodes get the
  // cross-context identity check.
  JSObject* to = destClass == &kNodeClass ? ContextOf(dest) : nullptr;
  if (from && to && from != to)
    return ThrowDOMException(*state, "InvalidAccessError",
                             "Cannot connect nodes that belong to different AudioContexts");
  JS::RootedObject array(cx);
  if (!Connections(cx, self, &array, true)) return false;
  uint32_t length = 0;
  bool found = false;
  if (!FindConnection(cx, array, dest, &length, &found)) return false;
  if (!found) {
    JS::RootedValue value(cx, JS::ObjectValue(*dest));
    if (!JS_DefineElement(cx, array, length, value, JSPROP_ENUMERATE)) return false;
  }
  // The AudioNode overload returns the destination; the AudioParam overload is
  // void (WebIDL), so the rval stays undefined for it.
  if (destClass == &kNodeClass) args.rval().setObject(*dest);
  return true;
}

bool NodeDisconnect(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State* state = detail::Self(args);
  JS::RootedObject self(cx);
  if (!detail::NodeReceiver(cx, args, kGenericNode, &self)) return false;
  args.rval().setUndefined();
  JS::RootedObject array(cx);
  if (!Connections(cx, self, &array, false)) return false;
  if (!args.length()) return !array || JS::SetArrayLength(cx, array, 0);
  if (!args[0].isObject())
    return WebKit::Fail(cx, "AudioNode.disconnect requires an AudioNode or AudioParam");
  JS::RootedObject dest(cx, &args[0].toObject());
  const JSClass* destClass = JS::GetClass(dest);
  if (destClass != &kNodeClass && destClass != &kParamClass)
    return WebKit::Fail(cx, "AudioNode.disconnect requires an AudioNode or AudioParam");
  if (array) {
    uint32_t length = 0;
    bool found = false;
    if (!FindConnection(cx, array, dest, &length, &found)) return false;
    for (uint32_t index = 0; found && index < length; ++index) {
      JS::RootedValue element(cx);
      if (!JS_GetElement(cx, array, index, &element)) return false;
      if (!element.isObject() || &element.toObject() != dest) continue;
      for (uint32_t next = index + 1; next < length; ++next) {
        JS::RootedValue moved(cx);
        if (!JS_GetElement(cx, array, next, &moved)) return false;
        if (!JS_DefineElement(cx, array, next - 1, moved, JSPROP_ENUMERATE)) return false;
      }
      if (!JS::SetArrayLength(cx, array, length - 1)) return false;
      return true;
    }
  }
  return ThrowDOMException(*state, "InvalidAccessError",
                           "The destination is not connected");
}

}  // namespace ruyijs::web_audio
