#ifndef RUYIJS_RUNTIME_WEAK_OBJECT_H
#define RUYIJS_RUNTIME_WEAK_OBJECT_H
#include "js/HeapAPI.h"

namespace ruyijs {
// Reads of a weak native cache of a JS object (node wrapper, live collection,
// token view, reflected facade, borrowed event target). Finalizers clear the
// cache, but an incremental GC runs the mutator between the sweep slice that
// found an object dead and the slice that finalizes it. In that window the
// address still looks valid; writing a slot into it records a store-buffer edge
// to an arena that is about to be freed, and the next minor GC traces freed
// memory (T41). Gecko's nsWrapperCache has the same two reads.

// GetWrapperPreserveColor: a dying object reads as absent. For comparisons and
// scans that do not hand the object to script.
inline JSObject* WeakPeek(JSObject* object) {
  if (!object || js::gc::EdgeNeedsSweepUnbarriered(&object)) return nullptr;
  return object;
}
// GetWrapper: also exposes a live object, so incremental marking sees the new
// strong reference before script or a slot write holds it.
inline JSObject* WeakLive(JSObject* object) {
  object = WeakPeek(object);
  if (object) JS::ExposeObjectToActiveJS(object);
  return object;
}
}  // namespace ruyijs
#endif
