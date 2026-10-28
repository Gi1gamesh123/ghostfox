// The Styling owner's hidden WebIDL Exception interface and original factory.
#pragma once
#include "web_styling/cssom.h"
namespace ruyijs::web_styling {
extern const JSClass kRuleExceptionClass;
enum RuleExceptionSlot { ExceptionName, ExceptionMessage, ExceptionResult, ExceptionFrame };
inline constexpr const char* kRuleExceptionMembers[]={
  "name","message","result","filename","lineNumber","columnNumber","data","stack"};
bool InstallRuleException(State&);
bool RuleExceptionReceiver(JSContext*,JS::HandleValue,const char*,JS::MutableHandleObject);
bool RuleExceptionGet(JSContext*,unsigned,JS::Value*);
bool RuleExceptionStackSet(JSContext*,unsigned,JS::Value*);
bool RuleExceptionString(JSContext*,unsigned,JS::Value*);
}
