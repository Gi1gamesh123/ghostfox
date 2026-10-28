#include "web_styling/rule_exception.h"
#include "js/SavedFrameAPI.h"
#include "js/Stack.h"
#include <cstdio>
namespace ruyijs::web_styling {
bool RuleExceptionGet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);int member=WebKit::Slot(args);
  char label[40];std::snprintf(label,sizeof(label),"get %s",kRuleExceptionMembers[member]);
  JS::RootedObject self(cx);
  if(!RuleExceptionReceiver(cx,args.thisv(),label,&self)) return false;
  if(member<3) {args.rval().set(JS::GetReservedSlot(self,uint32_t(member)));return JS_WrapValue(cx,args.rval());}
  const auto& slot=JS::GetReservedSlot(self,ExceptionFrame);
  JS::RootedObject frame(cx,slot.isObject()?&slot.toObject():nullptr);
  JS::RootedString text(cx);uint32_t line=0;JS::TaggedColumnNumberOneOrigin column;
  switch(member) {
    case 3: JS::GetSavedFrameSource(cx,nullptr,frame,&text,JS::SavedFrameSelfHosted::Exclude);break;
    case 4:
      JS::GetSavedFrameLine(cx,nullptr,frame,&line,JS::SavedFrameSelfHosted::Exclude);
      args.rval().setNumber(line);return true;
    case 5:
      JS::GetSavedFrameColumn(cx,nullptr,frame,&column,JS::SavedFrameSelfHosted::Exclude);
      args.rval().setNumber(column.oneOriginValue());return true;
    case 6: args.rval().setNull();return true;
    case 7: if(!JS::BuildStackString(cx,nullptr,frame,&text,0,js::StackFormat::SpiderMonkey)) return false;break;
  }
  if(!text) text=JS_GetEmptyString(cx);
  args.rval().setString(text);return JS_WrapValue(cx,args.rval());
}
bool RuleExceptionStackSet(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject self(cx);
  if(!RuleExceptionReceiver(cx,args.thisv(),"set stack",&self)) return false;
  // The replaceable member defines on the caller's receiver, including CCWs.
  JS::RootedObject target(cx,&args.thisv().toObject());
  if(!JS_DefineProperty(cx,target,"stack",args.get(0),JSPROP_ENUMERATE)) return false;
  args.rval().setUndefined();return true;
}
}
