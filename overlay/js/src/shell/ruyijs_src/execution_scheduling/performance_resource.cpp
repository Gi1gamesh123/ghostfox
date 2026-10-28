#include "execution_scheduling/performance.h"
#include "execution_scheduling/resource_timing.h"
#include "execution_scheduling/navigation_timing.h"
#include "execution_scheduling/performance_observer.h"
#include "js/Array.h"
#include "js/CharacterEncoding.h"
#include <cmath>
#include <memory>
#include <new>
namespace ruyijs::execution_scheduling {
namespace {
bool Text(JSContext* cx,JS::HandleObject data,uint32_t index,const std::string& text){
 JS::RootedString value(cx,JS_NewStringCopyUTF8N(cx,JS::UTF8Chars(text.data(),text.size())));
 return value&&JS_SetElement(cx,data,index,value);
}
bool String(JSContext* cx,const std::string& text,JS::MutableHandleValue out){
 JS::RootedString value(cx,JS_NewStringCopyUTF8N(cx,JS::UTF8Chars(text.data(),text.size())));
 if(!value)return false;out.setString(value);return true;
}
// A navigation entry's live record: its detail holds the Performance owner.
const NavigationTiming* NavigationRecord(JSContext* cx,JS::HandleObject entry){
 JS::RootedObject data(cx,&JS::GetReservedSlot(entry,EDetail).toObject());
 JS::RootedValue owner(cx);
 if(!JS_GetElement(cx,data,0,&owner)||!owner.isObject())return nullptr;
 return PerfNavigationRecord(&owner.toObject());
}
// The inherited PerformanceResourceTiming members of the navigation entry, in
// kPerfSchema[Resource] order. Only measured values are reported; DNS/connect/
// TLS stay 0 because the transport exports no per-hop measurement and a value
// must never be copied from fetchStart (runtime/resource_timing_ports.cpp).
bool NavigationResource(JSContext* cx,const NavigationTiming& r,unsigned field,JS::MutableHandleValue out){
 switch(field){
  case 0: return String(cx,"navigation",out);                 // initiatorType
  case 1: return String(cx,r.protocol,out);                    // nextHopProtocol
  case 5: out.setDouble(r.fetch_start.value_or(r.request_start));return true;
  case 11: out.setDouble(r.request_start);return true;
  case 14: out.setDouble(r.response_start);return true;
  case 15: out.setDouble(r.response_end);return true;
  case 16: out.setDouble(r.transfer_size?double(*r.transfer_size):r.encoded_size?double(r.encoded_size+300):0);return true;
  case 17: out.setDouble(double(r.encoded_size));return true;
  case 18: out.setDouble(double(r.decoded_size));return true;
  case 19: out.setInt32(r.status);return true;
  case 20: return String(cx,r.content_type,out);
  case 21:{JS::RootedObject list(cx,JS::NewArrayObject(cx,0));
   if(!list||!JS_FreezeObject(cx,list))return false;out.setObject(*list);return true;}
  case 22: return String(cx,"",out);
  default: out.setDouble(0);return true;
 }
}
}
bool PerfNavigationField(JSContext* cx,JS::HandleObject entry,unsigned field,JS::MutableHandleValue out){
 JSAutoRealm realm(cx,entry);
 const NavigationTiming* r=NavigationRecord(cx,entry);
 if(!r){out.setDouble(0);return true;}
 const double values[]={0,0,r->dom_interactive,r->dom_content_loaded_start,r->dom_content_loaded_end,
  r->dom_complete,r->load_event_start,r->load_event_end};
 if(field<8){out.setDouble(values[field]);return true;}
 if(field==8)return String(cx,r->type,out);
 out.setInt32(r->redirect_count);return true;
}
bool PerfResourceField(JSContext* cx,JS::HandleObject entry,unsigned field,JS::MutableHandleValue out){
 JSAutoRealm realm(cx,entry);
 if(JS::GetReservedSlot(entry,EKind).toInt32()==NavigationEntry){
  const NavigationTiming* r=NavigationRecord(cx,entry);
  if(!r){out.setDouble(0);return true;}
  return NavigationResource(cx,*r,field,out);
 }
 JS::RootedObject data(cx,&JS::GetReservedSlot(entry,EDetail).toObject());
 return JS_GetElement(cx,data,field,out);
}
bool Realm::RecordResource(JS::HandleObject page,const ResourceTimingRecord& record){
 auto& s=*state_;JSContext* cx=s.cx;JS::RootedObject owner(cx);
 for(const auto& candidate:s.performanceObjects){
  if(&JS::GetReservedSlot(candidate,PPage).toObject()==page){owner=candidate;break;}
 }
 if(!owner)return Error(cx,"Resource timing realm is not installed");
 JSAutoRealm realm(cx,owner);
 JS::RootedObject data(cx,JS::NewArrayObject(cx,23)),server(cx,JS::NewArrayObject(cx,0)),entry(cx);
 JS::RootedString name(cx,JS_NewStringCopyUTF8N(cx,JS::UTF8Chars(record.name.data(),record.name.size())));
 if(!data||!server||!name||!JS_FreezeObject(cx,server)||!Text(cx,data,0,record.initiator)||
    !Text(cx,data,1,record.protocol)||!Text(cx,data,20,record.contentType)||
    !Text(cx,data,22,record.deliveryType)||!JS_SetElement(cx,data,21,server))return false;
 const double numbers[]={record.workerStart,record.redirectStart,record.redirectEnd,record.fetchStart,
  record.domainLookupStart,record.domainLookupEnd,record.connectStart,record.connectEnd,
  record.secureConnectionStart,record.requestStart,record.finalResponseHeadersStart,
  record.firstInterimResponseStart,record.responseStart,record.responseEnd,double(record.transferSize),
  double(record.encodedBodySize),double(record.decodedBodySize),double(record.responseStatus)};
 for(unsigned i=0;i<18;++i)if(!JS_SetElement(cx,data,i+2,numbers[i]))return false;
 JS::RootedValue detail(cx,JS::ObjectValue(*data));
 return PerfEntry(cx,owner,Resource,name,record.start,record.responseEnd-record.start,detail,nullptr,&entry)&&
   PerfRecord(cx,owner,entry);
}
// This document's navigation (docs/owners/NAVIGATION_TIMING.md). The record is
// kept on the Performance owner and the one navigation entry points at it, so
// performance.timeOrigin, performance.timing.*, the entry's own members and its
// inherited resource members all read the same live values, and a milestone the
// load path stamps later is visible immediately.
bool Realm::RecordNavigation(JS::HandleObject page,const NavigationTiming& timing){
 auto& s=*state_;JSContext* cx=s.cx;JS::RootedObject owner(cx);
 owner=PerformanceOwner(page);
 if(!owner)return Error(cx,"Navigation timing realm is not installed");
 JSAutoRealm realm(cx,owner);
 // Starting a document replaces any previous record of this realm, so a retired
 // document never reads the next document's values.
 std::unique_ptr<NavigationTiming> stored(new (std::nothrow) NavigationTiming(timing));
 if(!stored)return Error(cx,"Navigation timing allocation failed");
 // Firefox reduces timer precision to 1 ms outside cross-origin isolation, so
 // the origin and every offset are whole milliseconds.
 stored->time_origin=std::floor(stored->time_origin);
 for(double* field:{&stored->request_start,&stored->response_start,&stored->response_end})
  *field=std::floor(*field);
 size_t replayIndex=0;
 if(!s.ApplyNavigationReplay(*stored,&replayIndex))return false;
 auto* previous=PerfNavigationRecord(owner);
 JS::SetReservedSlot(owner,PNavTiming,JS::PrivateValue(stored.release()));
 delete previous;
 JS::RootedObject detail(cx,JS::NewArrayObject(cx,1));
 JS::RootedString name(cx,JS_NewStringCopyUTF8N(cx,
     JS::UTF8Chars(timing.name.data(),timing.name.size())));
 JS::RootedObject entry(cx);
 JS::RootedValue ownerValue(cx,JS::ObjectValue(*owner));
 JS::RootedValue detailValue(cx,detail?JS::ObjectValue(*detail):JS::UndefinedValue());
 // startTime is 0: every member of a navigation entry is an offset from this
 // document's own origin, which is exactly what its time origin is.
 bool success=detail&&JS_SetElement(cx,detail,0,ownerValue)&&name&&
   PerfEntry(cx,owner,NavigationEntry,name,0,timing.load_event_end,detailValue,nullptr,&entry)&&
   PerfRecord(cx,owner,entry);
 if(success&&replayIndex<s.navigationReplay.size())s.navigationReplay[replayIndex].bound=true;
 return success;
}
bool Realm::MarkNavigation(JS::HandleObject page,int milestone){
 auto& s=*state_;JSContext* cx=s.cx;
 JS::RootedObject owner(cx,PerformanceOwner(page));
 if(!owner)return Error(cx,"Navigation timing realm is not installed");
 auto* record=PerfNavigationRecord(owner);
 if(!record)return true;
 s.SyncClock();
 const auto clock=s.model.Inspect();
 const double offset=std::floor(clock.epoch+clock.now-record->time_origin);
 switch(milestone) {
  case 0: record->dom_interactive=offset;break;
  case 1: record->dom_content_loaded_start=offset;break;
  case 2: record->dom_content_loaded_end=offset;record->dom_complete=offset;break;
  case 3: record->load_event_start=offset;break;
  case 4:
   if(record->load_event_finished)return true;
   record->load_event_end=record->recorded_load_event_end.value_or(offset);
   return PerfCompleteNavigation(cx,owner);
  default: return Error(cx,"Unknown navigation milestone");
 }
 return true;
}
// The one Performance owner of a page, or null when this realm has none.
// Web Animations document timeline time (docs/owners/WEB_ANIMATION.md): the same
// origin as performance.now(), unrounded, from the last synced clock so it holds
// still within one task like Firefox's refresh-driver tick. NaN without a Performance owner.
double Realm::DocumentNow(JS::HandleObject page){
 JSObject* raw=PerformanceOwner(page);if(!raw)return std::nan("");
 JS::RootedObject owner(state_->cx,raw);const auto snapshot=state_->model.Inspect();
 return snapshot.epoch+snapshot.now-PerfTimeOrigin(owner);
}
JSObject* Realm::PerformanceOwner(JS::HandleObject page){
 auto& s=*state_;
 for(const auto& candidate:s.performanceObjects)
  if(&JS::GetReservedSlot(candidate,PPage).toObject()==page)return candidate;
 return nullptr;
}
bool Realm::HasNavigation(JS::HandleObject page){
 auto& s=*state_;
 JSObject* owner=PerformanceOwner(page);
 if(!owner)return true;  // no Performance surface: nothing to record into
 auto* record=PerfNavigationRecord(owner);
 return record&&record->present;
}
}
