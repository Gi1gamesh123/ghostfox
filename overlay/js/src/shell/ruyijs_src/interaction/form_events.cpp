#include "interaction/internal.h"
#include "interaction/runtime_event.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"

namespace ruyijs::interaction {
bool Realm::FireClick(JS::HandleObject target, bool* allowed) {
  State& s = *state_;
  JSAutoRealm realm(s.cx, s.page);
  JS::RootedObject init(s.cx, JS_NewPlainObject(s.cx));
  JS::RootedObject prototype(s.cx, s.pointerPrototype);
  if (!init) return false;
  for (const char* field : {"bubbles", "cancelable", "composed"})
    if (!JS_DefineProperty(s.cx, init, field, true, JSPROP_ENUMERATE)) return false;
  return FireRuntimeEvent(s, target, "PointerEvent", "click", Iface::Pointer, Kind::Pointer,
                          init, prototype, -1, false, allowed);
}
bool Realm::FireSubmission(JS::HandleObject target, JS::HandleObject submitter,
                           JS::HandleObject data, bool* allowed) {
  State& s = *state_;
  JSAutoRealm realm(s.cx, s.page);
  JS::RootedObject init(s.cx, JS_NewPlainObject(s.cx)), prototype(s.cx);
  const auto iface = data ? Iface::FormDataPayload : Iface::Submit;
  JS::RootedValue payload(s.cx, JS::ObjectOrNullValue(data ? data : submitter));
  if (!init || !JS_WrapValue(s.cx, &payload) ||
      !ReadInstalledEventPrototype(s, iface, &prototype) ||
      !JS_DefineProperty(s.cx, init, "bubbles", true, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(s.cx, init, "cancelable", !data, JSPROP_ENUMERATE) ||
      !JS_DefineProperty(s.cx, init, data ? "formData" : "submitter", payload, JSPROP_ENUMERATE))
    return false;
  return FireRuntimeEvent(s, target, data ? "FormDataEvent" : "SubmitEvent",
      data ? "formdata" : "submit", iface, data ? Kind::FormDataPayload : Kind::Submit,
      init, prototype, -1, true, allowed);
}
}
