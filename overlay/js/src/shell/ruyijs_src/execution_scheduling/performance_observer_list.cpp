#include "execution_scheduling/performance_observer.h"
#include "runtime/web_kit.h"
#include "js/Array.h"
#include "js/Wrapper.h"
#include <algorithm>
namespace ruyijs::execution_scheduling {
namespace {
bool FilterString(JSContext* cx,JS::MutableHandleValue value){
 if(value.isUndefined())return true;JS::RootedString text(cx);
 if(!PerfString(cx,value,&text))return false;value.setString(text);return true;
}
}
bool PerfListMethod(JSContext* cx,unsigned argc,JS::Value* vp){
 auto args=JS::CallArgsFromVp(argc,vp);int op=PerfSelector(args);
 JS::RootedObject self(cx,args.thisv().isObject()?js::CheckedUnwrapStatic(&args.thisv().toObject()):nullptr);
 if(!self||JS::GetClass(self)!=&kPerfListClass)return WebKit::FailBrand(cx,args,"PerformanceObserverEntryList");
 JS::RootedValue name(cx),type(cx),initiator(cx);
 if(op){
  if(!argc)return Error(cx,"PerformanceObserverEntryList query requires an argument");
  JS::RootedString text(cx);if(!PerfString(cx,args[0],&text))return false;
  if(op==1)type.setString(text);else {name.setString(text);type=args.get(1);if(!FilterString(cx,&type))return false;}
 }else{
  if(!PerfOption(cx,args.get(0),"entryType",&type)||!FilterString(cx,&type)||
     !PerfOption(cx,args.get(0),"initiatorType",&initiator)||!FilterString(cx,&initiator)||
     !PerfOption(cx,args.get(0),"name",&name)||!FilterString(cx,&name))return false;
 }
 std::u16string wantedName,wantedType,wantedInitiator;
 JS::RootedString text(cx);
 if(!name.isUndefined()){text=name.toString();if(!PerfText(cx,text,&wantedName))return false;}
 if(!type.isUndefined()){text=type.toString();if(!PerfText(cx,text,&wantedType))return false;}
 if(!initiator.isUndefined()){text=initiator.toString();if(!PerfText(cx,text,&wantedInitiator))return false;}
 JS::RootedObject queue(cx,&JS::GetReservedSlot(self,0).toObject());
 JS::RootedObject result(cx,JS::NewArrayObject(cx,0));if(!result)return false;
 JS::RootedValueVector values(cx);std::vector<std::pair<double,uint32_t>> order;
 uint32_t length=0;{JSAutoRealm realm(cx,queue);if(!JS::GetArrayLength(cx,queue,&length))return false;}
 for(uint32_t i=0;i<length;++i){JS::RootedValue value(cx);
  {JSAutoRealm realm(cx,queue);if(!JS_GetElement(cx,queue,i,&value))return false;}
  JS::RootedObject entry(cx,js::CheckedUnwrapStatic(&value.toObject()));
  text=JS::GetReservedSlot(entry,EName).toString();std::u16string actualName;
  if(!PerfText(cx,text,&actualName))return false;
  int kind=JS::GetReservedSlot(entry,EKind).toInt32();
  const char16_t* actualType=kind==Mark?u"mark":kind==Resource?u"resource":kind==NavigationEntry?u"navigation":u"measure";
  if((!name.isUndefined()&&wantedName!=actualName)||(!type.isUndefined()&&wantedType!=actualType))continue;
  if(!initiator.isUndefined()){
   if(kind!=Resource&&kind!=NavigationEntry)continue;
   JS::RootedValue field(cx);std::u16string actualInitiator;
   if(!PerfResourceField(cx,entry,0,&field))return false;text=field.toString();
   if(!PerfText(cx,text,&actualInitiator))return false;
   if(actualInitiator!=wantedInitiator)continue;
  }
  if(!JS_WrapValue(cx,&value)||!values.append(value))return false;
  order.emplace_back(JS::GetReservedSlot(entry,EStart).toNumber(),uint32_t(values.length()-1));
 }
 std::stable_sort(order.begin(),order.end(),[](const auto& a,const auto& b){return a.first<b.first;});
 for(uint32_t i=0;i<order.size();++i)if(!JS_SetElement(cx,result,i,values[order[i].second]))return false;
 args.rval().setObject(*result);return true;
}
}
