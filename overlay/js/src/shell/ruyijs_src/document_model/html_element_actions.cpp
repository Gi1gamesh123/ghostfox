// HTMLElement.style/dataset accessors and click()/focus()/blur() (T4).
// Owner: document_model. Native click uses the original Interaction factory.
#include "document_model/state.h"
#include "document_model/html_element_internal.h"
#include "document_model/style_internal.h"
#include "document_model/form_submission.h"
#include "interaction/realm.h"
#include "js/CallAndConstruct.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "js/ValueArray.h"

namespace ruyijs::document_model {
// [PutForwards=cssText]: assigning HTMLElement.style writes style.cssText.
bool StyleElementSetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Wrapper* entry = ElementReceiver(cx, args.thisv());
  if (!entry) return false;
  JS::RootedObject style(cx);
  {
    State& state = *entry->owner;
    JSAutoRealm realm(cx, state.page);
    JS::RootedObject object(cx, entry->object);
    if (!StyleObject(state, object, &style)) return false;
  }
  if (!JS_WrapObject(cx, &style) || !JS_SetProperty(cx, style, "cssText", args.get(0))) return false;
  args.rval().setUndefined();
  return true;
}
bool StyleElementGetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Wrapper* entry = ElementReceiver(cx, args.thisv());
  if (!entry) return false;
  JS::RootedValue result(cx);
  {
    State& state = *entry->owner;
    JSAutoRealm realm(cx, state.page);
    JS::RootedObject object(cx, entry->object), out(cx);
    if (!StyleObject(state, object, &out)) return false;
    result.setObject(*out);
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result);
  return true;
}
bool DatasetElementGetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Wrapper* entry = ElementReceiver(cx, args.thisv());
  if (!entry) return false;
  JS::RootedValue result(cx);
  {
    State& state = *entry->owner;
    JSAutoRealm realm(cx, state.page);
    JS::RootedObject object(cx, entry->object), out(cx);
    if (!DataObject(state, object, &out)) return false;
    result.setObject(*out);
  }
  if (!JS_WrapValue(cx, &result)) return false;
  args.rval().set(result);
  return true;
}
bool ClickElement(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  Wrapper* entry = HTMLReceiver(cx, args.thisv(), *FunctionState(args), &args);
  if (!entry) return false;
  State& state = *entry->owner;
  JSAutoRealm realm(cx, state.page);
  JS::RootedObject element(cx, entry->object);
  args.rval().setUndefined();
  bool disabled, submit;
  if (!state.Check(state.model.FormControlInfo(entry->key, &disabled, &submit))) return false;
  if (disabled || entry->clickInProgress) return true;
  struct Guard { bool& flag; ~Guard() { flag = false; } } guard{entry->clickInProgress};
  entry->clickInProgress = true;
  bool allowed;
  if (!state.interaction.FireClick(element, &allowed)) return false;
  return !allowed || ActivateFormControl(*entry);
}
bool InertElement(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  if (!HTMLReceiver(cx, args.thisv(), *FunctionState(args), &args)) return false;
  args.rval().setUndefined();
  return true;
}
}  // namespace ruyijs::document_model
