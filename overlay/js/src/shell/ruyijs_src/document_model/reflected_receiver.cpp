// Brand validation for the existing inline style and dataset facades.
// The creation realm owns their JS surface; the original node owns all data.
#include "document_model/style_internal.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
bool ReflectedReceiver(JSContext* cx, JS::HandleValue value, bool dataset,
                       JS::MutableHandleObject out) {
  JSObject* object = value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr;
  if (!object || !(dataset ? IsDataObject(object) : IsStyleObject(object)))
    return TypeError(cx, "Reflected DOM object receiver is incompatible");
  out.set(object);
  return true;
}
Wrapper* ReflectedElement(JSContext* cx, JS::HandleObject self) {
  JS::RootedValue element(cx, JS::GetReservedSlot(self, 0));
  return ElementReceiver(cx, element);
}
}
