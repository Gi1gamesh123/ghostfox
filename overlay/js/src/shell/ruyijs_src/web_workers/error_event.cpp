#include "web_workers/error_event.h"
#include "interaction/internal.h"
#include "interaction/runtime_event.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Wrapper.h"

namespace ruyijs::web_workers {
JSObject* ErrorPrototype(JSContext* cx, JS::HandleObject global) {
  JS::RootedValue constructor(cx), prototype(cx);
  if (!JS_GetProperty(cx, global, "ErrorEvent", &constructor) || !constructor.isObject()) return nullptr;
  JS::RootedObject object(cx, &constructor.toObject());
  if (!JS_GetProperty(cx, object, "prototype", &prototype) || !prototype.isObject()) return nullptr;
  return &prototype.toObject();
}
bool FireError(JSContext* cx, JS::HandleObject target, JS::HandleObject init,
               JS::HandleObject prototype, bool* allowed) {
  auto* state = interaction::Owner(cx, target, false);
  if (!state) return false;
  return interaction::FireRuntimeEvent(*state, target, "ErrorEvent", "error", interaction::Iface::Error,
          interaction::Kind::Error, init, prototype, -1, true, allowed);
}
bool ErrorArguments(JSContext* cx, JS::HandleValue value, JS::RootedValueArray<5>& arguments) {
  if (!value.isObject()) return false;
  JS::RootedObject object(cx, js::CheckedUnwrapStatic(&value.toObject()));
  auto* state = object ? interaction::Owner(cx, object, true) : nullptr;
  if (!state || state->event(object)->kind != interaction::Kind::Error) return false;
  const unsigned slots[] = {interaction::kEventErrorMessage, interaction::kEventErrorFilename,
      interaction::kEventLineno, interaction::kEventColno, interaction::kEventErrorValue};
  for (unsigned index = 0; index < 5; ++index) arguments[index].set(JS::GetReservedSlot(object, slots[index]));
  return true;
}
}
