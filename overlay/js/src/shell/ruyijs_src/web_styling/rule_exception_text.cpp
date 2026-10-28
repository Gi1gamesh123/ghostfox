#include "web_styling/rule_exception.h"
#include "js/SavedFrameAPI.h"
#include <cstdio>
namespace ruyijs::web_styling {
namespace {
bool WrapString(JSContext* cx,JS::MutableHandleString text) {
  JS::RootedValue value(cx,JS::StringValue(text));
  if(!JS_WrapValue(cx,&value)) return false;
  text.set(value.toString());return true;
}
bool Append(JSContext* cx,JS::MutableHandleString text,JS::HandleString part) {
  text.set(JS_ConcatStrings(cx,text,part));return bool(text);
}
bool Append(JSContext* cx,JS::MutableHandleString text,const char* literal) {
  JS::RootedString part(cx,JS_NewStringCopyZ(cx,literal));return part&&Append(cx,text,part);
}
}
bool RuleExceptionString(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject self(cx);
  if(!RuleExceptionReceiver(cx,args.thisv(),"toString",&self)) return false;
  const auto& slot=JS::GetReservedSlot(self,ExceptionFrame);
  JS::RootedObject frame(cx,slot.isObject()?&slot.toObject():nullptr);
  JS::RootedString source(cx),name(cx);uint32_t line=0;
  JS::GetSavedFrameSource(cx,nullptr,frame,&source,JS::SavedFrameSelfHosted::Exclude);
  JS::GetSavedFrameFunctionDisplayName(cx,nullptr,frame,&name,JS::SavedFrameSelfHosted::Exclude);
  JS::GetSavedFrameLine(cx,nullptr,frame,&line,JS::SavedFrameSelfHosted::Exclude);
  if(!source) source=JS_GetEmptyString(cx);
  if(!WrapString(cx,&source)||(name&&!WrapString(cx,&name))) return false;
  JS::RootedString text(cx,JS_NewStringCopyZ(cx,
    "[Exception... \"Component is not available\"  nsresult: \"0x80040111 (NS_ERROR_NOT_AVAILABLE)\"  location: \"JS frame :: "));
  char ending[64];std::snprintf(ending,sizeof(ending)," :: line %u\"  data: no]",line);
  if(!text||!source||!Append(cx,&text,source)||!Append(cx,&text," :: ")||
      !(name?Append(cx,&text,name):Append(cx,&text,"<TOP_LEVEL>"))||!Append(cx,&text,ending)) return false;
  args.rval().setString(text);return true;
}
}
