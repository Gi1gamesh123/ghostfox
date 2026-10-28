#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/CallAndConstruct.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include "vm/StandaloneTrace.h"

namespace ruyijs {
PageHelpers::PageHelpers(JSContext* cx, JS::HandleObject page)
    : cx_(cx), page_(cx, page), helpers_(cx) {}
bool PageHelpers::Ensure() {
  if (helpers_) return true;
  JS::RootedObject holder(cx_, JS_NewPlainObject(cx_));
  JS::RootedValue helpers(cx_);
  if (!holder || !js::standalone_trace::DefineRuyijsObject(cx_, holder) ||
      !JS_GetProperty(cx_, holder, "ruyijs", &helpers)) return false;
  helpers_ = &helpers.toObject();
  return true;
}
bool PageHelpers::Call(const char* method, const JS::HandleValueArray& args,
                         JS::MutableHandleValue result) {
  JSAutoRealm realm(cx_, page_);
  return Ensure() && JS_CallFunctionName(cx_, helpers_, method, args, result);
}
bool PageHelpers::Enabled() const {
  return js::standalone_trace::IsEnabled(js::standalone_trace::Category::Ruyijs);
}
}
