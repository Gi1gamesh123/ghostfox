// TrustedTypePolicy: a name and the three optional callbacks from the options
// dictionary. Each create* converts the input, runs its callback and wraps the
// result (null/undefined become the empty string), as TrustedTypePolicy.cpp does.
#include "web_trusted_types/internal.h"
#include "runtime/page_helpers.h"
#include "jsapi.h"
#include "js/CallAndConstruct.h"
#include "js/Conversions.h"
#include "js/GCVector.h"
#include "js/Object.h"
#include "js/String.h"
#include "js/ValueArray.h"
#include <string>

namespace ruyijs::web_trusted_types {
const JSClass kPolicyClass = {"TrustedTypePolicy", JSCLASS_HAS_RESERVED_SLOTS(PolicySlotCount)};
namespace {
const char* const kMethods[3] = {"createHTML", "createScript", "createScriptURL"};
// USVString: lone surrogates become U+FFFD.
JSString* WellFormed(JSContext* cx, JS::HandleString text) {
  JS::Rooted<JSLinearString*> linear(cx, JS_EnsureLinearString(cx, text));
  if (!linear) return nullptr;
  JSString* plain = JS_FORGET_STRING_LINEARNESS(linear);
  if (JS::LinearStringHasLatin1Chars(linear)) return plain;
  std::u16string chars;
  {
    JS::AutoCheckCannotGC nogc;
    size_t length = 0;
    const char16_t* data = JS_GetTwoByteStringCharsAndLength(cx, nogc, plain, &length);
    if (!data) return nullptr;
    chars.assign(data, length);
  }
  bool changed = false;
  for (size_t i = 0; i < chars.size(); ++i) {
    const char16_t c = chars[i];
    if (c >= 0xD800 && c <= 0xDBFF && i + 1 < chars.size() &&
        chars[i + 1] >= 0xDC00 && chars[i + 1] <= 0xDFFF) { ++i; continue; }
    if (c >= 0xD800 && c <= 0xDFFF) { chars[i] = 0xFFFD; changed = true; }
  }
  return changed ? JS_NewUCStringCopyN(cx, chars.data(), chars.size()) : plain;
}
bool Name(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  JS::RootedObject self(cx);
  if (!Receiver(cx, args.thisv(), &kPolicyClass, "get name", "TrustedTypePolicy", &self))
    return false;
  args.rval().set(JS::GetReservedSlot(self, PolicyName));
  return JS_WrapValue(cx, args.rval());
}
bool Create(JSContext* cx, unsigned argc, JS::Value* vp) {
  auto args = JS::CallArgsFromVp(argc, vp);
  State& state = Self(args);
  const int32_t kind = WebKit::Slot(args);
  const std::string method = std::string("TrustedTypePolicy.") + kMethods[kind];
  JS::RootedObject self(cx);
  if (!Receiver(cx, args.thisv(), &kPolicyClass, kMethods[kind], "TrustedTypePolicy", &self) ||
      !Arity(cx, args, 1, method.c_str())) return false;
  JS::RootedString input(cx, JS::ToString(cx, args[0]));
  if (!input) return false;
  JS::RootedValue callback(cx, JS::GetReservedSlot(self, PolicyCallback + kind));
  if (!callback.isObject()) return Fail(cx, (method + ": Function missing.").c_str());
  JS::RootedValueVector values(cx);
  if (!values.append(JS::StringValue(input))) return false;
  for (unsigned index = 1; index < args.length(); ++index)
    if (!values.append(args[index])) return false;
  JS::RootedValue result(cx), receiver(cx, JS::UndefinedValue());
  if (!JS::Call(cx, receiver, callback, JS::HandleValueArray(values), &result)) return false;
  JS::RootedString data(cx, result.isNullOrUndefined() ? JS_GetEmptyString(cx)
                                                       : JS::ToString(cx, result));
  if (data && kind == int32_t(Kind::ScriptURL)) data = WellFormed(cx, data);
  if (!data) return false;
  JS::RootedObject value(cx);
  {
    JSAutoRealm realm(cx, state.kit.page);
    JS::RootedValue wrapped(cx, JS::StringValue(data));
    if (!JS_WrapValue(cx, &wrapped)) return false;
    JS::RootedString text(cx, wrapped.toString());
    value = NewValue(state, Kind(kind), text);
  }
  if (!value) return false;
  args.rval().setObject(*value);
  return JS_WrapValue(cx, args.rval());
}
}
JSObject* NewPolicy(State& state, JS::HandleString name, JS::HandleValue html,
                    JS::HandleValue script, JS::HandleValue url) {
  JS::RootedObject policy(state.cx, state.kit.New(&kPolicyClass, state.policy.prototype));
  if (!policy) return nullptr;
  JS::SetReservedSlot(policy, PolicyName, JS::StringValue(name));
  JS::SetReservedSlot(policy, PolicyCallback + 0, html);
  JS::SetReservedSlot(policy, PolicyCallback + 1, script);
  JS::SetReservedSlot(policy, PolicyCallback + 2, url);
  return policy;
}
bool InstallPolicy(State& state) {
  WebFace& face = state.policy;
  if (!state.kit.Define("TrustedTypePolicy", &kPolicyClass, nullptr, WebKit::Reject, &face))
    return false;
  for (int32_t kind = 0; kind < 3; ++kind)
    if (!state.kit.Method(face, "TrustedTypePolicy", kMethods[kind], Create, 1, kind))
      return false;
  return state.kit.Accessor(face, "TrustedTypePolicy", "name", Name, nullptr, 0) &&
         FinishPrototype(state, face);
}
}
