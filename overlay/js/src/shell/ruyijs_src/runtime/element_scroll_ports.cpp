// Element scrolling (docs/owners/WEB_GEOMETRY.md, element scroll): Document
// Model's Element.scroll* members read and move the position the Geometry
// owner keeps, and a moved container gets one trusted, non-bubbling `scroll`
// then `scrollend` UIEvent from the next task, through the Interaction owner.
#include "runtime/element_scroll_ports.h"
#include "runtime/browsing_context.h"
#include "execution_scheduling/realm.h"
#include "jsfriendapi.h"
#include "js/Realm.h"
#include "jsapi.h"

namespace ruyijs {
namespace {
bool Notify(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  // rval() aliases the callee slot: read the callee's reserved slots first.
  JS::RootedObject callee(cx, &args.callee());
  JS::RootedValue element(cx, js::GetFunctionNativeReserved(callee, 1));
  auto* owner = static_cast<BrowsingContext*>(js::GetFunctionNativeReserved(callee, 0).toPrivate());
  args.rval().setUndefined();
  if (!owner || !owner->document || !owner->interaction || !element.isObject()) return true;
  JS::RootedObject target(cx, &element.toObject());
  owner->document->ElementScrollDelivered(target);
  return owner->interaction->FireScroll(target, "scroll", false) &&
         owner->interaction->FireScroll(target, "scrollend", false);
}
}  // namespace

void BindElementScroll(BrowsingContext& context) {
  document_model::ElementScrollPort port;
  port.position = [](void* raw, JS::HandleObject element, bool write, double* x, double* y, bool* changed) {
    return static_cast<BrowsingContext*>(raw)->geometry->ElementScroll(element, write, x, y, changed);
  };
  port.notify = [](void* raw, JS::HandleObject element) {
    auto& owner = *static_cast<BrowsingContext*>(raw);
    if (!owner.scheduling) return true;  // No event loop yet: the position still moved.
    JSContext* cx = owner.cx;
    // Created in the element's realm: a retired document drops the task.
    JSAutoRealm realm(cx, element);
    JSFunction* task = js::NewFunctionWithReserved(cx, Notify, 0, 0, "scroll");
    if (!task) return false;
    JS::RootedObject object(cx, JS_GetFunctionObject(task));
    js::SetFunctionNativeReserved(object, 0, JS::PrivateValue(&owner));
    js::SetFunctionNativeReserved(object, 1, JS::ObjectValue(*element));
    return owner.scheduling->PostTask(object);
  };
  port.context = &context;
  context.document->SetElementScrollPort(port);
}
void UnbindElementScroll(BrowsingContext& context) {
  if (context.document) context.document->SetElementScrollPort({});
}
}  // namespace ruyijs
