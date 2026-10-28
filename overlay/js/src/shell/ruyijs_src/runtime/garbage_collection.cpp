#include "runtime/garbage_collection.h"
#include "js/GCAPI.h"
#include "js/Value.h"

namespace ruyijs {
void CollectGarbage(JSContext* cx, JS::MutableHandleValue result) {
  JS::PrepareForFullGC(cx);
  JS::NonIncrementalGC(cx, JS::GCOptions::Shrink, JS::GCReason::API);
  result.setNumber(JS_GetGCParameter(cx, JSGC_MAJOR_GC_NUMBER));
}
}
