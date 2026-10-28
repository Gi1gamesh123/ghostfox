#include "web_geometry/rect_list.h"
#include "web_geometry/box_input.h"
#include "js/Realm.h"
#include <algorithm>

namespace ruyijs::web_geometry {
namespace {
// Firefox nsRange::GetClientRects/GetBoundingClientRect over the elements the
// range contains at top level: each element's border box (Element geometry);
// the bounding rectangle unions their edges, or is zero without any.
bool RangeProjection(State& s, JS::HandleValue range, bool list, JS::MutableHandleValue result) {
  JSContext* cx = s.cx;
  JS::RootedObjectVector elements(cx);
  if (!s.rangeContents(s.rangeContext, range, &elements)) return false;
  std::vector<std::array<double,4>> rects;
  for (JSObject* raw : elements) {
    JS::RootedValue element(cx, JS::ObjectValue(*raw));
    if (!JS_WrapValue(cx, &element)) return false;
    document_model::LayoutNodeSource node;
    JS::RootedObject parent(cx);
    JS::RootedObjectVector children(cx);
    if (!s.boxPorts.node || !s.boxPorts.node(s.boxPorts.context, element, &node, &parent, &children)) return false;
    auto* owner = static_cast<Realm*>(node.layoutOwner);
    if (!owner) continue;
    JS::RootedObject target(cx, &element.toObject());
    std::array<double,4> rect{}; bool present = false;
    if (!owner->MeasureElement(target, &rect, &present)) return false;
    if (present) rects.push_back(rect);
  }
  JSAutoRealm realm(cx, s.kit.page);
  if (list) {
    if (!NewRectList(s, rects, result)) return false;
  } else {
    double box[4] = {};
    if (!rects.empty()) {
      double left = rects[0][0], top = rects[0][1], right = left + rects[0][2], bottom = top + rects[0][3];
      for (const auto& r : rects) {
        left = std::min(left, r[0]); top = std::min(top, r[1]);
        right = std::max(right, r[0] + r[2]); bottom = std::max(bottom, r[1] + r[3]);
      }
      box[0] = left; box[1] = top; box[2] = right - left; box[3] = bottom - top;
    }
    if (!NewValue(s, Rect, box, nullptr, result)) return false;
  }
  return JS_WrapValue(cx, result);
}
bool Project(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = Self(args);
  unsigned kind = WebKit::Slot(args);
  bool list = kind % 2 != 0, element = kind >= 2;
  if (!element) {
    if (!s.rangeContents) return WebKit::Fail(cx, "Geometry receiver port is not installed");
    return RangeProjection(s, args.thisv(), list, args.rval());
  }
  if (!s.elementCheck) return WebKit::Fail(cx, "Geometry receiver port is not installed");
  if (!s.elementCheck(s.elementContext, args.thisv())) {
    // The port set its own brand error; replace it with Firefox's exact
    // per-member wording ("'getBoundingClientRect' called on an object that
    // does not implement interface Element."), which Cloudflare fingerprints.
    JS_ClearPendingException(cx);
    return WebKit::FailBrand(cx, args, "Element");
  }
  return ElementProjection(s,args.thisv(),list,args.rval());
}
bool InstallProjection(State& s, JS::HandleObject prototype, bool element) {
  JSAutoRealm realm(s.cx, s.kit.page);
  WebFace borrowed(s.cx);
  borrowed.prototype = prototype;
  const char* name = element ? "Element" : "Range";
  unsigned first = element ? 2 : 0;
  return s.kit.Method(borrowed, name, "getBoundingClientRect", Project, 0, first) &&
         s.kit.Method(borrowed, name, "getClientRects", Project, 0, first + 1);
}
}
bool Realm::InstallRangeProjection(JS::HandleObject prototype,
    bool (*contents)(void*, JS::HandleValue, JS::MutableHandleObjectVector), void* context) {
  state_->rangeContents = contents; state_->rangeContext = context;
  return InstallProjection(*state_, prototype, false);
}
bool Realm::InstallElementProjection(JS::HandleObject prototype,
    bool (*check)(void*, JS::HandleValue), void* context) {
  state_->elementCheck = check; state_->elementContext = context;
  return InstallProjection(*state_, prototype, true);
}
void Realm::ClearProjections() {
  state_->svgPorts = {};
  state_->boxPorts = {};
  state_->rangeContents = nullptr; state_->rangeContext = nullptr;
  state_->elementCheck = nullptr; state_->elementContext = nullptr;
}
}
