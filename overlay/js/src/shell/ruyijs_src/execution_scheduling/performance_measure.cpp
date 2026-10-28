#include "execution_scheduling/performance.h"
namespace ruyijs::execution_scheduling {
namespace {
bool Union(JSContext* cx,JS::MutableHandleValue value) {
 if(value.isUndefined())return true;
 if(value.isNumber()){double n;return PerfNumber(cx,value,&n);}
 JS::RootedString text(cx);if(!PerfString(cx,value,&text))return false;
 value.setString(text);return true;
}
}
bool PerfMeasure(JSContext* cx,JS::HandleObject owner,const JS::CallArgs& args,JS::MutableHandleValue result) {
 if(!args.length())return Error(cx,"Performance.measure requires a name");
 JS::RootedString name(cx);if(!PerfString(cx,args[0],&name))return false;
 JS::RootedValue input(cx,args.get(1)),start(cx),end(cx),duration(cx),detail(cx),extra(cx),cloned(cx);
 bool dictionary=input.isNullOrUndefined()||input.isObject();double d=0;
 if(dictionary){
  if(!PerfOption(cx,input,"detail",&detail)||!PerfOption(cx,input,"duration",&duration)||
     (!duration.isUndefined()&&!PerfNumber(cx,duration,&d))||
     !PerfOption(cx,input,"end",&end)||!Union(cx,&end)||
     !PerfOption(cx,input,"start",&start)||!Union(cx,&start))return false;
 }else{JS::RootedString text(cx);if(!PerfString(cx,input,&text))return false;start.setString(text);}
 if(!args.get(2).isUndefined()){
  JS::RootedString text(cx);if(!PerfString(cx,args[2],&text))return false;extra.setString(text);
 }
 bool hasStart=!start.isUndefined(),hasEnd=!end.isUndefined(),hasDuration=!duration.isUndefined();
 bool nonempty=dictionary&&(hasStart||hasEnd||hasDuration||!detail.isUndefined());
 if(nonempty&&(!extra.isUndefined()||(!hasStart&&!hasEnd)||(hasStart&&hasEnd&&hasDuration)))
  return Error(cx,"Invalid PerformanceMeasure options combination");
 double from=0,to=0;
 if(!extra.isUndefined()){if(!PerfTimestamp(cx,owner,extra,&to))return false;}
 else if(hasEnd){if(!PerfTimestamp(cx,owner,end,&to))return false;}
 else if(dictionary&&hasStart&&hasDuration){
  if(!PerfTimestamp(cx,owner,start,&to))return false;
  if(d<0)return Error(cx,"Performance duration must be nonnegative");to+=d;
 }else to=PerfNow(owner);
 if(hasStart){if(!PerfTimestamp(cx,owner,start,&from))return false;}
 else if(hasDuration&&hasEnd){
  if(d<0)return Error(cx,"Performance duration must be nonnegative");
  if(!PerfTimestamp(cx,owner,end,&from))return false;from-=d;
 }
 if(!PerfClone(cx,owner,detail,&cloned))return false;
 JS::RootedObject object(cx),prototype(cx);
 if(!PerfEntry(cx,owner,MeasureEntry,name,from,to-from,cloned,prototype,&object)||
    !PerfRecord(cx,owner,object))return false;
 result.setObject(*object);return JS_WrapValue(cx,result);
}
}
