#include "execution_scheduling/scheduler.h"
#include "execution_scheduling/performance.h"
#include "resource_network/abort.h"
#include "jsfriendapi.h"
#include "js/CallAndConstruct.h"
#include "js/Exception.h"
#include "js/Promise.h"
namespace ruyijs::execution_scheduling {
bool WebTaskAbort(JSContext* cx,unsigned argc,JS::Value* vp){
 auto args=JS::CallArgsFromVp(argc,vp);
 JS::RootedObject f(cx,&js::GetFunctionNativeReserved(&args.callee(),0).toObject());
 auto& s=TaskState(f);uint64_t id=uint64_t(PerfSelector(args));Callback* cb=s.Find(id);
 args.rval().setUndefined();
 if(!cb||cb->running)return true;
 JS::RootedObject promise(cx,&cb->arguments[1].toObject());
 JS::RootedValue signal(cx,cb->arguments[2]),listener(cx,cb->arguments[3]),reason(cx);bool aborted=false;
 if(!TaskAborted(cx,f,signal,&aborted,&reason)||!aborted)return !JS_IsExceptionPending(cx);
 if(!resource_network::RemoveAbortAlgorithm(cx,signal,listener))return false;
 s.model.CancelWebTask(id);s.Remove(id);return JS::RejectPromise(cx,promise,reason);
}
bool InvokeWebTask(State& s,Callback& cb,JS::MutableHandleValue result){
 JSContext* cx=s.cx;JS::RootedObject f(cx,&cb.arguments[0].toObject());JSAutoRealm realm(cx,f);
 JS::RootedObject promise(cx,&cb.arguments[1].toObject());
 JS::RootedValue signal(cx,cb.arguments[2]),listener(cx,cb.arguments[3]),handler(cx,cb.handler),value(cx);
 if(listener.isObject()&&!resource_network::RemoveAbortAlgorithm(cx,signal,listener))return false;
 bool ok=handler.isUndefined()||JS::Call(cx,JS::UndefinedHandleValue,handler,JS::HandleValueArray::empty(),&value);
 if(!ok){if(!JS_GetPendingException(cx,&value))return false;JS_ClearPendingException(cx);}
 result.setUndefined();return ok?JS::ResolvePromise(cx,promise,value):JS::RejectPromise(cx,promise,value);
}
}
