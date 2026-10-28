#include "document_model/xpath.h"
#include "document_model/state.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"

namespace ruyijs::document_model {
// No native lifetime registry or retained DOM/resolver references.
const JSClass kXPathClass = {"XPath", JSCLASS_HAS_RESERVED_SLOTS(2), nullptr};
bool NewXPath(State& state, XPathKind kind, uint16_t type, JS::MutableHandleValue result,
              const JS::CallArgs* args) {
  const char* name = kind == XPathKind::Evaluator ? "XPathEvaluator"
      : kind == XPathKind::Expression ? "XPathExpression" : "XPathResult";
  JS::RootedObject prototype(state.cx, state.Find(name)->prototype);
  if (args) {
    JS::RootedObject target(state.cx, &args->newTarget().toObject());
    JS::RootedValue value(state.cx);
    if (!JS_GetProperty(state.cx, target, "prototype", &value)) return false;
    if (value.isObject()) prototype = &value.toObject();
  }
  JS::RootedObject object(state.cx, JS_NewObjectWithGivenProto(state.cx, &kXPathClass, prototype));
  if (!object) return false;
  JS::SetReservedSlot(object, 0, JS::Int32Value(int32_t(kind)));
  JS::SetReservedSlot(object, 1, JS::Int32Value(type));
  result.setObject(*object);
  return true;
}
bool XPathReceiver(JSContext* cx, JS::HandleValue value, XPathKind kind,
                   JS::MutableHandleObject object) {
  object.set(value.isObject() ? js::CheckedUnwrapStatic(&value.toObject()) : nullptr);
  if (object && JS::GetClass(object) == &kXPathClass &&
      JS::GetReservedSlot(object, 0).toInt32() == int32_t(kind)) return true;
  return TypeError(cx, "XPath receiver is incompatible");
}
}
