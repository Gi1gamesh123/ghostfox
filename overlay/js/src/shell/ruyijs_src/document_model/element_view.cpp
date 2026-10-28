#include "document_model/state.h"
#include "js/Conversions.h"
#include "js/Exception.h"
#include "js/Object.h"
#include "js/Realm.h"
#include "js/Wrapper.h"
#include "document_model/layout_source.h"
#include "browser_environment/environment.h"
#include <cmath>

namespace ruyijs::document_model {
namespace {
enum Field { ClientHeight, ClientLeft, ClientTop, ClientWidth, ScrollHeight, ScrollWidth,
             ScrollLeft, ScrollTop, Zoom };
constexpr const char* fields[] = {"clientHeight", "clientLeft", "clientTop", "clientWidth",
  "scrollHeight", "scrollWidth", "scrollLeft", "scrollTop", "currentCSSZoom"};
constexpr const char* methods[] = {"scroll", "scrollBy", "scrollTo", "scrollIntoView", "checkVisibility"};
Wrapper* Receiver(JSContext* cx, JS::HandleObject object) {
  if (object && JS::GetClass(object) == &kNodeClass) {
    auto* entry = static_cast<Wrapper*>(JS::GetReservedSlot(object, 0).toPrivate());
    if (entry && entry->owner && !entry->dead) {
      State& owner = *entry->owner;
      JSAutoRealm realm(cx, owner.page);
      NodeSnapshot value;
      if (!owner.Check(owner.model.Inspect(entry->key, &value))) return nullptr;
      if (value.kind == NodeKind::Element) return entry;
    }
  }
  TypeError(cx, "Element receiver is incompatible"); return nullptr;
}
// Firefox Document::IsScrollingElement: the root element in standards mode,
// the body in quirks mode (no parsed doctype, the compatMode/scrollingElement
// rule). Its scroll position is the viewport's; only the realm's window
// document has one. Called inside the owner's page realm.
bool ScrollsViewport(State& owner, NodeKey key, bool* result);
// Moves an element scroll container (not the viewport) to x,y and queues its
// one scroll notification when it moved.
bool ScrollElementTo(JSContext* cx, State& owner, Wrapper* entry, JS::HandleObject element, double x, double y) {
  if (!owner.elementScroll.position) return true;
  bool changed = false;
  if (!owner.elementScroll.position(owner.elementScroll.context, element, true, &x, &y, &changed)) return false;
  if (!changed || !owner.elementScroll.notify || owner.scrollPending.count(entry->key)) return true;
  owner.scrollPending.insert(entry->key);
  return owner.elementScroll.notify(owner.elementScroll.context, element);
}
bool ElementPosition(State& owner, JS::HandleObject element, double* x, double* y) {
  *x = *y = 0;
  bool changed = false;
  return !owner.elementScroll.position ||
         owner.elementScroll.position(owner.elementScroll.context, element, false, x, y, &changed);
}
bool ScrollsViewport(State& owner, NodeKey key, bool* result) {
  *result = false;
  if (Same(owner.main, {})) return true;
  NodeSnapshot node;
  if (!owner.Check(owner.model.Inspect(key, &node))) return false;
  if (!node.is_connected || !Same(node.owner_document, owner.main)) return true;
  DocumentSnapshot document;
  NodeKey type;
  if (!owner.Check(owner.model.InspectDocument(owner.main, &document)) ||
      !owner.Check(owner.model.ParsedDocumentType(owner.main, &type))) return false;
  const bool quirks = !document.xml_parsed && Same(type, {});
  *result = Same(key, quirks ? document.body : document.document_element);
  return true;
}
bool Read(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  const auto field = Field(FunctionSlot(args));
  JS::RootedObject object(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  Wrapper* entry = Receiver(cx, object);
  if (!entry) return false;
  state.Observe("Element", fields[field], int(ApiAccess::Read));
  if (field == ScrollTop || field == ScrollLeft) {
    State& owner = *entry->owner;
    double width = 0, height = 0, x = 0, y = 0;
    bool viewport = false;
    {
      JSAutoRealm realm(cx, owner.page);
      if (!ScrollsViewport(owner, entry->key, &viewport)) return false;
      if (viewport && !owner.environment.ReadLayoutViewport(&width, &height, &x, &y)) return false;
    }
    if (!viewport) {
      JS::RootedObject element(cx, &args.thisv().toObject());
      if (!ElementPosition(owner, element, &x, &y)) return false;
    }
    args.rval().setNumber(field == ScrollTop ? y : x);  // 0 for a non-container
    return true;
  }
  int32_t value = field == Zoom ? 1 : 0;
  if (field == ScrollWidth || field == ScrollHeight) {
    // A rendered element other than the scrolling element reports its
    // layout's scrollable overflow; the scrolling element (viewport) keeps the
    // document path below.
    State& owner = *entry->owner;
    bool viewport = false;
    {
      JSAutoRealm realm(cx, owner.page);
      if (!ScrollsViewport(owner, entry->key, &viewport)) return false;
    }
    OffsetGeometry geometry;
    if (!viewport && owner.offsetGeometry) {
      JS::RootedObject element(cx, &args.thisv().toObject());
      if (!owner.offsetGeometry(owner.offsetGeometryContext, element, &geometry)) return false;
    }
    if (geometry.present && !geometry.root) {
      const double size = field == ScrollWidth ? geometry.scroll_width : geometry.scroll_height;
      args.rval().setInt32(int32_t(std::round(size)));
      return true;
    }
  }
  if ((field == ClientWidth || field == ClientHeight) && entry->owner->offsetGeometry) {
    // Layout-derived (nsIFrame::GetPaddingRect): the root element reports the
    // layout viewport, any other element its padding box.
    State& owner = *entry->owner;
    OffsetGeometry geometry;
    JS::RootedObject element(cx, &args.thisv().toObject());
    if (!owner.offsetGeometry(owner.offsetGeometryContext, element, &geometry)) return false;
    double size = field == ClientWidth ? geometry.client_width : geometry.client_height;
    if (geometry.present && geometry.root) {
      double width = 0, height = 0, x = 0, y = 0;
      JSAutoRealm realm(cx, owner.page);
      if (!owner.environment.ReadLayoutViewport(&width, &height, &x, &y)) return false;
      size = field == ClientWidth ? width : height;
    }
    args.rval().setInt32(int32_t(std::trunc(size)));
    return true;
  }
  if (field == ClientWidth || field == ClientHeight || field == ScrollWidth || field == ScrollHeight) {
    State& owner = *entry->owner;
    JSAutoRealm realm(cx, owner.page);
    if (!owner.Check(owner.model.ElementViewDimension(entry->key,
        field == ClientHeight || field == ScrollHeight, &value))) return false;
  }
  args.rval().setInt32(value); return true;
}
bool Write(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  const int field = FunctionSlot(args);
  JS::RootedObject object(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  Wrapper* entry = Receiver(cx, object);
  if (!entry) return false;
  state.Observe("Element", fields[field], int(ApiAccess::Write));
  double number;
  if (!JS::ToNumber(cx, args.get(0), &number)) return false;
  args.rval().setUndefined();
  State& owner = *entry->owner;
  JSAutoRealm realm(cx, owner.page);
  bool viewport = false;
  if (!ScrollsViewport(owner, entry->key, &viewport)) return false;
  if (!std::isfinite(number)) number = 0;
  if (!viewport) {
    JS::RootedObject element(cx, &args.thisv().toObject());
    double x = 0, y = 0;
    if (!ElementPosition(owner, element, &x, &y)) return false;
    return ScrollElementTo(cx, owner, entry, element, field == ScrollLeft ? number : x,
                           field == ScrollTop ? number : y);
  }
  // Element::ScrollTo: a non-finite value is 0 and the other axis stays put;
  // the window's scroll operation clamps and queues scroll/scrollend.
  double width = 0, height = 0, x = 0, y = 0;
  if (!owner.environment.ReadLayoutViewport(&width, &height, &x, &y)) return false;
  if (!std::isfinite(number)) number = 0;
  return owner.environment.ScrollViewportTo(field == ScrollLeft ? number : x,
                                            field == ScrollTop ? number : y);
}
bool Call(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = *FunctionState(args);
  const int method = FunctionSlot(args);
  JS::RootedObject object(cx, args.thisv().isObject()
      ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr);
  Wrapper* entry = Receiver(cx, object);
  if (!entry) return false;
  state.Observe("Element", methods[method], int(ApiAccess::Call));
  // scroll()/scrollTo() take x, y or ScrollToOptions {left, top};
  // scrollBy() the same as deltas. Behavior is always instant here.
  double x = NAN, y = NAN;
  if (method < 3 && argc == 1 && args[0].isObject()) {
    JS::RootedObject options(cx, &args[0].toObject());
    JS::RootedValue left(cx), top(cx);
    if (!JS_GetProperty(cx, options, "left", &left) || !JS_GetProperty(cx, options, "top", &top)) return false;
    if (!left.isUndefined() && !JS::ToNumber(cx, left, &x)) return false;
    if (!top.isUndefined() && !JS::ToNumber(cx, top, &y)) return false;
  }
  if (method < 3 && argc >= 2) {
    if (!JS::ToNumber(cx, args[0], &x)) {
      // Public conversion of a non-number y clears an ordinary x exception.
      // An already numeric y leaves that exception observable.
      if (!JS_IsExceptionPending(cx) || JS_IsThrowingOutOfMemory(cx)) return false;
      if (args[1].isNumber()) return false;
      JS_ClearPendingException(cx);
    }
    if (!JS::ToNumber(cx, args[1], &y)) return false;
  }
  if (method == 4) { args.rval().setBoolean(true); return true; }
  args.rval().setUndefined();
  if (method >= 3 || (std::isnan(x) && std::isnan(y) && argc < 2)) return true;
  State& owner = *entry->owner;
  JSAutoRealm realm(cx, owner.page);
  bool viewport = false;
  if (!ScrollsViewport(owner, entry->key, &viewport)) return false;
  double width = 0, height = 0, cx0 = 0, cy0 = 0;
  JS::RootedObject element(cx, object);
  if (viewport) {
    if (!owner.environment.ReadLayoutViewport(&width, &height, &cx0, &cy0)) return false;
  } else if (!ElementPosition(owner, element, &cx0, &cy0)) return false;
  const auto axis = [&](double value, double current) {
    if (std::isnan(value)) return current;
    if (!std::isfinite(value)) value = 0;
    return method == 1 ? current + value : value;
  };
  const double nx = axis(x, cx0), ny = axis(y, cy0);
  if (viewport) return owner.environment.ScrollViewportTo(nx, ny);
  return ScrollElementTo(cx, owner, entry, element, nx, ny);
}
}
void Realm::ElementScrollDelivered(JS::HandleObject element) {
  JSObject* object = element ? js::CheckedUnwrapStatic(element) : nullptr;
  if (!object || JS::GetClass(object) != &kNodeClass) return;
  auto* entry = static_cast<Wrapper*>(JS::GetReservedSlot(object, 0).toPrivate());
  if (entry && entry->owner) entry->owner->scrollPending.erase(entry->key);
}
bool InstallElementView(State& state) {
  for (int field = ClientHeight; field <= Zoom; ++field)
    if (!state.Accessor("Element", fields[field], Read, field,
        field == ScrollLeft || field == ScrollTop ? Write : nullptr)) return false;
  for (int method = 0; method < int(std::size(methods)); ++method)
    if (!state.Method("Element", methods[method], Call, 0, method)) return false;
  return true;
}
}
