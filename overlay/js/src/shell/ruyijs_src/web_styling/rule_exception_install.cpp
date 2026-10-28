#include "web_styling/rule_exception.h"
#include "js/Symbol.h"
#include "js/Wrapper.h"
#include <cstdio>
#include <iterator>
namespace ruyijs::web_styling {
const JSClass kRuleExceptionClass={"Exception",JSCLASS_HAS_RESERVED_SLOTS(4)};
bool RuleExceptionReceiver(JSContext* cx,JS::HandleValue value,const char* label,
                           JS::MutableHandleObject out) {
  out.set(value.isObject()?js::CheckedUnwrapStatic(&value.toObject()):nullptr);
  if(out&&JS::GetClass(out)==&kRuleExceptionClass) return true;
  char message[160];std::snprintf(message,sizeof(message),
    "'%s' called on an object that does not implement interface Exception.",label);
  return WebKit::Fail(cx,message);
}
bool InstallRuleException(State& state) {
  auto& kit=state.kit;auto& face=state.ruleException;
  // LegacyNoInterfaceObject: retain this native prototype without a constructor.
  face.prototype=JS_NewPlainObject(kit.cx);
  if(!face.prototype||!kit.Method(face,"Exception","toString",RuleExceptionString,0,0)) return false;
  for(size_t i=0;i<std::size(kRuleExceptionMembers);++i)
    if(!kit.Accessor(face,"Exception",kRuleExceptionMembers[i],RuleExceptionGet,
        i==7?RuleExceptionStackSet:nullptr,int32_t(i))) return false;
  JS::RootedString tag(kit.cx,JS_NewStringCopyZ(kit.cx,"Exception"));
  JS::RootedId id(kit.cx,JS::PropertyKey::Symbol(JS::GetWellKnownSymbol(kit.cx,JS::SymbolCode::toStringTag)));
  return tag&&JS_DefinePropertyById(kit.cx,face.prototype,id,tag,JSPROP_READONLY);
}
}
