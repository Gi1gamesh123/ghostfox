#include "resource_network/native_fetch_internal.h"
#include "resource_network/state.h"
#include "resource_network/worker_network_wire.h"
#include "resource_network/worker_fetch_session.h"
#include "jsfriendapi.h"
#include "js/Realm.h"
#include <new>

namespace ruyijs::resource_network {
namespace {
namespace wire=worker_network_wire;
void Finalize(JS::GCContext*,JSObject* object) {
  auto value=JS::GetReservedSlot(object,0);
  if (!value.isUndefined()) delete static_cast<std::shared_ptr<NativeFetchRecord>*>(value.toPrivate());
}
const JSClassOps kOps={nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,Finalize};
const JSClass kWait={"Resource Worker fetch lifetime",JSCLASS_HAS_RESERVED_SLOTS(1)|JSCLASS_FOREGROUND_FINALIZE,&kOps};
bool Queue(State&,JS::HandleObject);
bool Poll(JSContext* cx,unsigned argc,JS::Value* vp) {
  auto args=JS::CallArgsFromVp(argc,vp);
  JS::RootedObject lifetime(cx,&js::GetFunctionNativeReserved(&args.callee(),0).toObject());
  auto keep=*static_cast<std::shared_ptr<NativeFetchRecord>*>(JS::GetReservedSlot(lifetime,0).toPrivate());
  if (!keep->operation) return true;
  JS::RootedObject operation(cx,keep->operation),factories(cx,SlotObject(operation,FFactories));
  State* state=StreamFactoryState(cx,factories);
  if (!state) return false;
  std::string input="FP",reply;wire::Writer writer(&input);writer.Wide(keep->workerId);
  if (!state->workerExchange(state->workerExchangeContext,input,&reply)) {
    JS_ClearPendingException(cx);return FinishNativeFetch(*state,keep,NativeFetchOutcome::NetworkFailure);
  }
  if (reply=="FP") return Queue(*state,lifetime);
  HttpResult result;bool ok=false;
  keep->workerId=0;
  if (reply.substr(0,2)!="FR" || !wire::DecodeResult(std::string_view(reply).substr(2),&ok,&result))
    return FinishNativeFetch(*state,keep,NativeFetchOutcome::NetworkFailure);
  return FinishNativeFetch(*state,keep,ok?NativeFetchOutcome::Complete:NativeFetchOutcome::NetworkFailure,&result);
}
bool Queue(State& state,JS::HandleObject lifetime) {
  JSContext* cx=state.cx;
  JS::RootedFunction callback(cx,js::NewFunctionWithReserved(cx,Poll,0,0,nullptr));
  if (!callback) return false;
  JS::RootedObject task(cx,JS_GetFunctionObject(callback));
  js::SetFunctionNativeReserved(task,0,JS::ObjectValue(*lifetime));
  return state.abortDelay?state.abortDelay(state.abortDelayContext,task,1):
      state.keepaliveTask && state.keepaliveTask(state.keepaliveTaskContext,task);
}
}
bool StartWorkerFetch(State& state,const std::shared_ptr<NativeFetchRecord>& record,
                       const RequestDraft& request,const HttpOptions& options,RedirectMode redirect) {
  std::string input,reply;
  if (!worker_network_wire::EncodeRequest({request.method,request.url,request.body,request.headers,redirect,options},&input))
    return Error(state.cx,"Worker fetch request exceeds channel limits");
  input.replace(0,1,"FS");
  if (!state.workerExchange(state.workerExchangeContext,input,&reply) || reply.substr(0,2)!="FO")
    return Error(state.cx,"Worker fetch session is unavailable");
  worker_network_wire::Reader reader(std::string_view(reply).substr(2));
  if (!reader.Wide(&record->workerId) || !reader.Done()) return Error(state.cx,"Invalid Worker fetch admission");
  auto* keep=new(std::nothrow) std::shared_ptr<NativeFetchRecord>(record);
  if (!keep) {JS_ReportOutOfMemory(state.cx);return false;}
  JS::RootedObject lifetime(state.cx,JS_NewObjectWithGivenProto(state.cx,&kWait,nullptr));
  if (!lifetime) {delete keep;return false;}
  JS::SetReservedSlot(lifetime,0,JS::PrivateValue(keep));
  return Queue(state,lifetime);
}
void CancelNativeFetchTransport(State& state,NativeFetchRecord& record) {
  CancelAsyncHttp(record.source,record.ticket);
  if (!record.workerId || !state.workerExchange) return;
  std::string input="FC",reply;worker_network_wire::Writer writer(&input);writer.Wide(record.workerId);
  record.workerId=0;
  JS::AutoSaveExceptionState saved(state.cx);
  if (!state.workerExchange(state.workerExchangeContext,input,&reply)) JS_ClearPendingException(state.cx);
  saved.restore();
}
std::shared_ptr<WorkerFetchSession> Realm::OpenWorkerFetchSession() {
  std::shared_ptr<WorkerFetchSession> result;
  if (!state_ || !state_->asyncSource || !Report(state_->cx,NewWorkerFetchSession(state_->asyncSource,&result))) return {};
  return result;
}
}
