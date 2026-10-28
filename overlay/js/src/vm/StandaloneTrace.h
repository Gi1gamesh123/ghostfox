/* ruyi-clone stub: the original's engine-level trace hook (debug mode).
   No-op stand-in; the debug observation layer lands later. */
#ifndef vm_StandaloneTrace_h
#define vm_StandaloneTrace_h

#include "js/CallArgs.h"
#include "js/RootingAPI.h"

namespace js {
namespace standalone_trace {

enum class Category { Ruyijs };

struct CallState {};

inline bool IsEnabled(Category) { return false; }

inline void EnterCall(JSContext*, const JS::CallArgs&, bool, const char*,
                      CallState*) {}

inline void LeaveCall(JSContext*, const JS::CallArgs&, CallState&, bool) {}

inline bool DefineRuyijsObject(JSContext* cx, JS::HandleObject holder) {
  return true;
}

}  // namespace standalone_trace
}  // namespace js

#endif
