// TrustedTypePolicyFactory: the one per-realm `trustedTypes` object.
// Behaviour follows TrustedTypePolicyFactory.cpp; CSP enforcement is absent (D127).
#include "web_trusted_types/internal.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/Object.h"
#include "js/PropertyAndElement.h"
#include "js/String.h"
#include "js/ValueArray.h"
#include "js/Wrapper.h"
#include "mozilla/Range.h"
#include <string>

namespace ruyijs::web_trusted_types {
const JSClass kFactoryClass = {"TrustedTypePolicyFactory",
                               JSCLASS_HAS_RESERVED_SLOTS(FactorySlotCount)};
namespace {
constexpr const char* kIface = "TrustedTypePolicyFactory";
const char* const kOptions[3] = {"createHTML", "createScript", "createScriptURL"};
const char* const kTypeNames[3] = {"TrustedHTML", "TrustedScript", "TrustedScriptURL"};
bool Text(JSContext* cx, JS::HandleValue value, std::u16string* out, bool nullable = false) {
  out->clear();
  if (value.isUndefined() || (nullable && value.isNull())) return true;
  JS::RootedString text(cx, JS::ToString(cx, value));
  if (!text) return false;
  out->resize(JS_GetStringLength(text));
  return JS_CopyStringChars(cx, mozilla::Range<char16_t>(out->data(), out->size()), text);
}
std::u16string Lower(std::u16string text) {
  for (auto& c : text) if (c >= u'A' && c <= u'Z') c = char16_t(c + 32);
  return text;
}
bool Result(JSContext* cx, int type, JS::MutableHandleValue rval) {
  if (type < 0) { rval.setNull(); return true; }
  JS::RootedString name(cx, JS_NewStringCopyZ(cx, kTypeNames[type]));
  if (!name) return false;
  rval.setString(name);
  return true;
}
bool Factory(JSContext* cx, const JS::CallArgs& args, const char* member,
             JS::MutableHandleObject self) {
  return Receiver(cx, args.thisv(), &kFactoryClass, member, kIface, self);
}
bool CreatePolicy(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = Self(args);
  JS::RootedObject self(cx);
  if (!Factory(cx, args, "createPolicy", &self) ||
      !Arity(cx, args, 1, "TrustedTypePolicyFactory.createPolicy")) return false;
  JS::RootedString name(cx, JS::ToString(cx, args[0]));
  if (!name) return false;
  JS::RootedValueArray<3> callbacks(cx);
  JS::RootedValue options(cx, args.get(1));
  if (!options.isNullOrUndefined()) {
    if (!options.isObject())
      return Fail(cx, "TrustedTypePolicyFactory.createPolicy: Argument 2 can't be converted to a dictionary.");
    JS::RootedObject dictionary(cx, &options.toObject());
    for (int index = 0; index < 3; ++index) {
      if (!JS_GetProperty(cx, dictionary, kOptions[index], callbacks[index])) return false;
      if (callbacks[index].isUndefined()) continue;
      const std::string prefix = std::string("TrustedTypePolicyFactory.createPolicy: '") +
          kOptions[index] + "' member of TrustedTypePolicyOptions is not ";
      if (!callbacks[index].isObject()) return Fail(cx, (prefix + "an object.").c_str());
      if (!JS::IsCallable(&callbacks[index].toObject()))
        return Fail(cx, (prefix + "callable.").c_str());
    }
  }
  JS::RootedValue current(cx, JS::GetReservedSlot(self, FactoryDefault));
  bool isDefault = false;
  if (!JS_StringEqualsAscii(cx, name, "default", &isDefault)) return false;
  if (isDefault && current.isObject())
    return Fail(cx, "TrustedTypePolicyFactory.createPolicy: Tried to create a second default policy");
  JS::RootedObject policy(cx);
  {
    JSAutoRealm realm(cx, state.kit.page);
    JS::RootedValue nameValue(cx, JS::StringValue(name));
    if (!JS_WrapValue(cx, &nameValue)) return false;
    for (int index = 0; index < 3; ++index) if (!JS_WrapValue(cx, callbacks[index])) return false;
    JS::RootedString text(cx, nameValue.toString());
    policy = NewPolicy(state, text, callbacks[0], callbacks[1], callbacks[2]);
    if (!policy) return false;
    if (isDefault) JS::SetReservedSlot(self, FactoryDefault, JS::ObjectValue(*policy));
  }
  args.rval().setObject(*policy);
  return JS_WrapValue(cx, args.rval());
}
bool Is(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int32_t kind = WebKit::Slot(args);
  static const char* const names[3] = {"isHTML", "isScript", "isScriptURL"};
  JS::RootedObject self(cx);
  if (!Factory(cx, args, names[kind], &self) ||
      !Arity(cx, args, 1, (std::string("TrustedTypePolicyFactory.") + names[kind]).c_str()))
    return false;
  args.rval().setBoolean(args[0].isObject() && IsValue(&args[0].toObject(), Kind(kind)));
  return true;
}
// emptyHTML (0), emptyScript (1) and defaultPolicy (2) read factory slots.
bool Slot(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  const int32_t slot = WebKit::Slot(args);
  static const char* const names[3] = {"get emptyHTML", "get emptyScript", "get defaultPolicy"};
  static const int slots[3] = {FactoryEmptyHTML, FactoryEmptyScript, FactoryDefault};
  JS::RootedObject self(cx);
  if (!Factory(cx, args, names[slot], &self)) return false;
  args.rval().set(JS::GetReservedSlot(self, slots[slot]));
  return JS_WrapValue(cx, args.rval());
}
bool AttributeType(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  std::u16string tag, attribute, elementNs, attrNs;
  if (!Factory(cx, args, "getAttributeType", &self) ||
      !Arity(cx, args, 2, "TrustedTypePolicyFactory.getAttributeType") ||
      !Text(cx, args[0], &tag) || !Text(cx, args[1], &attribute) ||
      !Text(cx, args.get(2), &elementNs, true) || !Text(cx, args.get(3), &attrNs, true)) return false;
  const bool xlink = attrNs == u"http://www.w3.org/1999/xlink";
  const bool html = elementNs.empty() || elementNs == u"http://www.w3.org/1999/xhtml";
  const bool svg = elementNs == u"http://www.w3.org/2000/svg";
  const bool mathml = elementNs == u"http://www.w3.org/1998/Math/MathML";
  int type = -1;
  attribute = Lower(attribute);
  tag = Lower(tag);
  if ((attrNs.empty() || xlink) && (html || svg || mathml)) {
    if (attrNs.empty() && attribute.size() > 2 && attribute.compare(0, 2, u"on") == 0) type = 1;
    else if (html && tag == u"iframe" && attrNs.empty() && attribute == u"srcdoc") type = 0;
    else if (html && tag == u"script" && attrNs.empty() && attribute == u"src") type = 2;
    else if (svg && tag == u"script" && attribute == u"href") type = 2;
  }
  return Result(cx, type, args.rval());
}
bool PropertyType(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  std::u16string tag, property, elementNs;
  if (!Factory(cx, args, "getPropertyType", &self) ||
      !Arity(cx, args, 2, "TrustedTypePolicyFactory.getPropertyType") ||
      !Text(cx, args[0], &tag) || !Text(cx, args[1], &property) ||
      !Text(cx, args.get(2), &elementNs, true)) return false;
  int type = -1;
  tag = Lower(tag);
  if (elementNs.empty() || elementNs == u"http://www.w3.org/1999/xhtml") {
    if (tag == u"iframe" && property == u"srcdoc") type = 0;
    else if (tag == u"script" && (property == u"innerText" || property == u"text" ||
                                  property == u"textContent")) type = 1;
    else if (tag == u"script" && property == u"src") type = 2;
  }
  if (type < 0 && (property == u"innerHTML" || property == u"outerHTML")) type = 0;
  return Result(cx, type, args.rval());
}
bool Global(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = Self(args);
  if (args.thisv().isObject() &&
      js::CheckedUnwrapStatic(&args.thisv().toObject()) != state.kit.page.get())
    return Fail(cx, "'get trustedTypes' called on an object that does not implement interface Window.");
  args.rval().setObject(*state.factory);
  return JS_WrapValue(cx, args.rval());
}
}
bool InstallFactory(State& state, JS::HandleObject globalTarget, const char* globalIface) {
  JSContext* cx = state.cx;
  WebFace& face = state.factoryFace;
  if (!state.kit.Define(kIface, &kFactoryClass, nullptr, WebKit::Reject, &face) ||
      !state.kit.Method(face, kIface, "createPolicy", CreatePolicy, 1, 0) ||
      !state.kit.Method(face, kIface, "isHTML", Is, 1, 0) ||
      !state.kit.Method(face, kIface, "isScript", Is, 1, 1) ||
      !state.kit.Method(face, kIface, "isScriptURL", Is, 1, 2) ||
      !state.kit.Method(face, kIface, "getAttributeType", AttributeType, 2, 0) ||
      !state.kit.Method(face, kIface, "getPropertyType", PropertyType, 2, 0) ||
      !state.kit.Accessor(face, kIface, "emptyHTML", Slot, nullptr, 0) ||
      !state.kit.Accessor(face, kIface, "emptyScript", Slot, nullptr, 1) ||
      !state.kit.Accessor(face, kIface, "defaultPolicy", Slot, nullptr, 2) ||
      !FinishPrototype(state, face)) return false;
  state.factory = state.kit.New(&kFactoryClass, face.prototype);
  JS::RootedString empty(cx, JS_GetEmptyString(cx));
  JS::RootedObject html(cx, NewValue(state, Kind::HTML, empty));
  JS::RootedObject script(cx, NewValue(state, Kind::Script, empty));
  JS::RootedObject getter(cx, state.kit.Function(Global, "get trustedTypes", 0, 0)), setter(cx);
  if (!state.factory || !html || !script || !getter) return false;
  JS::SetReservedSlot(state.factory, FactoryDefault, JS::NullValue());
  JS::SetReservedSlot(state.factory, FactoryEmptyHTML, JS::ObjectValue(*html));
  JS::SetReservedSlot(state.factory, FactoryEmptyScript, JS::ObjectValue(*script));
  return JS_DefineProperty(cx, globalTarget, "trustedTypes", getter, setter, JSPROP_ENUMERATE) &&
         state.kit.helpers->Accessor(globalIface, globalTarget, "trustedTypes") &&
         state.kit.helpers->Object(kIface, state.factory);
}
}
