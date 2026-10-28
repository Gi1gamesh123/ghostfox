// Detached page and keyframe declarations use one realm-local native Exception.
#include "web_styling/rule_exception.h"
#include "js/Exception.h"
#include "js/Realm.h"
#include "js/Stack.h"
namespace ruyijs::web_styling {
bool CheckRuleDeclaration(State& caller,JS::HandleObject rule) {
  if((RuleDataOf(rule).kind!=8&&RuleDataOf(rule).kind!=6)||!RuleSlotValue(rule,RuleParent).isNull()) return true;
  JSContext* cx=caller.cx;JS::RootedValue value(cx);
  {
    JSAutoRealm realm(cx,caller.page);
    JS::RootedObject frame(cx),exception(cx);
    if(!JS::CaptureCurrentStack(cx,&frame)) return false;
    exception=JS_NewObjectWithGivenProto(cx,&kRuleExceptionClass,caller.ruleException.prototype);
    JS::RootedString name(cx,JS_NewStringCopyZ(cx,"NS_ERROR_NOT_AVAILABLE"));
    JS::RootedString message(cx,JS_NewStringCopyZ(cx,""));
    if(!exception||!name||!message) return false;
    JS::SetReservedSlot(exception,ExceptionName,JS::StringValue(name));
    JS::SetReservedSlot(exception,ExceptionMessage,JS::StringValue(message));
    JS::SetReservedSlot(exception,ExceptionResult,JS::DoubleValue(2147746065.0));
    JS::SetReservedSlot(exception,ExceptionFrame,JS::ObjectOrNullValue(frame));
    value.setObject(*exception);
  }
  if(!JS_WrapValue(cx,&value)) return false;
  JS_SetPendingException(cx,value);return false;
}
}
