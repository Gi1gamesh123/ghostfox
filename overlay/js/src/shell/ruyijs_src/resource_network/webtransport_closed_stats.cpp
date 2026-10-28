#include "resource_network/webtransport.h"
#include "js/Realm.h"
#include "jsfriendapi.h"

namespace ruyijs::resource_network {
namespace {
bool ResolveStats(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject promise(cx,&js::GetFunctionNativeReserved(&args.callee(),0).toObject());
  JS::RootedValue value(cx,js::GetFunctionNativeReserved(&args.callee(),1));
  args.rval().setUndefined();return TransportResolve(cx,promise,value);
}
}
bool TransportClosedStats(JSContext* cx,JS::HandleObject transport,JS::HandleObject promise) {
  State* state=TransportState(cx,transport);if(!state)return false;
  const auto* stats=TransportFinalStats(transport);JS::RootedValue value(cx);
  if(!stats)return TransportDomError(*state,"InvalidStateError","No stats available for closed connection",&value) && TransportReject(cx,promise,value);
  if(!TransportStatsObject(cx,*stats,false,false,&value))return false;
  JS::RootedFunction function(cx,js::NewFunctionWithReserved(cx,ResolveStats,0,0,nullptr));if(!function)return false;
  JS::RootedObject task(cx,JS_GetFunctionObject(function));
  js::SetFunctionNativeReserved(task,0,JS::ObjectValue(*promise));js::SetFunctionNativeReserved(task,1,value);
  return state->abortDelay && state->abortDelay(state->abortDelayContext,task,0);
}
}
