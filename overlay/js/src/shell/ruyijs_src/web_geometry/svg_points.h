#pragma once
#include "web_geometry/internal.h"
#include "document_model/svg_points_types.h"
#include "domain/document_model/svg_point_value.h"
#include "js/GCPolicyAPI.h"
#include "js/SweepingAPI.h"
#include "js/Proxy.h"
#include <map>
namespace ruyijs::web_geometry {
using document_model::SvgPointRef;
struct SvgPointNative { SvgPointRef value; JS::Heap<JSObject*> list; };
struct SvgPointEntries {
  std::map<const domain::document_model::SvgPointValue*, JS::Heap<JSObject*>> points;
  bool empty() const { return points.empty(); }
  bool traceWeak(JSTracer* tracer) {
    for (auto it = points.begin(); it != points.end();) {
      if (!JS::GCPolicy<JS::Heap<JSObject*>>::traceWeak(tracer, &it->second)) it = points.erase(it);
      else ++it;
    }
    return true;
  }
};
struct SvgPointListNative {
  State* state;
  JS::WeakCache<SvgPointEntries> cache;
  SvgPointListNative(State& s, JS::Zone* zone) : state(&s), cache(zone) {}
};
extern const JSClass kSvgPointClass, kSvgPointListClass;
inline SvgPointNative* SvgPointData(JSObject* object) {
  const auto& slot = JS::GetReservedSlot(object, 0);
  return slot.isUndefined() ? nullptr : static_cast<SvgPointNative*>(slot.toPrivate());
}
inline SvgPointListNative* SvgPointListData(JSObject* object) {
  const auto& slot = js::GetProxyReservedSlot(object, 2);
  return slot.isUndefined() ? nullptr : static_cast<SvgPointListNative*>(slot.toPrivate());
}
bool SvgPointReceiver(JSContext*, JS::HandleValue, const char*, JS::MutableHandleObject);
bool SvgPointException(State&, const char* type, const char* member, const char* detail);
bool NewSvgPointObject(State&, const SvgPointRef&, JS::HandleObject list, JS::MutableHandleObject);
bool SvgPointListSnapshot(State&, JS::HandleObject, document_model::SvgPointsSnapshot*);
bool SvgPointListLength(JSContext*, JS::HandleObject, uint32_t*);
bool SvgPointListItem(JSContext*, JS::HandleObject, uint32_t, JS::MutableHandleValue);
bool SvgPointListMutate(State&, JS::HandleObject, unsigned operation, uint32_t,
                        JS::HandleObject input, const char* member, JS::MutableHandleValue);
bool SvgPointIndexedSet(JSContext*, JS::HandleObject, uint32_t, JS::HandleValue);
bool SvgPointListMember(JSContext*, unsigned, JS::Value*);
bool SvgPointMatrixTransform(JSContext*, unsigned, JS::Value*);
bool InstallSvgPointList(State&);
bool InstallSvgPoints(State&);  // SVGPoint, then SVGPointList
bool NewSvgPointList(State&, JS::HandleValue node, unsigned role, JS::MutableHandleObject);
const js::BaseProxyHandler* SvgPointListHandler();
}
