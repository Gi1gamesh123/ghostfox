// composedPath projects the active rooted snapshot, never a mutable public tree.
#include "interaction/internal.h"
#include "js/PropertyAndElement.h"

namespace ruyijs::interaction {
bool ComposeEventPath(JSContext* cx, Event& event, JS::HandleObject array) {
  if (!event.value.dispatching() || !event.dispatchPath) return true;
  const auto& path = *event.dispatchPath;
  size_t first = 0;
  // In an unslotted shadow path each closed root hides its inner prefix from
  // invocation positions outside that root. Assignment remains a separate
  // original-DocumentModel capability; no synthetic slot edges are invented.
  for (size_t index = 0; index < path.currentPosition && index < path.flags.size(); ++index)
    if (path.flags[index] & 1) first = index + 1;
  JS::RootedValue value(cx);
  for (size_t index = first; index < path.objects.length(); ++index) {
    value.setObject(*path.objects[index]);
    if (!JS_WrapValue(cx, &value) ||
        !JS_DefineElement(cx, array, uint32_t(index - first), value, JSPROP_ENUMERATE)) return false;
  }
  return true;
}
}
