#include "execution_scheduling/performance_observer.h"
#include "js/Conversions.h"
#include "js/ForOfIterator.h"
#include "js/MapAndSet.h"
namespace ruyijs::execution_scheduling {
namespace {
bool Type(JSContext* cx,JS::HandleValue input,uint32_t* bit){
 JS::RootedString name(cx);if(!PerfString(cx,input,&name))return false;*bit=0;
 for(unsigned i=0;i<8;++i){bool equal=false;if(!JS_StringEqualsAscii(cx,name,kObservedTypes[i],&equal))return false;
  if(equal){*bit=1u<<i;break;}}
 return true;
}
}
bool PerfObserve(JSContext* cx,JS::HandleObject object,JS::HandleValue input){
 JS::RootedValue buffered(cx),duration(cx),types(cx),type(cx);
 if(!PerfOption(cx,input,"buffered",&buffered)||!PerfOption(cx,input,"durationThreshold",&duration))return false;
 double threshold=0;if(!duration.isUndefined()&&!PerfNumber(cx,duration,&threshold))return false;
 if(!PerfOption(cx,input,"entryTypes",&types))return false;
 uint32_t mask=0,bit=0;
 if(!types.isUndefined()){
  if(!types.isObject())return Error(cx,"PerformanceObserver entryTypes must be a sequence");
  JS::ForOfIterator iterator(cx);if(!iterator.init(types))return false;
  JS::RootedValue item(cx);bool done=false;
  while(true){if(!iterator.next(&item,&done))return false;if(done)break;
   if(!Type(cx,item,&bit))return false;mask|=bit;}
 }
 if(!PerfOption(cx,input,"type",&type)||(!type.isUndefined()&&!Type(cx,type,&bit)))return false;
 bool multiple=!types.isUndefined();
 if(multiple==!type.isUndefined())return Error(cx,"PerformanceObserver needs exactly one type option");
 if(!multiple)mask=bit;
 JS::RootedObject owner(cx,&JS::GetReservedSlot(object,OOwner).toObject());
 if(!ConfigurePerfSubscription(Subscription(object),multiple,mask))return PerfThrow(cx,owner,"InvalidModificationError");
 if(!mask)return true;
 if(!PerfConnect(cx,object,true))return false;
 if(!multiple&&JS::ToBoolean(buffered)&&(mask&((1u<<3)|(1u<<4)|(1u<<5)|(1u<<7)))){
  auto* timeline=static_cast<Timeline*>(JS::GetReservedSlot(owner,PTimeline).toPrivate());
  std::vector<uint32_t> ids;
  if(!SelectTimeline(timeline,nullptr,mask==(1u<<3)?0:mask==(1u<<7)?2:mask==(1u<<5)?3:1,false,&ids)){JS_ReportOutOfMemory(cx);return false;}
  JS::RootedObject queue(cx,&JS::GetReservedSlot(object,OQueue).toObject());
  for(auto id:ids){JS::RootedValue entry(cx),key(cx,JS::NumberValue(id));
   {JSAutoRealm realm(cx,owner);JS::RootedObject map(cx,&JS::GetReservedSlot(owner,PEntries).toObject());
    if(!JS::MapGet(cx,map,key,&entry))return false;}
   if(!PerfAppend(cx,queue,entry))return false;
  }
  if(!ids.empty()&&!PerfQueueObserverTask(cx,owner))return false;
 }
 return true;
}
}
