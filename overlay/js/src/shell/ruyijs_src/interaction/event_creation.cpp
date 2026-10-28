#include "interaction/internal.h"
#include "interaction/event_creation.h"
#include "interaction/event_keyboard.h"
#include "js/Array.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/Realm.h"
#include <iterator>

namespace ruyijs::interaction {
namespace {
struct LegacyType { std::u16string_view name; Iface iface; Kind kind; };
constexpr LegacyType kTypes[] = {
  {u"event", Iface::Event, Kind::Event},
  {u"customevent", Iface::Custom, Kind::Custom},
  {u"uievent", Iface::UI, Kind::UI},
  {u"mouseevent", Iface::Mouse, Kind::Mouse},
  {u"keyboardevent", Iface::Keyboard, Kind::Keyboard},
  {u"focusevent", Iface::Focus, Kind::Focus},
  {u"messageevent", Iface::Message, Kind::Message},
  {u"storageevent", Iface::Storage, Kind::Storage},
  {u"hashchangeevent", Iface::HashChange, Kind::HashChange},
  {u"dragevent", Iface::Drag, Kind::Drag},
  {u"compositionevent", Iface::Composition, Kind::Composition},
  {u"beforeunloadevent", Iface::BeforeUnload, Kind::BeforeUnload},
  {u"devicemotionevent", Iface::DeviceMotion, Kind::DeviceMotion},
  {u"deviceorientationevent", Iface::DeviceOrientation, Kind::DeviceOrientation},
  {u"textevent", Iface::TextInput, Kind::TextInput},
  {u"touchevent", Iface::Touch, Kind::Touch}
};
bool Equal(std::u16string_view text, std::u16string_view lower) {
  if (text.size() != lower.size()) return false;
  for (size_t i = 0; i < text.size(); ++i) {
    char16_t c = text[i];
    if (c >= u'A' && c <= u'Z') c += u'a' - u'A';
    if (c != lower[i]) return false;
  }
  return true;
}
unsigned Slot(Iface iface) {
  for (unsigned i = 0; i < std::size(kTypes); ++i) if (kTypes[i].iface == iface) return i;
  if (iface == Iface::Submit) return unsigned(std::size(kTypes));
  if (iface == Iface::FormDataPayload) return unsigned(std::size(kTypes) + 1);
  // Host user input dispatches trusted InputEvents (trusted_input.cpp).
  if (iface == Iface::Input) return unsigned(std::size(kTypes) + 2);
  return unsigned(std::size(kTypes) + 3);
}
unsigned Slot(std::u16string_view name) {
  for (unsigned i = 0; i < std::size(kTypes); ++i) if (Equal(name, kTypes[i].name)) return i;
  if (Equal(name, u"events") || Equal(name, u"htmlevents") || Equal(name, u"svgevents")) return Slot(Iface::Event);
  if (Equal(name, u"uievents")) return Slot(Iface::UI);
  if (Equal(name, u"mouseevents")) return Slot(Iface::Mouse);
  return unsigned(std::size(kTypes));
}
bool Unsupported(State& state) {
  return state.exceptionReporter ? state.exceptionReporter(state.exceptionContext,
      "NotSupportedError", "Operation is not supported") :
      Error(state.cx, "TypeError", "Event exception reporter is not installed");
}
bool InitializeLegacyFields(JSContext* cx, const LegacyType& type,
                             JS::HandleObject object, JS::HandleObject view) {
  // Legacy payload defaults differ from constructor dictionaries. They still
  // populate the original traced fields consumed by the existing getters.
  if (type.iface == Iface::DeviceMotion) {
    for (unsigned slot : {kEventMotionAcceleration, kEventMotionGravity,
                         kEventMotionRotation, kEventMotionInterval})
      JS::SetReservedSlot(object, slot, JS::NullValue());
    return true;
  }
  JS::RootedValue options(cx, JS::UndefinedValue());
  if (!InitEventFields(cx, type.iface, options, object)) return false;
  if (type.kind == Kind::Keyboard) SetSlotInt(object, kEventKeyboardFlags, 0);
  if (IsA(type.kind, Iface::UI))
    JS::SetReservedSlot(object, kEventView, JS::ObjectOrNullValue(view));
  if (type.iface == Iface::Message)
    JS::SetReservedSlot(object, kEventMessageData, JS::UndefinedValue());
  return type.iface != Iface::TextInput || SetSlotText(cx, object, kEventPublicFirst, "");
}
}
bool SaveLegacyPrototype(State& state, Iface iface, JS::HandleObject prototype) {
  const unsigned slot = Slot(iface);
  if (slot == std::size(kTypes) + 3) return true;
  if (!state.legacyPrototypes) {
    state.legacyPrototypes = JS::NewArrayObject(state.cx, std::size(kTypes) + 3);
    if (!state.legacyPrototypes) return false;
  }
  JS::RootedObject array(state.cx, state.legacyPrototypes);
  return JS_SetElement(state.cx, array, slot, prototype);
}
bool ReadInstalledEventPrototype(State& state, Iface iface, JS::MutableHandleObject result) {
  JS::RootedObject array(state.cx, state.legacyPrototypes);
  JS::RootedValue value(state.cx);
  if (!array || !JS_GetElement(state.cx, array, Slot(iface), &value) || !value.isObject())
    return Error(state.cx, "TypeError", "Event prototype is not installed");
  result.set(&value.toObject());
  return true;
}
bool Realm::CreateLegacyEvent(std::u16string_view name, JS::HandleObject view,
                             JS::MutableHandleValue result) {
  State& state = *state_;
  JSContext* cx = state.cx;
  JSAutoRealm realm(cx, state.page);
  const unsigned slot = Slot(name);
  if (slot == std::size(kTypes) || !state.legacyPrototypes) return Unsupported(state);
  JS::RootedObject array(cx, state.legacyPrototypes);
  JS::RootedValue value(cx);
  if (!JS_GetElement(cx, array, slot, &value)) return false;
  if (!value.isObject()) return Unsupported(state);
  if (state.events.size() >= 100000) return Error(cx, "RangeError", "Event limit exceeded");
  const auto& type = kTypes[slot];
  // The donor legacy TouchEvent factory is disabled independently of the
  // public TouchEvent constructor, including when touch interfaces are enabled.
  if (type.iface == Iface::Touch) return Unsupported(state);
  JS::RootedObject prototype(cx, &value.toObject());
  JS::RootedObject object(cx, JS_NewObjectWithGivenProto(cx, &kEventClass, prototype));
  JS::RootedString empty(cx, JS_NewStringCopyZ(cx, ""));
  if (!object || !empty) return false;
  JS::SetReservedSlot(object, kEventKind, JS::Int32Value(int32_t(type.kind)));
  if (!InstallTrusted(state, object) || !InitializeLegacyFields(cx, type, object, view)) return false;
  auto event = std::make_unique<Event>(cx, object, empty, std::string{}, false, false);
  event->kind = type.kind;
  // Firefox's legacy native FocusEvent factory starts composed; initEvent
  // keeps that bit. Its public constructor retains its existing defaults.
  event->value.set_composed(type.iface == Iface::Focus);
  event->value.mark_uninitialized();
  event->value.set_time_stamp(std::chrono::duration<double, std::milli>(
      std::chrono::steady_clock::now() - state.started).count());
  state.events.push_back(std::move(event));
  result.setObject(*object);
  return true;
}
}
