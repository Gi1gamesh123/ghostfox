#ifndef RUYIJS_NATIVE_CONSTRUCTOR_TRACE_H
#define RUYIJS_NATIVE_CONSTRUCTOR_TRACE_H
#include "js/CallArgs.h"
#include "vm/StandaloneTrace.h"

namespace ruyijs {
class NativeConstructorTrace final {
 public:
  NativeConstructorTrace(JSContext* cx, const JS::CallArgs& args);
  ~NativeConstructorTrace();
  NativeConstructorTrace(const NativeConstructorTrace&) = delete;
  NativeConstructorTrace& operator=(const NativeConstructorTrace&) = delete;
  bool Finish(bool ok) { ok_ = ok; return ok; }
 private:
  JSContext* cx_;
  JS::CallArgs args_;
  js::standalone_trace::CallState state_;
  bool enabled_ = false;
  bool ok_ = false;
};
}
#endif
