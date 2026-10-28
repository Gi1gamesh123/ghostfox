#include "resource_network/webtransport.h"
#include "js/GCPolicyAPI.h"
#include "js/Realm.h"
#include "js/SweepingAPI.h"
#include "jsfriendapi.h"

namespace ruyijs::resource_network {
namespace {
struct Entry {
  JS::Heap<JSObject*> wrapper;
  bool empty() const {return !wrapper;}
  bool traceWeak(JSTracer* tracer) {
    if(wrapper && !JS::GCPolicy<JS::Heap<JSObject*>>::traceWeak(tracer,&wrapper))wrapper.unbarrieredSet(nullptr);
    return true;
  }
};
struct Ticket {
  JS::WeakCache<Entry> entry;
  explicit Ticket(JSObject* wrapper):entry(JS::GetObjectZone(wrapper)) {entry.get().wrapper=wrapper;}
};
void Finalize(JS::GCContext*,JSObject* object) {
  const auto slot=JS::GetReservedSlot(object,0);if(!slot.isUndefined())delete static_cast<Ticket*>(slot.toPrivate());
}
const JSClassOps kOps={nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,Finalize};
const JSClass kTicketClass={"WebTransport poll ticket",JSCLASS_HAS_RESERVED_SLOTS(1)|JSCLASS_FOREGROUND_FINALIZE,&kOps};
bool Task(JSContext* cx,unsigned argc,JS::Value* vp) {
  return TransportGuard(cx,[&] {
    auto args=JS::CallArgsFromVp(argc,vp);
    JS::RootedObject ticket(cx,&js::GetFunctionNativeReserved(&args.callee(),0).toObject());
    args.rval().setUndefined();
    const auto* native=static_cast<Ticket*>(JS::GetReservedSlot(ticket,0).toPrivate());
    JS::RootedObject transport(cx,native->entry.get().wrapper.get());if(!transport)return true;
    JS::ExposeObjectToActiveJS(transport);JSAutoRealm realm(cx,transport);
    if((TransportStatus(transport)==TransportPhase::Closed && !JS::GetReservedSlot(transport,WTClosing).toBoolean()) || TransportStatus(transport)==TransportPhase::Failed)return true;
    State* state=TransportState(cx,transport);if(!state)return false;
    if(state->callbackEntry && !state->callbackEntry(state->callbackEntryContext,state->page))
      return TransportFail(cx,transport,"WebTransport document is no longer active");
    return TransportProcess(cx,transport) && TransportSchedule(cx,transport);
  });
}
}
void TransportUpdateTaskRoot(JSObject* transport) {
  JSObject* task=SlotObject(transport,WTTask);if(!task)return;
  const bool keep=TransportStatus(transport)==TransportPhase::Connecting || SlotObject(transport,WTRequests) ||
      JS::GetReservedSlot(transport,WTClosing).toBoolean();
  js::SetFunctionNativeReserved(task,1,keep?JS::ObjectValue(*transport):JS::UndefinedValue());
}
bool TransportSchedule(JSContext* cx,JS::HandleObject transport) {
  TransportUpdateTaskRoot(transport);
  if((TransportStatus(transport)==TransportPhase::Closed && !JS::GetReservedSlot(transport,WTClosing).toBoolean()) || TransportStatus(transport)==TransportPhase::Failed)return true;
  State* state=TransportState(cx,transport);if(!state)return false;
  if(!state->abortDelay)return Error(cx,"WebTransport delayed task owner is unavailable");
  JS::RootedObject task(cx,SlotObject(transport,WTTask));
  if(!task) {
    JS::RootedObject ticket(cx,JS_NewObjectWithGivenProto(cx,&kTicketClass,nullptr));if(!ticket)return false;
    auto native=std::make_unique<Ticket>(transport);JS::SetReservedSlot(ticket,0,JS::PrivateValue(native.release()));
    JS::RootedFunction function(cx,js::NewFunctionWithReserved(cx,Task,0,0,nullptr));if(!function)return false;
    task=JS_GetFunctionObject(function);js::SetFunctionNativeReserved(task,0,JS::ObjectValue(*ticket));
    JS::SetReservedSlot(transport,WTTask,JS::ObjectValue(*task));TransportUpdateTaskRoot(transport);
  }
  return state->abortDelay(state->abortDelayContext,task,1);
}
}
