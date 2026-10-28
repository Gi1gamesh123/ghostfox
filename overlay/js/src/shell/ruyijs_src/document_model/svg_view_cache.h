// Weak view identities owned by the original native Node Wrapper: viewBox
// (0 animated, 1 base, 2 anim), points (3 base, 4 animated) and SVG lengths
// keyed by field * 3 + role.
#pragma once
#include "js/GCPolicyAPI.h"
#include "js/RootingAPI.h"
#include "js/SweepingAPI.h"
#include <map>
namespace ruyijs::document_model {
struct SvgViewEntries {
  JS::Heap<JSObject*> views[5];
  std::map<unsigned, JS::Heap<JSObject*>> lengths;
  bool empty() const {
    for (const auto& view : views) if (view) return false;
    return lengths.empty();
  }
  bool traceWeak(JSTracer* tracer) {
    for (auto& view : views) {
      // The tracing policy reports liveness but leaves dead pointer slots intact.
      if (!JS::GCPolicy<JS::Heap<JSObject*>>::traceWeak(tracer, &view))
        view.unbarrieredSet(nullptr);
    }
    for (auto it = lengths.begin(); it != lengths.end();) {
      if (!JS::GCPolicy<JS::Heap<JSObject*>>::traceWeak(tracer, &it->second)) it = lengths.erase(it);
      else ++it;
    }
    return true;
  }
};
struct SvgViewCache {
  JS::WeakCache<SvgViewEntries> entries;
  explicit SvgViewCache(JS::Zone* zone) : entries(zone) {}
};
}
