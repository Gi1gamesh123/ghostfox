#include "runtime/native_constructor_trace.h"
#include "js/Exception.h"

namespace ruyijs {
NativeConstructorTrace::NativeConstructorTrace(JSContext* cx, const JS::CallArgs& args)
    : cx_(cx), args_(args) {
  enabled_ = args.isConstructing() &&
      js::standalone_trace::IsEnabled(js::standalone_trace::Category::Ruyijs);
  if (enabled_)
    js::standalone_trace::EnterCall(cx_, args_, true, "native-constructor", &state_);
}
NativeConstructorTrace::~NativeConstructorTrace() {
  if (enabled_)
    js::standalone_trace::LeaveCall(cx_, args_, state_, ok_ && !JS_IsExceptionPending(cx_));
}
}
