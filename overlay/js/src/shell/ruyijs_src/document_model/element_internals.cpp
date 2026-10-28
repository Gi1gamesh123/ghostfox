// HTMLElement.attachInternals and ElementInternals (HTML §4.13.7,
// docs/owners/DOCUMENT_MODEL.md 2026-10-21). An internals object belongs to
// one element (registry WeakMap, element -> internals). Its slots hold the
// element, the validity flags and message, the submission value/state, the
// SameObject states/validity/labels, and an internal Map of ARIA reflection.
#include "document_model/element_internals.h"
#include "interaction/realm.h"
#include "js/Array.h"
#include "js/Conversions.h"
#include "js/ForOfIterator.h"
#include "js/MapAndSet.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/WeakMap.h"
#include "js/Wrapper.h"
#include <iterator>
#include <string>

namespace ruyijs::document_model {
const JSClass kInternalsClass = {"ElementInternals", JSCLASS_HAS_RESERVED_SLOTS(kInternalsSlots)};
namespace {
const char* const kFlagNames[] = {"valueMissing", "typeMismatch", "patternMismatch", "tooLong", "tooShort",
                                  "rangeUnderflow", "rangeOverflow", "stepMismatch", "badInput", "customError"};
std::string Lower(const DomString& name) {
  std::string out;
  for (char16_t c : name) out.push_back(char(c < 128 ? c : '?'));
  return out;
}
}  // namespace
bool InternalsFail(State& s, const std::string& message) { return ThrowDOM(s, "NotSupportedError", message.c_str()); }
JSObject* InternalsSelf(JS::HandleValue v) {
  JSObject* o = v.isObject() ? js::CheckedUnwrapStatic(&v.toObject()) : nullptr;
  return o && JS::GetClass(o) == &kInternalsClass ? o : nullptr;
}
bool InternalsIsElement(State& s, JS::HandleValue v) {
  JSObject* o = v.isObject() ? js::CheckedUnwrapStatic(&v.toObject()) : nullptr;
  Wrapper* entry = o && JS::GetClass(o) == &kNodeClass ? s.Find(o) : nullptr;
  NodeSnapshot node;
  return entry && entry->owner && entry->owner->model.Inspect(entry->key, &node).code == ErrorCode::None &&
         node.kind == NodeKind::Element;
}
CustomElementDefinition* InternalsDefinition(State& s, JSObject* internals) {
  JSObject* element = &JS::GetReservedSlot(internals, kElement).toObject();
  Wrapper* entry = s.Find(element);
  if (!entry || !s.customElements) return nullptr;
  auto found = s.customElements->states.find(entry->key);
  return found == s.customElements->states.end() ? nullptr : found->second.definition;
}
// HTMLElement.prototype.attachInternals()
bool AttachInternals(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& s = *FunctionState(args);
  const std::string prefix = "HTMLElement.attachInternals: ";
  JSObject* raw = args.thisv().isObject() ? js::CheckedUnwrapStatic(&args.thisv().toObject()) : nullptr;
  Wrapper* entry = raw && JS::GetClass(raw) == &kNodeClass ? s.Find(raw) : nullptr;
  NodeSnapshot node;
  if (!entry || !s.Check(s.model.Inspect(entry->key, &node)) || node.kind != NodeKind::Element || !node.is_html_element)
    return TypeError(cx, "'attachInternals' called on an object that does not implement interface HTMLElement.");
  const std::string name = node.local_name ? Lower(*node.local_name) : "";
  auto found = s.customElements->states.find(entry->key);
  CustomElementDefinition* definition = found == s.customElements->states.end() ? nullptr : found->second.definition;
  if (!definition) {
    if (name.find('-') != std::string::npos && name[0] >= 'a' && name[0] <= 'z')
      return InternalsFail(s, prefix + "Cannot attach ElementInternals to a non-custom element '" + name + "'");
    return InternalsFail(s, prefix + "Cannot attach ElementInternals to a customized built-in or non-custom element '" + name + "'");
  }
  if (definition->disableInternals)
    return InternalsFail(s, prefix + "AttachInternal() to '" + name + "' is disabled by disabledFeatures");
  JS::RootedObject map(cx, s.customElements->internals), element(cx, raw);
  JS::RootedValue key(cx, JS::ObjectValue(*element)), existing(cx);
  if (!JS::GetWeakMapEntry(cx, map, key, &existing)) return false;
  if (!existing.isUndefined()) return InternalsFail(s, prefix + "AttachInternals() has already been called from '" + name + "'");
  JS::RootedObject proto(cx, s.Find("ElementInternals")->prototype);
  JS::RootedObject internals(cx, JS_NewObjectWithGivenProto(cx, &kInternalsClass, proto));
  JS::RootedObject states(cx, NewCustomStateSet(s)), validity(cx), labels(cx), aria(cx, JS::NewMapObject(cx));
  if (!internals || !states || !aria || !NewInternalsValidity(s, internals, &validity) ||
      !s.NodeListOwned({}, &labels)) return false;
  JS::SetReservedSlot(internals, kElement, JS::ObjectValue(*element));
  JS::SetReservedSlot(internals, kFlags, JS::Int32Value(0));
  JS::SetReservedSlot(internals, kMessage, JS::StringValue(JS_GetEmptyString(cx)));
  JS::SetReservedSlot(internals, kValue, JS::NullValue());
  JS::SetReservedSlot(internals, kFormState, JS::NullValue());
  JS::SetReservedSlot(internals, kStates, JS::ObjectValue(*states));
  JS::SetReservedSlot(internals, kValidity, JS::ObjectValue(*validity));
  JS::SetReservedSlot(internals, kLabels, JS::ObjectValue(*labels));
  JS::SetReservedSlot(internals, kAria, JS::ObjectValue(*aria));
  JS::RootedValue value(cx, JS::ObjectValue(*internals));
  if (!JS::SetWeakMapEntry(cx, map, key, value)) return false;
  args.rval().set(value);
  return true;
}
bool InternalsSetValidity(State& s, JS::HandleObject self, const JS::CallArgs& args) {
  JSContext* cx = s.cx;
  const std::string prefix = "ElementInternals.setValidity: ";
  JS::HandleValue flagsValue = args.get(0);
  if (!flagsValue.isNullOrUndefined() && !flagsValue.isObject())
    return TypeError(cx, (prefix + "Argument 1 can't be converted to a dictionary.").c_str());
  int32_t flags = 0;
  if (flagsValue.isObject()) {
    // ValidityStateFlags members in lexicographic order; bits keep ValidityState's order.
    static const int kOrder[] = {8, 9, 2, 6, 5, 7, 3, 4, 1, 0};
    JS::RootedObject o(cx, &flagsValue.toObject());
    JS::RootedValue v(cx);
    for (int index : kOrder) {
      if (!JS_GetProperty(cx, o, kFlagNames[index], &v)) return false;
      if (JS::ToBoolean(v)) flags |= 1 << index;
    }
  }
  JS::RootedString message(cx, JS_GetEmptyString(cx));
  if (!args.get(1).isUndefined() && !(message = JS::ToString(cx, args[1]))) return false;
  if (!InternalsDefinition(s, self) || !InternalsDefinition(s, self)->formAssociated)
    return InternalsFail(s, prefix + "Target element is not a form-associated custom element");
  if (flags && JS_GetStringLength(message) == 0) return TypeError(cx, (prefix + "Need to provide validation message").c_str());
  JS::SetReservedSlot(self, kFlags, JS::Int32Value(flags));
  JS::SetReservedSlot(self, kMessage, JS::StringValue(flags ? message.get() : JS_GetEmptyString(cx)));
  args.rval().setUndefined();
  return true;
}
bool InternalsValidityFlags(JSObject* internals, uint32_t* flags) {
  if (!internals || JS::GetClass(internals) != &kInternalsClass) return false;
  *flags = uint32_t(JS::GetReservedSlot(internals, kFlags).toInt32());
  return true;
}
}  // namespace ruyijs::document_model
