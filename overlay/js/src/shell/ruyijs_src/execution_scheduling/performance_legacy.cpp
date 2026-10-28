#include "execution_scheduling/performance.h"
#include "execution_scheduling/navigation_timing.h"
#include "js/Wrapper.h"
#include <string>
namespace ruyijs::execution_scheduling {
const JSClass kLegacyTimingClass={"PerformanceTiming",JSCLASS_HAS_RESERVED_SLOTS(1)};
const JSClass kLegacyNavigationClass={"PerformanceNavigation",JSCLASS_HAS_RESERVED_SLOTS(1)};
namespace {
// The document's navigation record, read off the Performance owner the legacy
// object was created from. A document that was never navigated has none, and
// every field is then 0 (Firefox reports the same for the initial about:blank).
const NavigationTiming* Record(JS::HandleObject self) {
 const JS::Value owner=JS::GetReservedSlot(self,0);
 if(!owner.isObject())return nullptr;
 auto* timing=PerfNavigationRecord(&owner.toObject());
 return timing&&timing->present?timing:nullptr;
}
// The legacy PerformanceTiming field order is kLegacyTimingFields; the value is
// an absolute epoch millisecond timestamp, so navigationStart is this
// document's origin and every other field is an offset from it.
double Field(const NavigationTiming* record,double origin,unsigned field) {
 if(!record)return 0;
 switch(field) {
  case 0: return origin;                                  // navigationStart
  case 5: return origin+record->fetch_start.value_or(record->request_start);
  case 11: return origin+record->request_start;            // requestStart
  case 12: return origin+record->response_start;           // responseStart
  case 13: return origin+record->response_end;             // responseEnd
  case 14: return origin+record->response_end;             // domLoading
  case 15: return origin+record->dom_interactive;          // domInteractive
  case 16: return origin+record->dom_content_loaded_start;
  case 17: return origin+record->dom_content_loaded_end;
  case 18: return origin+record->dom_complete;             // domComplete
  case 19: return origin+record->load_event_start;
  case 20: return origin+record->load_event_end;
  default: return 0;                                       // unload/redirect/TLS
 }
}
// PerformanceNavigation.type is the donor's nsDOMNavigationTiming::Type number
// (TYPE_NAVIGATE 0, TYPE_RELOAD 1, TYPE_BACK_FORWARD 2, TYPE_RESERVED 255) for
// the same record whose string PerformanceNavigationTiming.type reports.
int32_t LegacyType(const NavigationTiming* record) {
 if(!record)return 0;
 const std::string type=record->type;
 return type=="navigate"?0:type=="reload"?1:type=="back_forward"?2:255;
}
}
bool PerfLegacyMember(JSContext* cx,unsigned argc,JS::Value* vp) {
 auto args=JS::CallArgsFromVp(argc,vp);int selector=PerfSelector(args),kind=selector/100,field=selector%100;
 JS::RootedObject self(cx,args.thisv().isObject()?js::CheckedUnwrapStatic(&args.thisv().toObject()):nullptr);
 if(!self||JS::GetClass(self)!=(kind==LegacyTiming?&kLegacyTimingClass:&kLegacyNavigationClass))
  return Error(cx,"Illegal legacy Performance receiver");
 const NavigationTiming* record=Record(self);
 const double origin=record?record->time_origin:0;
 if(field==99){
  const auto& schema=kPerfSchema[kind];JS::RootedObject json(cx,JS_NewPlainObject(cx));
  if(!json)return false;
  for(unsigned i=0;i<schema.count;++i){
   JS::RootedValue value(cx);
   if(kind==LegacyTiming)value.setDouble(Field(record,origin,i));
   else if(i==0)value.setInt32(LegacyType(record));
   else value.setInt32(record?record->redirect_count:0);
   if(!JS_DefineProperty(cx,json,schema.fields[i],value,JSPROP_ENUMERATE))return false;
  }
  args.rval().setObject(*json);return true;
 }
 if(kind==LegacyTiming){args.rval().setDouble(Field(record,origin,field));return true;}
 if(field==0){args.rval().setInt32(LegacyType(record));return true;}
 args.rval().setInt32(record?record->redirect_count:0);return true;
}
}
