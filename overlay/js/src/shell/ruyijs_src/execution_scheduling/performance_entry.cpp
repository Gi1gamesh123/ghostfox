#include "execution_scheduling/performance.h"
#include "execution_scheduling/performance_observer.h"
#include "js/MapAndSet.h"
#include "js/Wrapper.h"
namespace ruyijs::execution_scheduling {
const JSClass kEntryClass={"PerformanceEntry",JSCLASS_HAS_RESERVED_SLOTS(EntrySlots)};
const char* PerfEntryType(int kind){return kind==Mark?"mark":kind==MeasureEntry?"measure":
 kind==Resource?"resource":kind==NavigationEntry?"navigation":"";}
int PerfTimelineKind(int kind){return kind==Mark?0:kind==MeasureEntry?1:kind==Resource?2:
 kind==NavigationEntry?3:4;}
namespace {
bool Receiver(JSContext* cx,JS::HandleValue value,int required,JS::MutableHandleObject out) {
 out.set(value.isObject()?js::CheckedUnwrapStatic(&value.toObject()):nullptr);
 if(out&&JS::GetClass(out)==&kEntryClass){
  int kind=JS::GetReservedSlot(out,EKind).toInt32();
  if(required==Entry&&kind<=LargestPaint)return true;
  if(required==kind)return true;
  // PerformanceNavigationTiming inherits every PerformanceResourceTiming member.
  if(required==Resource&&kind==NavigationEntry)return true;
 }
 return Error(cx,"Illegal PerformanceEntry receiver");
}
}
bool PerfEntryMember(JSContext* cx,unsigned argc,JS::Value* vp) {
 auto args=JS::CallArgsFromVp(argc,vp);int selector=PerfSelector(args),kind=selector/100,field=selector%100;
 JS::RootedObject self(cx);if(!Receiver(cx,args.thisv(),kind,&self))return false;
 if(kind==Mark||kind==MeasureEntry)args.rval().set(JS::GetReservedSlot(self,EDetail));
 else if(kind==Entry){
  if(field==1){
   int actual=JS::GetReservedSlot(self,EKind).toInt32();
   JS::RootedString type(cx,JS_NewStringCopyZ(cx,PerfEntryType(actual)));
   if(!type)return false;args.rval().setString(type);
  }else args.rval().set(JS::GetReservedSlot(self,field==0?EName:field==2?EStart:EDuration));
 }else if(kind==Resource){if(!PerfResourceField(cx,self,field,args.rval()))return false;}
 else if(kind==NavigationEntry){
  if(!PerfNavigationField(cx,self,field,args.rval()))return false;
 }
 else return Error(cx,"No native timing record is available");
 return JS_WrapValue(cx,args.rval());
}
bool PerfEntryJson(JSContext* cx,unsigned argc,JS::Value* vp) {
 auto args=JS::CallArgsFromVp(argc,vp);JS::RootedObject self(cx);
 if(!Receiver(cx,args.thisv(),PerfSelector(args),&self))return false;
 JS::RootedObject json(cx,JS_NewPlainObject(cx));
 JS::RootedValue name(cx,JS::GetReservedSlot(self,EName));
 int actual=JS::GetReservedSlot(self,EKind).toInt32();
 JS::RootedString type(cx,JS_NewStringCopyZ(cx,PerfEntryType(actual)));
 if(!json||!type||!JS_WrapValue(cx,&name)||
    !JS_DefineProperty(cx,json,"name",name,JSPROP_ENUMERATE)||
    !JS_DefineProperty(cx,json,"entryType",type,JSPROP_ENUMERATE)||
    !JS_DefineProperty(cx,json,"startTime",JS::GetReservedSlot(self,EStart).toNumber(),JSPROP_ENUMERATE)||
    !JS_DefineProperty(cx,json,"duration",JS::GetReservedSlot(self,EDuration).toNumber(),JSPROP_ENUMERATE))return false;
 int kind=JS::GetReservedSlot(self,EKind).toInt32();
 if(kind==Resource||kind==NavigationEntry){
  // PerformanceNavigationTiming inherits the whole resource field set (it is
  // typed after PerformanceResourceTiming), then adds its own.
  for(unsigned i=0;i<kPerfSchema[Resource].count;++i){JS::RootedValue value(cx);
   if(!PerfResourceField(cx,self,i,&value)||!JS_WrapValue(cx,&value)||
      !JS_DefineProperty(cx,json,kPerfSchema[Resource].fields[i],value,JSPROP_ENUMERATE))return false;
  }
 }
 if(kind==NavigationEntry){
  for(unsigned i=0;i<kPerfSchema[NavigationEntry].count;++i){JS::RootedValue value(cx);
   if(!PerfNavigationField(cx,self,i,&value)||!JS_WrapValue(cx,&value)||
      !JS_DefineProperty(cx,json,kPerfSchema[NavigationEntry].fields[i],value,JSPROP_ENUMERATE))return false;
  }
 }
 args.rval().setObject(*json);return true;
}
bool PerfEntry(JSContext* cx,JS::HandleObject owner,int kind,JS::HandleString name,
 double start,double duration,JS::HandleValue detail,JS::HandleObject custom,JS::MutableHandleObject out) {
 JSAutoRealm realm(cx,owner);
 JS::RootedObject prototypes(cx,&JS::GetReservedSlot(owner,PPrototypes).toObject()),prototype(cx,custom);
 JS::RootedValue value(cx);
 if(!prototype){if(!JS_GetElement(cx,prototypes,kind,&value))return false;prototype=&value.toObject();}
 JS::RootedValue localName(cx,JS::StringValue(name)),localDetail(cx,detail);
 if(!JS_WrapObject(cx,&prototype)||!JS_WrapValue(cx,&localName)||!JS_WrapValue(cx,&localDetail))return false;
 out.set(JS_NewObjectWithGivenProto(cx,&kEntryClass,prototype));if(!out)return false;
 JS::SetReservedSlot(out,EKind,JS::Int32Value(kind));JS::SetReservedSlot(out,EName,localName);
 JS::SetReservedSlot(out,EStart,JS::DoubleValue(start));JS::SetReservedSlot(out,EDuration,JS::DoubleValue(duration));
 JS::SetReservedSlot(out,EDetail,localDetail);return true;
}
bool PerfRecord(JSContext* cx,JS::HandleObject owner,JS::HandleObject entry) {
 JS::RootedString name(cx,JS::GetReservedSlot(entry,EName).toString());std::u16string text;
 if(!PerfText(cx,name,&text))return false;
 uint32_t id=0;bool schedule=false;
 int kind=JS::GetReservedSlot(entry,EKind).toInt32();
 if(kind==Resource&&!PerfNotifyEntry(cx,owner,entry))return false;
 auto* timeline=static_cast<Timeline*>(JS::GetReservedSlot(owner,PTimeline).toPrivate());
 if(!InsertTimeline(timeline,text,PerfTimelineKind(kind),
     JS::GetReservedSlot(entry,EStart).toNumber(),JS::GetReservedSlot(entry,EDuration).toNumber(),&id,&schedule)){
  JS_ReportOutOfMemory(cx);return false;
 }
 JSAutoRealm realm(cx,owner);
 JS::RootedObject map(cx,&JS::GetReservedSlot(owner,PEntries).toObject());
 JS::RootedValue key(cx,JS::NumberValue(id)),value(cx,JS::ObjectValue(*entry));
 return JS_WrapValue(cx,&value)&&JS::MapSet(cx,map,key,value)&&
   (kind==Resource||kind==NavigationEntry||PerfNotifyEntry(cx,owner,entry))&&(!schedule||PerfQueueResourceTask(cx,owner));
}
}
