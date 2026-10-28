// The ElementInternals members in Firefox's prototype order: form-associated
// operations and attributes, states, and the ARIA reflection attributes (kept
// in the internals' own Map; they do not touch content attributes).
#include "document_model/element_internals.h"
#include "interaction/realm.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/ForOfIterator.h"
#include "js/MapAndSet.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include <cstring>
#include <iterator>

namespace ruyijs::document_model {
namespace {
enum Kind { Method, FormGet, Get, AriaString, AriaElement, AriaElements };
struct Member { const char* name; Kind kind; unsigned length; };
const Member kMembers[] = {
  {"setFormValue", Method, 1}, {"setValidity", Method, 0}, {"checkValidity", Method, 0},
  {"reportValidity", Method, 0}, {"shadowRoot", Get, 0}, {"form", FormGet, 0}, {"willValidate", FormGet, 0},
  {"validity", FormGet, 0}, {"validationMessage", FormGet, 0}, {"labels", FormGet, 0}, {"states", Get, 0},
  {"ariaActiveDescendantElement", AriaElement, 0}, {"role", AriaString, 0}, {"ariaAtomic", AriaString, 0},
  {"ariaAutoComplete", AriaString, 0}, {"ariaBrailleLabel", AriaString, 0},
  {"ariaBrailleRoleDescription", AriaString, 0}, {"ariaBusy", AriaString, 0}, {"ariaChecked", AriaString, 0},
  {"ariaColCount", AriaString, 0}, {"ariaColIndex", AriaString, 0}, {"ariaColIndexText", AriaString, 0},
  {"ariaColSpan", AriaString, 0}, {"ariaControlsElements", AriaElements, 0}, {"ariaCurrent", AriaString, 0},
  {"ariaDescribedByElements", AriaElements, 0}, {"ariaDescription", AriaString, 0},
  {"ariaDetailsElements", AriaElements, 0}, {"ariaDisabled", AriaString, 0},
  {"ariaErrorMessageElements", AriaElements, 0}, {"ariaExpanded", AriaString, 0},
  {"ariaFlowToElements", AriaElements, 0}, {"ariaHasPopup", AriaString, 0}, {"ariaHidden", AriaString, 0},
  {"ariaInvalid", AriaString, 0}, {"ariaKeyShortcuts", AriaString, 0}, {"ariaLabel", AriaString, 0},
  {"ariaLabelledByElements", AriaElements, 0}, {"ariaLevel", AriaString, 0}, {"ariaLive", AriaString, 0},
  {"ariaModal", AriaString, 0}, {"ariaMultiLine", AriaString, 0}, {"ariaMultiSelectable", AriaString, 0},
  {"ariaOrientation", AriaString, 0}, {"ariaOwnsElements", AriaElements, 0}, {"ariaPlaceholder", AriaString, 0},
  {"ariaPosInSet", AriaString, 0}, {"ariaPressed", AriaString, 0}, {"ariaReadOnly", AriaString, 0},
  {"ariaRelevant", AriaString, 0}, {"ariaRequired", AriaString, 0}, {"ariaRoleDescription", AriaString, 0},
  {"ariaRowCount", AriaString, 0}, {"ariaRowIndex", AriaString, 0}, {"ariaRowIndexText", AriaString, 0},
  {"ariaRowSpan", AriaString, 0}, {"ariaSelected", AriaString, 0}, {"ariaSetSize", AriaString, 0},
  {"ariaSort", AriaString, 0}, {"ariaValueMax", AriaString, 0}, {"ariaValueMin", AriaString, 0},
  {"ariaValueNow", AriaString, 0}, {"ariaValueText", AriaString, 0}};
bool FormAssociated(State& s, JSObject* self, const Member& m) {
  CustomElementDefinition* definition = InternalsDefinition(s, self);
  if (definition && definition->formAssociated) return true;
  const std::string where = m.kind == Method ? std::string(m.name) : std::string(m.name) + " getter";
  return InternalsFail(s, "ElementInternals." + where + ": Target element is not a form-associated custom element");
}
bool Valid(JS::HandleObject self) { return JS::GetReservedSlot(self, kFlags).toInt32() == 0; }
bool Disabled(State& s, JS::HandleObject self) {
  Wrapper* entry = s.Find(&JS::GetReservedSlot(self, kElement).toObject());
  std::optional<DomString> disabled;
  return entry && s.model.GetAttribute(entry->key, u"disabled", &disabled).code == ErrorCode::None && disabled;
}
bool Call(State& s, JS::HandleObject self, int index, const JS::CallArgs& args) {
  JSContext* cx = s.cx;
  if (index == 1) return InternalsSetValidity(s, self, args);
  if (index == 0 && !args.length())
    return TypeError(cx, "ElementInternals.setFormValue: At least 1 argument required, but only 0 passed");
  if (!FormAssociated(s, self, kMembers[index])) return false;
  if (index == 0) {
    JS::SetReservedSlot(self, kValue, args[0]);
    JS::SetReservedSlot(self, kFormState, args.get(1).isUndefined() ? args[0] : args[1]);
    args.rval().setUndefined();
    return true;
  }
  const bool valid = Disabled(s, self) || Valid(self);
  if (!valid) {  // check/reportValidity fire a cancelable "invalid" at the element
    JS::RootedObject element(cx, &JS::GetReservedSlot(self, kElement).toObject());
    if (!s.interaction.Fire(element, "invalid", false, true)) return false;
  }
  args.rval().setBoolean(valid);
  return true;
}
bool Read(State& s, JS::HandleObject self, const Member& m, JS::MutableHandleValue rval) {
  JSContext* cx = s.cx;
  if (m.kind == FormGet && !FormAssociated(s, self, m)) return false;
  const std::string name = m.name;
  if (name == "shadowRoot" || name == "form") { rval.setNull(); return true; }
  if (name == "willValidate") { rval.setBoolean(!Disabled(s, self)); return true; }
  if (name == "validity") { rval.set(JS::GetReservedSlot(self, kValidity)); return true; }
  if (name == "validationMessage") { rval.set(JS::GetReservedSlot(self, kMessage)); return true; }
  if (name == "labels") { rval.set(JS::GetReservedSlot(self, kLabels)); return true; }
  if (name == "states") { rval.set(JS::GetReservedSlot(self, kStates)); return true; }
  JS::RootedObject map(cx, &JS::GetReservedSlot(self, kAria).toObject());
  JS::RootedValue key(cx);
  if (!Value(cx, DomStringView(DomString(name.begin(), name.end())), &key) || !JS::MapGet(cx, map, key, rval))
    return false;
  if (rval.isUndefined()) rval.setNull();
  return JS_WrapValue(cx, rval);
}
bool Write(State& s, JS::HandleObject self, const Member& m, JS::HandleValue v) {
  JSContext* cx = s.cx;
  const std::string prefix = std::string("ElementInternals.") + m.name + " setter: ";
  JS::RootedValue stored(cx, JS::NullValue());
  if (m.kind == AriaString && !v.isNullOrUndefined()) {
    JS::RootedString text(cx, JS::ToString(cx, v));
    if (!text) return false;
    stored.setString(text);
  } else if (m.kind == AriaElement && !v.isNullOrUndefined()) {
    if (!InternalsIsElement(s, v))
      return TypeError(cx, (prefix + "Value being assigned does not implement interface Element.").c_str());
    stored.set(v);
  } else if (m.kind == AriaElements && !v.isNullOrUndefined()) {
    JS::RootedObject array(cx, JS::NewArrayObject(cx, 0));
    JS::ForOfIterator it(cx);
    JS::RootedValue item(cx);
    if (!array || !v.isObject() || !it.init(v))
      return array && !JS_IsExceptionPending(cx)
          ? TypeError(cx, (prefix + "Value being assigned can't be converted to a sequence.").c_str()) : false;
    for (uint32_t i = 0;; ++i) {
      bool done = false;
      if (!it.next(&item, &done)) return false;
      if (done) break;
      if (!InternalsIsElement(s, item))
        return TypeError(cx, (prefix + "Element of value being assigned does not implement interface Element.").c_str());
      if (!JS_DefineElement(cx, array, i, item, JSPROP_ENUMERATE)) return false;
    }
    if (!JS_FreezeObject(cx, array)) return false;  // FrozenArray: the same array until the next set
    stored.setObject(*array);
  }
  JS::RootedObject map(cx, &JS::GetReservedSlot(self, kAria).toObject());
  JS::RootedValue key(cx);
  return Value(cx, DomStringView(DomString(m.name, m.name + std::strlen(m.name))), &key) &&
         JS::MapSet(cx, map, key, stored);
}
bool Dispatch(JSContext* cx, unsigned argc, JS::Value* vp, bool setter) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *FunctionState(args);
  const int index = FunctionSlot(args);
  const Member& m = kMembers[index];
  JS::RootedObject self(cx, InternalsSelf(args.thisv()));
  if (!self) {
    const std::string name = m.kind == Method ? m.name : std::string(setter ? "set " : "get ") + m.name;
    return TypeError(cx, ("'" + name + "' called on an object that does not implement interface ElementInternals.").c_str());
  }
  if (m.kind == Method) return Call(s, self, index, args);
  if (!setter) return Read(s, self, m, args.rval());
  args.rval().setUndefined();
  return Write(s, self, m, args.get(0));
}
bool Getter(JSContext* cx, unsigned argc, JS::Value* vp) { return Dispatch(cx, argc, vp, false); }
bool Setter(JSContext* cx, unsigned argc, JS::Value* vp) { return Dispatch(cx, argc, vp, true); }
}  // namespace

bool InstallElementInternals(State& state) {
  JSContext* cx = state.cx;
  JS::RootedObject plain(cx, JS::GetRealmObjectPrototype(cx));
  if (!state.DefineInterface("ElementInternals", plain, Illegal, false)) return false;
  for (int i = 0; i < int(std::size(kMembers)); ++i) {
    const Member& m = kMembers[i];
    if (m.kind == Method) {
      if (!state.Method("ElementInternals", m.name, Getter, m.length, i)) return false;
    } else if (!state.Accessor("ElementInternals", m.name, Getter, i,
                               m.kind >= AriaString ? Setter : nullptr)) {
      return false;
    }
  }
  return state.Method("HTMLElement", "attachInternals", AttachInternals, 0, 0);
}
}  // namespace ruyijs::document_model
